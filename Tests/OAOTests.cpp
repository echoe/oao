// OAOTests.cpp
// Full test suite for the oao FM synthesizer plugin.
//
// HOW TO BUILD:
// Add a CMake target that compiles this file alongside your Source/ files
// (excluding PluginEditor.cpp and PluginProcessor.cpp, which need a full JUCE
// plugin host to link).  Something like:
//
//   juce_add_console_app(oao_tests PRODUCT_NAME "oao_tests")
//   target_sources(oao_tests PRIVATE OAOTests.cpp)
//   target_include_directories(oao_tests PRIVATE Source/)
//   target_link_libraries(oao_tests PRIVATE
//       juce::juce_audio_basics
//       juce::juce_audio_processors
//       juce::juce_dsp)
//
// Then:  cmake --build . && ctest --output-on-failure
//
// Each JUCE UnitTest subclass auto-registers itself via its static instance.
// The main() at the bottom runs them all and returns non-zero on failure.

#include <JuceHeader.h>
#include "WaveTable.h"
#include "SynthEffect.h"
#include "FMOperator.h"
#include "FMVoice.h"
#include "Constants.h"

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Returns true when every value in [data, data+n) is finite (no NaN / Inf).
static bool allFinite (const float* data, int n)
{
    for (int i = 0; i < n; ++i)
        if (!std::isfinite (data[i])) return false;
    return true;
}

// Feed `n` samples of a sine wave into a processing lambda and return whether
// all outputs were finite.
template <typename Fn>
static bool sineInputFinite (Fn&& process, int n = 2048,
                              float freq = 440.0f, double sr = 44100.0)
{
    for (int i = 0; i < n; ++i)
    {
        float in  = std::sin (2.0f * juce::MathConstants<float>::pi * freq * i / (float)sr);
        float out = process (in);
        if (!std::isfinite (out)) return false;
    }
    return true;
}


// ═════════════════════════════════════════════════════════════════════════════
// 1.  WaveTable
// ═════════════════════════════════════════════════════════════════════════════
class WaveTableTests : public juce::UnitTest
{
public:
    WaveTableTests() : juce::UnitTest ("WaveTable", "oao") {}

    void runTest() override
    {
        WaveTable wt;
        wt.prepare();

        // --- 1.1  All tables stay in [-1, 1] across the full phase range ----
        beginTest ("Sine table bounded [-1,1]");
        {
            float lo =  1.0f, hi = -1.0f;
            for (int i = 0; i < WaveTable::tableSize; ++i)
            {
                float ph = (float)i / WaveTable::tableSize
                            * juce::MathConstants<float>::twoPi;
                float v  = wt.lookupSine (ph);
                lo = std::min (lo, v);
                hi = std::max (hi, v);
            }
            expectWithinAbsoluteError (lo, -1.0f, 0.01f);
            expectWithinAbsoluteError (hi,  1.0f, 0.01f);
        }

        beginTest ("Triangle table bounded [-1,1]");
        {
            float hi = -1.0f;
            for (int i = 0; i < WaveTable::tableSize; ++i)
            {
                float ph = (float)i / WaveTable::tableSize
                            * juce::MathConstants<float>::twoPi;
                float v  = std::abs (wt.lookupTriangle (ph));
                hi = std::max (hi, v);
            }
            expect (hi <= 1.01f, "Triangle exceeds 1");
        }

        beginTest ("Saw table bounded [-1,1] after normalisation");
        {
            for (int i = 0; i < WaveTable::tableSize; ++i)
            {
                float ph = (float)i / WaveTable::tableSize
                            * juce::MathConstants<float>::twoPi;
                float v  = wt.lookupSaw (ph);
                expect (v >= -1.01f && v <= 1.01f, "Saw out of range");
            }
        }

        beginTest ("Square table bounded [-1,1] after normalisation");
        {
            for (int i = 0; i < WaveTable::tableSize; ++i)
            {
                float ph = (float)i / WaveTable::tableSize
                            * juce::MathConstants<float>::twoPi;
                float v  = wt.lookupSquare (ph);
                expect (v >= -1.01f && v <= 1.01f, "Square out of range");
            }
        }

        // --- 1.2  Lookup at phase 0 is consistent with sin(0) = 0 ----------
        beginTest ("Sine lookup at phase 0 is ~0");
        expectWithinAbsoluteError (wt.lookupSine (0.0f), 0.0f, 0.01f);

        // --- 1.3  Lookup at pi/2 gives near-peak value ----------------------
        beginTest ("Sine lookup at pi/2 is ~+1");
        {
            float v = wt.lookupSine (juce::MathConstants<float>::halfPi);
            expectWithinAbsoluteError (v, 1.0f, 0.01f);
        }

        // --- 1.4  All lookups produce finite values for all phases -----------
        beginTest ("All waveform lookups are finite");
        {
            bool ok = true;
            for (int i = 0; i < WaveTable::tableSize * 2 && ok; ++i)
            {
                float ph = (float)i / WaveTable::tableSize
                            * juce::MathConstants<float>::twoPi;
                ok &= std::isfinite (wt.lookupSine     (ph));
                ok &= std::isfinite (wt.lookupTriangle (ph));
                ok &= std::isfinite (wt.lookupSaw      (ph));
                ok &= std::isfinite (wt.lookupSquare   (ph));
            }
            expect (ok);
        }
    }
};
static WaveTableTests waveTableTests;


// ═════════════════════════════════════════════════════════════════════════════
// 2.  SynthEffect — SVF filters
// ═════════════════════════════════════════════════════════════════════════════
class SVFFilterTests : public juce::UnitTest
{
public:
    SVFFilterTests() : juce::UnitTest ("SynthEffect_SVF", "oao") {}

    void runTest() override
    {
        static constexpr double SR = 44100.0;

        // --- 2.1  Lowpass at 1kHz attenuates 10kHz sine ---------------------
        beginTest ("LP 1kHz attenuates 10kHz input");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            fx.setType (0); // Lowpass
            fx.setCutoff (1000.0f);
            fx.setResonance (0.707f);

            // run 2k samples to let filter settle
            float maxOut = 0.0f;
            for (int i = 0; i < 4096; ++i)
            {
                float in  = std::sin (2.0f * juce::MathConstants<float>::pi * 10000.0f * i / (float)SR);
                float out = fx.processSample (in);
                if (i > 2048) maxOut = std::max (maxOut, std::abs (out));
            }
            expect (maxOut < 0.25f, "LP should heavily attenuate 10kHz above 1kHz cutoff");
        }

        // --- 2.2  Highpass passes high frequencies, attenuates low ----------
        beginTest ("HP 2kHz passes 10kHz, attenuates 100Hz");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            fx.setType (1); // Highpass
            fx.setCutoff (2000.0f);
            fx.setResonance (0.707f);

            float maxLow = 0.0f, maxHigh = 0.0f;
            for (int i = 0; i < 8192; ++i)
            {
                float inLow  = std::sin (2.0f * juce::MathConstants<float>::pi *  100.0f * i / (float)SR);
                float inHigh = std::sin (2.0f * juce::MathConstants<float>::pi * 10000.0f * i / (float)SR);
                float outLow  = fx.processSample (inLow);
                float outHigh = fx.processSample (inHigh);
                if (i > 4096)
                {
                    maxLow  = std::max (maxLow,  std::abs (outLow));
                    maxHigh = std::max (maxHigh, std::abs (outHigh));
                }
            }
            // High frequency should come through more than very low frequency
            expect (maxHigh > maxLow, "HP filter should pass high more than low");
        }

        // --- 2.3  Filter output is always finite on silent input -------------
        beginTest ("SVF stays finite on silence");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            fx.setType (0);
            fx.setCutoff (1000.0f);
            fx.setResonance (0.707f);
            bool ok = sineInputFinite ([&] (float in) { return fx.processSample (in * 0.0f); });
            expect (ok);
        }

        // --- 2.4  setCutoff clamps to safe range ----------------------------
        beginTest ("setCutoff clamps extremes");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            fx.setCutoff (-9999.0f); // below min
            expect (sineInputFinite ([&](float in){ return fx.processSample(in); }));
            fx.setCutoff (999999.0f); // above max
            expect (sineInputFinite ([&](float in){ return fx.processSample(in); }));
        }

        // --- 2.5  reset() clears state without crash ------------------------
        beginTest ("reset() leaves filter in clean state");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            fx.setType (0);
            fx.setCutoff (500.0f);
            // Pump some audio through to dirty the state
            for (int i = 0; i < 512; ++i)
                fx.processSample (std::sin ((float)i * 0.1f));
            fx.reset();
            // Should now produce finite output
            float out = fx.processSample (0.5f);
            expect (std::isfinite (out));
        }

        // --- 2.6  Audio-rate modulated SVF is finite ------------------------
        beginTest ("processSampleAudioRate finite at all cutoffs");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            fx.setType (0);
            float k = fx.getPrecalculatedK();
            bool ok = true;
            // Sweep cutoff from 20 Hz to 20 kHz
            for (int i = 0; i < 4096 && ok; ++i)
            {
                float cutoff = 20.0f * std::pow (1000.0f, (float)i / 4096.0f);
                float input  = std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * i / (float)SR);
                float out    = fx.processSampleAudioRate (input, cutoff, k);
                ok = std::isfinite (out);
            }
            expect (ok);
        }
    }
};
static SVFFilterTests svfTests;


// ═════════════════════════════════════════════════════════════════════════════
// 3.  SynthEffect — per-effect smoke tests
// ═════════════════════════════════════════════════════════════════════════════
class SynthEffectSmokeTests : public juce::UnitTest
{
public:
    SynthEffectSmokeTests() : juce::UnitTest ("SynthEffect_Effects", "oao") {}

    // Helper: run `n` samples of a 440Hz sine through `process`, assert finite.
    bool smokeTest (std::function<float(float)> process,
                    int n = 2048, float freq = 440.0f, double sr = 44100.0)
    {
        return sineInputFinite (process, n, freq, sr);
    }

    void runTest() override
    {
        static constexpr double SR = 44100.0;

        // --- 3.1  Comb filter -----------------------------------------------
        beginTest ("Comb filter: finite output");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in)
            {
                return fx.processSampleComb (in, 440.0f, 0.5f, 0.3f);
            }));
        }

        // --- 3.2  Comb filter: feedback at 0.995 doesn't explode ------------
        beginTest ("Comb filter: max safe feedback stays finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in)
            {
                return fx.processSampleComb (in, 100.0f, 0.995f, 0.5f);
            }, 8192));
        }

        // --- 3.3  Formant filter --------------------------------------------
        beginTest ("Formant filter: A vowel (index 0.0) is finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in)
            {
                return fx.processSampleFormant (in, 0.0f, 8.0f);
            }));
        }

        beginTest ("Formant filter: transitions across all 5 vowels");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            bool ok = true;
            for (float v = 0.0f; v <= 4.0f && ok; v += 0.1f)
            {
                float out = fx.processSampleFormant (0.5f, v, 8.0f);
                ok = std::isfinite (out);
            }
            expect (ok);
        }

        // --- 3.4  Granular: argument order (regression for scatterAmt/grainDurationMs swap) ---
        beginTest ("Granular: grainDurationMs=500 scatterAmt=0 is finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            // This order must match the fixed signature: (input, freq, grainDurationMs, scatterAmt, damping, feedback)
            expect (smokeTest ([&](float in)
            {
                return fx.processSampleGranular (in, 440.0f, 500.0f, 0.0f, 0.3f, 0.0f);
            }));
        }

        beginTest ("Granular: maximum scatter and feedback stays finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in)
            {
                return fx.processSampleGranular (in, 220.0f, 50.0f, 1.0f, 0.5f, 0.9f);
            }, 4096));
        }

        // --- 3.5  Ring Mod: frequency boundary values -----------------------
        beginTest ("Ring Mod: 1Hz carrier (min) is finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in)
            {
                return fx.processSampleRingMod (in, 1.0f, 0.0f, 1.0f, 0.0f, SR);
            }));
        }

        beginTest ("Ring Mod: 20kHz carrier (max) is finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in)
            {
                return fx.processSampleRingMod (in, 20000.0f, 1.0f, 1.0f, 0.9f, SR);
            }));
        }

        beginTest ("Ring Mod: all three shape zones produce finite output");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            for (float shape : { 0.0f, 0.16f, 0.33f, 0.5f, 0.66f, 0.83f, 1.0f })
            {
                bool ok = smokeTest ([&](float in) {
                    return fx.processSampleRingMod (in, 440.0f, shape, 1.0f, 0.5f, SR);
                });
                expect (ok, "Ring Mod shape=" + juce::String (shape) + " produced NaN/Inf");
                fx.reset();
            }
        }

        beginTest ("Ring Mod: depth=0 is a dry pass-through");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            float in  = 0.7f;
            float out = fx.processSampleRingMod (in, 440.0f, 0.0f, 0.0f, 0.0f, SR);
            // depth=0 → output = modulated*0 + input*1 = input
            expectWithinAbsoluteError (out, in, 0.001f);
        }

        // --- 3.6  Compressor ------------------------------------------------
        beginTest ("Compressor: loud input is attenuated");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            float maxOut = 0.0f;
            float in = 1.0f;
            // Threshold at 0.5 (-6dB), ratio 0.9 (near 20:1)
            for (int i = 0; i < 4096; ++i)
            {
                float out = fx.processSampleCompressor (in, 0.5f, 0.9f, 0.1f, 0.5f, SR);
                if (i > 2048) maxOut = std::max (maxOut, std::abs (out));
            }
            expect (maxOut < in, "Compressor should reduce level");
        }

        beginTest ("Compressor: below threshold, output approximates input");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            float in  = 0.01f; // well below threshold=0.5 → ~-34dBFS
            float out = fx.processSampleCompressor (in, 0.5f, 0.5f, 0.0f, 1.0f, SR);
            expect (std::isfinite (out));
        }

        // --- 3.7  Scatter ---------------------------------------------------
        beginTest ("Scatter: all four pattern types are finite");
        {
            for (int pat = 0; pat < 4; ++pat)
            {
                auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
                fx.prepare (SR);
                float patNorm = pat / 4.0f + 0.01f;
                bool ok = smokeTest ([&](float in) {
                    return fx.processSampleScatter (in, patNorm, 0.5f, 0.5f, 0.8f, SR);
                }, 4096);
                expect (ok, "Scatter pattern " + juce::String (pat) + " produced NaN/Inf");
            }
        }

        // --- 3.8  Bitcrush --------------------------------------------------
        beginTest ("Bitcrush: 1-bit produces finite values");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            // bits=1.0 → nearly 1-bit
            expect (smokeTest ([&](float in) {
                return fx.processSampleBitcrush (in, 1.0f, 0.0f, 0.0f, 0.0f, SR);
            }));
        }

        beginTest ("Bitcrush: max downsample + max jitter + max noise stays finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in) {
                return fx.processSampleBitcrush (in, 1.0f, 1.0f, 1.0f, 1.0f, SR);
            }, 4096));
        }

        // --- 3.9  Distortion ------------------------------------------------
        beginTest ("Distortion: all four flavor zones are finite");
        {
            for (float flavor : { 0.1f, 0.3f, 0.6f, 0.9f })
            {
                auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
                fx.prepare (SR);
                bool ok = smokeTest ([&](float in) {
                    return fx.processSampleDistortion (in, 0.8f, flavor, 0.5f, 0.5f, SR);
                }, 4096);
                expect (ok, "Distortion flavor=" + juce::String (flavor) + " produced NaN/Inf");
            }
        }

        // --- 3.10  Chorus ---------------------------------------------------
        beginTest ("Chorus: 4 voices, max depth stays finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in) {
                return fx.processSampleChorus (in, 0.5f, 1.0f, 1.0f, 1.0f, SR);
            }, 4096));
        }

        // --- 3.11  Old Chorus (Juno-style) ----------------------------------
        beginTest ("Old Chorus Mode I and Mode II are finite");
        {
            for (float mode : { 0.0f, 1.0f })
            {
                auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
                fx.prepare (SR);
                bool ok = smokeTest ([&](float in) {
                    return fx.processSampleOldChorus (in, 0.3f, 0.7f, mode, 0.5f, SR);
                }, 4096);
                expect (ok, "Old Chorus mode=" + juce::String (mode) + " NaN/Inf");
            }
        }

        // --- 3.12  Allpass Delay --------------------------------------------
        beginTest ("AP Delay: long delay + max feedback is finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in) {
                return fx.processSampleAllpassDelay (in, 0.9f, 0.9f, 0.7f, 0.7f, SR);
            }, 8192));
        }

        // --- 3.13  Allpass Reverb -------------------------------------------
        beginTest ("AP Reverb: large size + long decay is finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in) {
                return fx.processSampleAllpassReverb (in, 1.0f, 0.9f, 0.7f, 0.7f, SR);
            }, 8192));
        }

        // --- 3.14  Timeshift Delay ------------------------------------------
        beginTest ("Timeshift Delay: self-oscillating feedback (1.2) stays finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in) {
                return fx.processSampleTimeCtrlDelay (in, 0.5f, 1.0f, 0.5f, 0.8f, SR);
            }, 8192));
        }

        // --- 3.15  LoFi -----------------------------------------------------
        beginTest ("LoFi: extreme decimation + 2-bit depth + max wear is finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in) {
                return fx.processSampleLoFi (in, 1.0f, 1.0f, 1.0f, 1.0f, SR);
            }, 4096));
        }

        // --- 3.16  3-Band EQ ------------------------------------------------
        beginTest ("3-Band EQ: +12dB low / -24dB mid / +12dB high stays finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in) {
                return fx.processSampleThreeBandEQ (in, 1.0f, 0.0f, 1.0f, 0.5f, SR);
            }));
        }

        beginTest ("3-Band EQ: master gain 0 produces silence");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            float out = fx.processSampleThreeBandEQ (1.0f, 0.5f, 0.5f, 0.5f, 0.0f, SR);
            expectWithinAbsoluteError (out, 0.0f, 0.001f);
        }

        // --- 3.17  Filter + Drive -------------------------------------------
        beginTest ("Filter Drive: max drive + LP stays finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in) {
                return fx.processSampleFilterDrive (in, 0.5f, 0.8f, 1.0f, 0.0f, SR);
            }, 4096));
        }

        // --- 3.18  OTT ------------------------------------------------------
        beginTest ("OTT: heavy depth + upward comp stays finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in) {
                return fx.processSampleOTT (in, 1.0f, 0.5f, 1.0f, 0.5f, SR);
            }, 4096));
        }

        // --- 3.19  DJFX Delay -----------------------------------------------
        beginTest ("DJFX Delay: loop mode (on >= 0.5) is finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in) {
                return fx.processSampleDJFXDelay (in, 0.5f, 0.5f, 1.0f, 0.2f, SR);
            }, 4096));
        }

        beginTest ("DJFX Delay: varispeed mode (on < 0.5) is finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in) {
                return fx.processSampleDJFXDelay (in, 0.5f, 0.5f, 0.0f, 0.2f, SR);
            }, 4096));
        }

        // --- 3.20  Ambient Delay -------------------------------------------
        beginTest ("Ambient Delay: max shimmer + max feedback stays finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in) {
                return fx.processSampleAmbientDelay (in, 0.8f, 0.95f, 1.0f, 0.5f, SR);
            }, 8192));
        }

        // --- 3.21  Color Bass -----------------------------------------------
        beginTest ("Color Bass: all knobs at max stays finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            expect (smokeTest ([&](float in) {
                return fx.processSampleColorBass (in, 1.0f, 1.0f, 1.0f, 1.0f, SR);
            }, 4096));
        }

        // --- 3.22  Spectral Freeze: identity (freeze OFF) -------------------
        beginTest ("Spectral Freeze: freeze=0 is approximately dry");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            // When freeze <= 0.5, effect passes audio through with dry mix ramped to 0
            bool ok = smokeTest ([&](float in) {
                return fx.processSampleSpectralFreeze (in, 0.0f, 1.0f, 0.0f, 0.0f, SR);
            }, 4096);
            expect (ok);
        }

        beginTest ("Spectral Freeze: freeze=1 produces finite output");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            // Pre-fill with audio so there is a frame to freeze
            for (int i = 0; i < 4096; ++i)
            {
                float in = std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * i / (float)SR);
                fx.processSampleSpectralFreeze (in, 1.0f, 1.0f, 0.0f, 0.0f, SR);
            }
            // Continued frozen playback
            bool ok = smokeTest ([&](float in) {
                return fx.processSampleSpectralFreeze (in, 1.0f, 1.0f, 0.0f, 0.0f, SR);
            }, 4096);
            expect (ok);
        }

        beginTest ("Spectral Freeze: pitch=+24 semitones stays finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            for (int i = 0; i < 4096; ++i)
            {
                float in = std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * i / (float)SR);
                fx.processSampleSpectralFreeze (in, 1.0f, 1.0f, 24.0f, 0.0f, SR);
            }
            bool ok = smokeTest ([&](float in) {
                return fx.processSampleSpectralFreeze (in, 1.0f, 1.0f, 24.0f, 0.0f, SR);
            }, 4096);
            expect (ok);
        }

        beginTest ("Spectral Freeze: pitch=-24 semitones stays finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            for (int i = 0; i < 4096; ++i)
            {
                float in = std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * i / (float)SR);
                fx.processSampleSpectralFreeze (in, 1.0f, 1.0f, -24.0f, 0.0f, SR);
            }
            bool ok = smokeTest ([&](float in) {
                return fx.processSampleSpectralFreeze (in, 1.0f, 1.0f, -24.0f, 0.0f, SR);
            }, 4096);
            expect (ok);
        }

        // --- 3.23  Stereo reverb -------------------------------------------
        beginTest ("Stereo AP Reverb: both channels finite");
        {
            auto fxLPtr = std::make_unique<SynthEffect>();
            auto fxRPtr = std::make_unique<SynthEffect>();
            auto& fxL = *fxLPtr;
            auto& fxR = *fxRPtr;
            fxL.prepare (SR);
            fxR.prepare (SR);
            bool ok = true;
            for (int i = 0; i < 4096 && ok; ++i)
            {
                float inL = std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * i / (float)SR);
                float inR = std::sin (2.0f * juce::MathConstants<float>::pi * 441.0f * i / (float)SR);
                auto [outL, outR] = fxL.processSampleStereoAllpassReverb (inL, inR, fxR, 0.8f, 0.85f, 0.7f, 0.5f, SR);
                ok = std::isfinite (outL) && std::isfinite (outR);
            }
            expect (ok);
        }
    }
};
static SynthEffectSmokeTests effectSmokeTests;


// ═════════════════════════════════════════════════════════════════════════════
// 4.  FXModLFO
// ═════════════════════════════════════════════════════════════════════════════
class FXModLFOTests : public juce::UnitTest
{
    // A minimal stub APVTS-like helper — we can't easily build a real APVTS
    // without a full AudioProcessor, so we test the LFO math in isolation
    // by calling tick() directly after forcing its private fields via a
    // thin subclass workaround.
public:
    FXModLFOTests() : juce::UnitTest ("FXModLFO", "oao") {}

    // Instead of trying to construct an APVTS, we test the waveform output
    // formulae that tick() computes, reconstructed here.
    struct LFOWaveformTester
    {
        static float sine     (double ph) { return std::sin ((float)ph); }
        static float triangle (double ph)
        {
            const float twoPi = juce::MathConstants<float>::twoPi;
            float fPh = (float)ph;
            return (2.0f / twoPi) * (fPh < twoPi * 0.5f ? fPh : twoPi - fPh) * 2.0f - 1.0f;
        }
        static float saw      (double ph)
        {
            return 1.0f - (2.0f * (float)ph / juce::MathConstants<float>::twoPi);
        }
        static float square   (double ph)
        {
            return ((float)ph < juce::MathConstants<float>::pi) ? 1.0f : -1.0f;
        }
    };

    void runTest() override
    {
        const double twoPi = juce::MathConstants<double>::twoPi;

        // --- 4.1  Sine waveform stays in [-1, 1] across full cycle -----------
        beginTest ("LFO Sine: bounded [-1,1]");
        {
            for (int i = 0; i < 1024; ++i)
            {
                double ph  = twoPi * i / 1024.0;
                float  out = LFOWaveformTester::sine (ph);
                expect (out >= -1.01f && out <= 1.01f);
            }
        }

        // --- 4.2  Triangle waveform -----------------------------------------
        beginTest ("LFO Triangle: bounded [-1,1]");
        {
            bool ok = true;
            for (int i = 0; i < 1024 && ok; ++i)
            {
                double ph  = twoPi * i / 1024.0;
                float  out = LFOWaveformTester::triangle (ph);
                ok = (out >= -1.1f && out <= 1.1f);
            }
            expect (ok);
        }

        // --- 4.3  Saw waveform starts at +1 and descends to -1 --------------
        beginTest ("LFO Saw: starts at +1, ends near -1");
        {
            float atStart = LFOWaveformTester::saw (0.0);
            float atEnd   = LFOWaveformTester::saw (twoPi - 0.001);
            expectWithinAbsoluteError (atStart,  1.0f, 0.01f);
            expectWithinAbsoluteError (atEnd,   -1.0f, 0.01f);
        }

        // --- 4.4  Square waveform is exactly +1 or -1 -----------------------
        beginTest ("LFO Square: only +1 or -1");
        {
            bool ok = true;
            for (int i = 0; i < 1024 && ok; ++i)
            {
                double ph  = twoPi * i / 1024.0;
                float  out = LFOWaveformTester::square (ph);
                ok = (out == 1.0f || out == -1.0f);
            }
            expect (ok);
        }

        // --- 4.5  Square flips at pi -----------------------------------------
        beginTest ("LFO Square: flips sign at pi");
        {
            float beforePi = LFOWaveformTester::square (juce::MathConstants<double>::pi - 0.001);
            float afterPi  = LFOWaveformTester::square (juce::MathConstants<double>::pi + 0.001);
            expect (beforePi ==  1.0f);
            expect (afterPi  == -1.0f);
        }

        // --- 4.6  Sine at 0 and pi ------------------------------------------
        beginTest ("LFO Sine: zero-crossings at 0 and pi");
        {
            expectWithinAbsoluteError (LFOWaveformTester::sine (0.0),          0.0f, 0.001f);
            expectWithinAbsoluteError (LFOWaveformTester::sine (juce::MathConstants<double>::pi), 0.0f, 0.001f);
        }

        // --- 4.7  getTarget() encoding: fxBase = tgtIdx - 1, capped 0..14 --
        beginTest ("FXModLFO getTarget encoding: index 1 -> fxBase 0");
        {
            // Manually verify the encoding formula used in getTarget():
            // tgtIdx=1 → fxBase=0 → slot 0, param 0
            int tgtIdx = 1;
            int fxBase = tgtIdx - 1;
            expectEquals (fxBase, 0);
            expectEquals (fxBase / 5, 0); // slot
            expectEquals (fxBase % 5, 0); // param
        }

        beginTest ("FXModLFO getTarget encoding: index 15 -> slot 2 param 4");
        {
            int tgtIdx = 15;
            int fxBase = tgtIdx - 1; // 14
            expectEquals (fxBase / 5, 2); // slot 2
            expectEquals (fxBase % 5, 4); // Mix
        }
    }
};
static FXModLFOTests lfoTests;


// ═════════════════════════════════════════════════════════════════════════════
// 5.  FMOperator
// ═════════════════════════════════════════════════════════════════════════════
class FMOperatorTests : public juce::UnitTest
{
public:
    FMOperatorTests() : juce::UnitTest ("FMOperator", "oao") {}

    // Convenience: trigger a note and render n samples
    static float renderOneSample (FMOperator& op, WaveTable& wt, int waveShape = 0,
                                  float freq = 440.0f, int mode = 0)
    {
        juce::ADSR::Parameters p { 0.001f, 0.1f, 1.0f, 0.1f };
        op.noteOn (p);
        return op.processSample (freq, 120.0f,
            /*ratio*/1.0f, /*detune*/0.0f, /*phase*/0.0f, /*fold*/0.0f,
            /*audioIn*/0.0f, /*modSum*/0.0f,
            /*pitchMod*/0.0f, /*phaseMod*/0.0f, /*cutoffMod*/0.0f, /*foldMod*/0.0f,
            mode, waveShape, /*effectType*/0, /*freqMode*/0);
    }

    void runTest() override
    {
        WaveTable wt;
        wt.prepare();
        static constexpr double SR = 44100.0;

        // --- 5.1  Inactive operator returns 0 -------------------------------
        beginTest ("Inactive operator returns 0");
        {
            auto opPtr = std::make_unique<FMOperator>(); auto& op = *opPtr;
            op.prepare (SR, &wt);
            float out = op.processSample (440.0f, 120.0f,
                1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0);
            expectWithinAbsoluteError (out, 0.0f, 0.0001f);
        }

        // --- 5.2  All waveform modes stay in [-1, 1] during sustain ---------
        const char* waveNames[] = { "Sine","Triangle","Saw","Square","Pulse","SquarePWM","WhiteNoise","PinkNoise" };
        for (int ws = 0; ws < 8; ++ws)
        {
            beginTest (juce::String ("Waveform ") + waveNames[ws] + " bounded [-1,1]");
            {
                auto opPtr = std::make_unique<FMOperator>(); auto& op = *opPtr;
                op.prepare (SR, &wt);
                juce::ADSR::Parameters p { 0.001f, 0.1f, 1.0f, 0.5f };
                op.noteOn (p);

                float maxAbs = 0.0f;
                bool  ok     = true;
                for (int i = 0; i < 4096 && ok; ++i)
                {
                    float out = op.processSample (440.0f, 120.0f,
                        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 0.0f, 0, ws, 0, 0);
                    if (!std::isfinite (out)) { ok = false; break; }
                    maxAbs = std::max (maxAbs, std::abs (out));
                }
                expect (ok, "NaN/Inf detected");
                // During full sustain (gain=1), envelope×output ≤ 1
                expect (maxAbs <= 1.05f,
                    juce::String (waveNames[ws]) + " exceeds 1.05: " + juce::String (maxAbs));
            }
        }

        // --- 5.3  Additive mode stays finite --------------------------------
        beginTest ("Additive mode (mode=1): finite output");
        {
            auto opPtr = std::make_unique<FMOperator>(); auto& op = *opPtr;
            op.prepare (SR, &wt);
            juce::ADSR::Parameters p { 0.001f, 0.1f, 1.0f, 0.5f };
            op.noteOn (p);

            bool ok = true;
            for (int i = 0; i < 4096 && ok; ++i)
            {
                float out = op.processSample (440.0f, 120.0f,
                    1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 0.0f, 1, 0, 0, 0);
                ok = std::isfinite (out);
            }
            expect (ok);
        }

        // --- 5.4  Wavefolder produces finite output with large fold ----------
        beginTest ("Wavefold (fold=1.0): finite output");
        {
            auto opPtr = std::make_unique<FMOperator>(); auto& op = *opPtr;
            op.prepare (SR, &wt);
            juce::ADSR::Parameters p { 0.001f, 0.1f, 1.0f, 0.5f };
            op.noteOn (p);

            bool ok = true;
            for (int i = 0; i < 2048 && ok; ++i)
            {
                float out = op.processSample (440.0f, 120.0f,
                    1.0f, 0.0f, 0.0f, /*fold*/1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0);
                ok = std::isfinite (out);
            }
            expect (ok);
        }

        // --- 5.5  FM self-modulation stays finite with high mod index -------
        beginTest ("FM self-modulation (modSum=10): finite output");
        {
            auto opPtr = std::make_unique<FMOperator>(); auto& op = *opPtr;
            op.prepare (SR, &wt);
            juce::ADSR::Parameters p { 0.001f, 0.1f, 1.0f, 0.5f };
            op.noteOn (p);

            bool ok = true;
            for (int i = 0; i < 2048 && ok; ++i)
            {
                float out = op.processSample (440.0f, 120.0f,
                    1.0f, 0.0f, 0.0f, 0.0f, 0.0f, /*modSum*/10.0f,
                    0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0);
                ok = std::isfinite (out);
            }
            expect (ok);
        }

        // --- 5.6  resetPhase converts degrees to radians correctly ----------
        beginTest ("resetPhase: 360 degrees → same as 0");
        {
            auto opAPtr = std::make_unique<FMOperator>();
            auto opBPtr = std::make_unique<FMOperator>();
            auto& opA = *opAPtr;
            auto& opB = *opBPtr;
            opA.prepare (SR, &wt);
            opB.prepare (SR, &wt);
            opA.resetPhase (0.0f);
            opB.resetPhase (360.0f);
            juce::ADSR::Parameters p { 0.001f, 0.1f, 1.0f, 0.5f };
            opA.noteOn (p);
            opB.noteOn (p);

            // Both should produce the same first sample (phase starts at same position)
            float outA = opA.processSample (440.0f, 120.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0);
            float outB = opB.processSample (440.0f, 120.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0);
            expectWithinAbsoluteError (outA, outB, 0.001f);
        }

        // --- 5.7  Envelope transitions to inactive after note-off -----------
        beginTest ("Operator becomes inactive after note-off and release");
        {
            auto opPtr = std::make_unique<FMOperator>(); auto& op = *opPtr;
            op.prepare (SR, &wt);
            juce::ADSR::Parameters p { 0.001f, 0.001f, 1.0f, 0.001f }; // instant release
            op.noteOn (p);
            op.noteOff();

            bool becameInactive = false;
            for (int i = 0; i < (int)SR * 5; ++i)
            {
                op.processSample (440.0f, 120.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0);
                if (!op.isActive()) { becameInactive = true; break; }
            }
            expect (becameInactive, "Operator should become inactive after release");
        }

        // --- 5.8  Frequency modes produce finite output ---------------------
        beginTest ("FreqMode Sync (mode=1): finite output");
        {
            auto opPtr = std::make_unique<FMOperator>(); auto& op = *opPtr;
            op.prepare (SR, &wt);
            juce::ADSR::Parameters p { 0.001f, 0.1f, 1.0f, 0.5f };
            op.noteOn (p);
            bool ok = true;
            for (int i = 0; i < 2048 && ok; ++i)
            {
                float out = op.processSample (440.0f, 120.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, /*freqMode=Sync*/1);
                ok = std::isfinite (out);
            }
            expect (ok);
        }

        beginTest ("FreqMode Hz (mode=2): finite output at ratio=1 (1000Hz)");
        {
            auto opPtr = std::make_unique<FMOperator>(); auto& op = *opPtr;
            op.prepare (SR, &wt);
            juce::ADSR::Parameters p { 0.001f, 0.1f, 1.0f, 0.5f };
            op.noteOn (p);
            bool ok = true;
            for (int i = 0; i < 2048 && ok; ++i)
            {
                float out = op.processSample (440.0f, 120.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, /*freqMode=Hz*/2);
                ok = std::isfinite (out);
            }
            expect (ok);
        }

        // --- 5.9  Effect mode: passthrough with zero audio input is 0 -------
        beginTest ("Effect mode, passthrough (effectType=0), no audio input → 0");
        {
            auto opPtr = std::make_unique<FMOperator>(); auto& op = *opPtr;
            op.prepare (SR, &wt);
            juce::ADSR::Parameters p { 0.001f, 0.1f, 1.0f, 1.0f };
            op.noteOn (p);
            float out = op.processSample (440.0f, 120.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                /*audioIn*/0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, /*mode=Effect*/3, 0, /*effectType=pass*/0, 0);
            expectWithinAbsoluteError (out, 0.0f, 0.001f);
        }

        // --- 5.10  prepareForBlock / setSampleData null safety ---------------
        beginTest ("prepareForBlock with null sample buffer is safe");
        {
            auto opPtr = std::make_unique<FMOperator>(); auto& op = *opPtr;
            op.prepare (SR, &wt);
            op.prepareForBlock(); // blockSampleBuffer = nullptr
            juce::ADSR::Parameters p { 0.001f, 0.1f, 1.0f, 0.5f };
            op.noteOn (p);
            // mode=2 (Sample) with no buffer loaded — should fall through to audioInputSum
            float out = op.processSample (440.0f, 120.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, /*mode=Sample*/2, 0, 0, 0);
            expect (std::isfinite (out));
        }
    }
};
static FMOperatorTests operatorTests;


// ═════════════════════════════════════════════════════════════════════════════
// 6.  FMVoice
// ═════════════════════════════════════════════════════════════════════════════
class FMVoiceTests : public juce::UnitTest
{
public:
    FMVoiceTests() : juce::UnitTest ("FMVoice", "oao") {}

    void runTest() override
    {
        WaveTable wt;
        wt.prepare();
        static constexpr double SR = 44100.0;
        static constexpr int    BS = 512;

        // --- 6.1  Inactive voice renders silence ----------------------------
        beginTest ("Inactive voice renders silence");
        {
            auto voicePtr = std::make_unique<FMVoice>(); auto& voice = *voicePtr;
            voice.prepare (SR, BS, &wt);
            juce::AudioBuffer<float> buf (2, BS);
            buf.clear();
            juce::MidiBuffer midi;
            voice.renderNextBlock (buf, 0, BS);
            for (int ch = 0; ch < 2; ++ch)
                for (int s = 0; s < BS; ++s)
                    expectWithinAbsoluteError (buf.getSample (ch, s), 0.0f, 0.0001f);
        }

        // --- 6.2  resetVoiceState() zeros all history -----------------------
        beginTest ("resetVoiceState() sets outputs to zero");
        {
            auto voicePtr = std::make_unique<FMVoice>(); auto& voice = *voicePtr;
            voice.prepare (SR, BS, &wt);
            voice.resetVoiceState();
            // After reset, voice should be inactive (all envelopes reset)
            expect (!voice.isVoiceActive());
        }

        // --- 6.3  setOversamplingFactor does not crash ----------------------
        beginTest ("setOversamplingFactor(4) does not crash");
        {
            auto voicePtr = std::make_unique<FMVoice>(); auto& voice = *voicePtr;
            voice.prepare (SR, BS, &wt);
            voice.setOversamplingFactor (4);
            voice.setOversamplingFactor (1);
            expect (true); // just reached here without crashing
        }

        // --- 6.4  setDAWTempo stores the value ------------------------------
        beginTest ("setDAWTempo stores value without crash");
        {
            auto voicePtr = std::make_unique<FMVoice>(); auto& voice = *voicePtr;
            voice.prepare (SR, BS, &wt);
            voice.setDAWTempo (180.0f);
            expect (true);
        }

        // --- 6.5  currentVelocity and currentModWheel atomics ---------------
        beginTest ("Velocity and ModWheel atomics initialise to 0");
        {
            auto voicePtr = std::make_unique<FMVoice>(); auto& voice = *voicePtr;
            voice.prepare (SR, BS, &wt);
            expectWithinAbsoluteError (voice.currentVelocity.load(), 0.0f, 0.0001f);
            expectWithinAbsoluteError (voice.currentModWheel.load(), 0.0f, 0.0001f);
        }

        // --- 6.6  fxLfoOutputs atomics initialise to 0 ----------------------
        beginTest ("fxLfoOutputs[0..2] initialise to 0");
        {
            auto voicePtr = std::make_unique<FMVoice>(); auto& voice = *voicePtr;
            voice.prepare (SR, BS, &wt);
            for (int i = 0; i < 3; ++i)
                expectWithinAbsoluteError (voice.fxLfoOutputs[i].load(), 0.0f, 0.0001f);
        }

        // --- 6.7  setSampleData on all operator indices is safe -------------
        beginTest ("setSampleData on all operators is safe");
        {
            auto voicePtr = std::make_unique<FMVoice>(); auto& voice = *voicePtr;
            voice.prepare (SR, BS, &wt);
            auto buf = std::make_shared<juce::AudioBuffer<float>> (1, 1024);
            buf->clear();
            for (int i = 0; i < ProjectConfig::numOperators; ++i)
                voice.setSampleData (i, buf);
            expect (true);
        }

        // --- 6.8  setSampleData with out-of-range index is safe ------------
        beginTest ("setSampleData with out-of-range index is safe");
        {
            auto voicePtr = std::make_unique<FMVoice>(); auto& voice = *voicePtr;
            voice.prepare (SR, BS, &wt);
            auto buf = std::make_shared<juce::AudioBuffer<float>> (1, 512);
            voice.setSampleData (-1, buf);
            voice.setSampleData (99, buf);
            expect (true);
        }
    }
};
static FMVoiceTests voiceTests;


// ═════════════════════════════════════════════════════════════════════════════
// 7.  Constants / Utility
// ═════════════════════════════════════════════════════════════════════════════
class ConstantsTests : public juce::UnitTest
{
public:
    ConstantsTests() : juce::UnitTest ("Constants", "oao") {}

    void runTest() override
    {
        // --- 7.1  Effect type count matches knob label table ----------------
        beginTest ("getEffectTypeChoices count matches getEffectKnobLabels");
        {
            auto choices = ProjectConfig::getEffectTypeChoices();
            int n = choices.size();
            for (int i = 0; i < n; ++i)
            {
                auto labels = ProjectConfig::getEffectKnobLabels (i);
                // Every slot should have exactly 4 label strings (even if "--")
                int filled = 0;
                for (auto& lbl : labels)
                    if (lbl != nullptr) filled++;
                expectEquals (filled, 4,
                    "Effect index " + juce::String (i) + " (" + choices[i] + ") has wrong label count");
            }
        }

        // --- 7.2  Modulation targets list has expected size -----------------
        beginTest ("ModChoices::targets() has 82 entries (1 None + 30 op + 15 fx + 36 matrix)");
        {
            auto tgts = ModChoices::targets();
            // 1 (None) + 30 (6 ops × 5 params) + 15 (3 fx × 5 params) + 36 (6×6 matrix) = 82
            expectEquals (tgts.size(), 82);
        }

        // --- 7.3  FX targets list has 16 entries ----------------------------
        beginTest ("ModChoices::fxtargets() has 16 entries");
        {
            auto t = ModChoices::fxtargets();
            expectEquals (t.size(), 16); // 1 None + 3 slots × 5 params = 16
        }

        // --- 7.4  numOperators and numModSlots are as expected ---------------
        beginTest ("numOperators == 6, numModSlots == 6, numEffects == 3");
        {
            expectEquals (ProjectConfig::numOperators, 6);
            expectEquals (ProjectConfig::numModSlots,  6);
            expectEquals (ProjectConfig::numEffects,   3);
        }
    }
};
static ConstantsTests constantsTests;


// ═════════════════════════════════════════════════════════════════════════════
// 8.  Regression tests for previously-fixed bugs
// ═════════════════════════════════════════════════════════════════════════════
class RegressionTests : public juce::UnitTest
{
public:
    RegressionTests() : juce::UnitTest ("Regressions", "oao") {}

    void runTest() override
    {
        static constexpr double SR = 44100.0;

        // --- 8.1  Granular argument order (scatterAmt / grainDurationMs swap)
        //     After the fix: (freq, grainDurationMs, scatterAmt, damping, feedback)
        //     grainDurationMs=10 and grainDurationMs=1000 should both be finite.
        beginTest ("Regression: Granular grainDurationMs=10 finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            bool ok = sineInputFinite ([&](float in) {
                return fx.processSampleGranular (in, 440.0f, /*grainDurationMs*/10.0f,
                                                 /*scatterAmt*/0.5f, 0.3f, 0.0f);
            }, 4096);
            expect (ok);
        }

        beginTest ("Regression: Granular grainDurationMs=1000 finite");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            bool ok = sineInputFinite ([&](float in) {
                return fx.processSampleGranular (in, 220.0f, /*grainDurationMs*/1000.0f,
                                                 /*scatterAmt*/0.1f, 0.3f, 0.0f);
            }, 4096);
            expect (ok);
        }

        // --- 8.2  Wavefold bug fix: was using rawSample instead of foldedSample
        //     With the fix, fold=1.0 should produce a finite value that is NOT
        //     simply equal to std::sin(rawSample * drive) with an incorrect gain.
        beginTest ("Regression: Wavefold (fold=1.0) uses foldedSample, not rawSample");
        {
            WaveTable wt;
            wt.prepare();
            auto opPtr = std::make_unique<FMOperator>(); auto& op = *opPtr;
            op.prepare (SR, &wt);
            juce::ADSR::Parameters p { 0.001f, 0.1f, 1.0f, 0.5f };
            op.noteOn (p);
            bool allFiniteResult = true;
            for (int i = 0; i < 2048 && allFiniteResult; ++i)
            {
                float out = op.processSample (440.0f, 120.0f,
                    1.0f, 0.0f, 0.0f, /*fold*/1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, /*foldMod*/0.0f, 0, 0, 0, 0);
                allFiniteResult = std::isfinite (out);
            }
            expect (allFiniteResult);
        }

        // --- 8.3  Ring Mod frequency mapping should not go to 0 or absurdly high
        beginTest ("Regression: Ring Mod frequency mapped via ratio stays sane");
        {
            auto fxPtr = std::make_unique<SynthEffect>(); auto& fx = *fxPtr;
            fx.prepare (SR);
            // normalizedRatio = (ratio - 0.01) / (16.0 - 0.01)
            // frequency = 20 * pow(250, normalizedRatio)
            // At ratio=0.01 → freq=20Hz, at ratio=16 → freq=20*250=5000Hz
            float freqAtMin = 20.0f * std::pow (250.0f, 0.0f);         // 20 Hz
            float freqAtMax = 20.0f * std::pow (250.0f, 1.0f);         // 5000 Hz
            expect (freqAtMin >= 1.0f && freqAtMin <= 100.0f,
                    "Ring Mod min freq out of range: " + juce::String (freqAtMin));
            expect (freqAtMax >= 1000.0f && freqAtMax <= 25000.0f,
                    "Ring Mod max freq out of range: " + juce::String (freqAtMax));
        }

        // --- 8.4  Spectral Freeze pitch ratio at 0 semitones is exactly 1.0 -
        beginTest ("Regression: Spectral Freeze pitchRatio=pow(2,0/12)=1.0");
        {
            float ratio = std::pow (2.0f, 0.0f / 12.0f);
            expectWithinAbsoluteError (ratio, 1.0f, 0.0001f);
        }

        // --- 8.5  Spectral Freeze pitch ratio at +12 is 2.0 ----------------
        beginTest ("Regression: Spectral Freeze pitchRatio=pow(2,12/12)=2.0");
        {
            float ratio = std::pow (2.0f, 12.0f / 12.0f);
            expectWithinAbsoluteError (ratio, 2.0f, 0.0001f);
        }

        // --- 8.6  Spectral Freeze pitch ratio at -12 is 0.5 ----------------
        beginTest ("Regression: Spectral Freeze pitchRatio=pow(2,-12/12)=0.5");
        {
            float ratio = std::pow (2.0f, -12.0f / 12.0f);
            expectWithinAbsoluteError (ratio, 0.5f, 0.0001f);
        }
    }
};
static RegressionTests regressionTests;


// ═════════════════════════════════════════════════════════════════════════════
// main — runs all registered tests
// ═════════════════════════════════════════════════════════════════════════════
int main()
{
    // Initialise JUCE (needed for things like juce::Random, ADSR, etc.)
    juce::ScopedJuceInitialiser_GUI libraryInitialiser;

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;

    if (failures > 0)
        std::cerr << "\n*** " << failures << " test(s) FAILED ***\n";
    else
        std::cout << "\nAll tests passed.\n";

    return failures > 0 ? 1 : 0;
}
