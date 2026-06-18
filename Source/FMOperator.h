//FMOperator.h
#pragma once
#include <JuceHeader.h>
#include "SynthEffect.h"
#include "WaveTable.h"
#include <memory>

class FMOperator
{
public:
    void prepare (double sampleRate, WaveTable* wt)
    {
        currentSampleRate = sampleRate;
        waveTable = wt;
        envelope.setSampleRate (sampleRate * oversamplingFactor);
        phase = 0.0;
        internalEffect.prepare (sampleRate);
        internalEffect.reset();
    }

    void setEnvelopeParameters (const juce::ADSR::Parameters& p) {envelope.setParameters (p);}

    void resetEnvelope(){envelope.reset();}

    void noteOn (const juce::ADSR::Parameters& envParams)
    {
        envelope.setParameters (envParams);
        envelope.noteOn();
        phase = 0.0;
        samplePlayPos  = 0.0;
        xfadePlayPos   = 0.0;
        pingPongDir    = 1.0f;
        xfadePingDir   = 1.0f;
        xfadeActive    = false;
	scatterWindowOffset = 0.0f;
        internalEffect.reset();
    }

    void noteOff() { envelope.noteOff(); }

    void resetPhase (float phaseInDegrees)
    {
        phase = (phaseInDegrees / 360.0) * juce::MathConstants<double>::twoPi;
    }

    void prepareForBlock() noexcept{blockSampleBuffer = sampleBuffer;}

    void resetVoiceState()
    {
        envelope.reset();
        pinkB0 = pinkB1 = pinkB2 = pinkB3 = pinkB4 = pinkB5 = pinkB6 = 0.0f;
        samplePlayPos = 0.0;
        xfadePlayPos  = 0.0;
        pingPongDir   = 1.0f;
        xfadePingDir  = 1.0f;
        xfadeActive   = false;
	scatterWindowOffset = 0.0f;
	blockSampleBuffer = nullptr;
        internalEffect.reset();
    }

    void setOversamplingFactor (int factor)
    {
        oversamplingFactor = factor;
        envelope.setSampleRate (currentSampleRate * factor);
    }

    // Set a shared pointer to the loaded audio buffer for sample mode.
    // Thread-safe for audio thread reads as long as the buffer is not mutated after loading.
    void setSampleData (std::shared_ptr<juce::AudioBuffer<float>> newBuffer) noexcept
    {
        sampleBuffer  = newBuffer;
        samplePlayPos = 0.0;
        xfadePlayPos  = 0.0;
        xfadeActive   = false;
    }

    bool isActive() const { return envelope.isActive(); }

    float processSample (double baseFrequency, float currentBpm,
                         float ratio, float detune, float phaseKnob, float foldKnob,
                         float audioInputSum, float modulationSum,
                         float pitchModOffset, float phaseModOffset, float cutoffModOffset, float foldModOffset,
                         int mode, int waveShape, int effectType, int freqMode)
    {
        if (!envelope.isActive()) return 0.0f;

        float outputSample = 0.0f;
        if (mode == 2) // Sample playback
        {
            auto* buf = blockSampleBuffer.get();
	    if (buf != nullptr && buf->getNumSamples() > 0)
            {
                int numSamples  = buf->getNumSamples();
                int numChannels = buf->getNumChannels();

                // --- Speed / pitch ---
                // Ratio knob (0.01–16): playback speed multiplier. 1.0 = original pitch.
                float normalizedRatio = (ratio - 0.01f) / (16.0f - 0.01f); // 0..1
                float speedMult       = std::pow (2.0f, (normalizedRatio - 0.5f) * 4.0f); // ~0.25x to ~4x

                // Pitch the sample to the incoming MIDI note.
                // Root is middle C (261.63 Hz) — playing C4 plays back at original speed.
                static constexpr double rootHz = 261.6255653;
                float pitchScale = static_cast<float> (baseFrequency / rootHz);

                // FM modulation → subtle pitch nudge via speed
                float modSpeed = speedMult * pitchScale
                                 * (1.0f + std::tanh (modulationSum * 0.15f) * 0.5f);
                modSpeed = juce::jlimit (0.001f, 8.0f, modSpeed);

                // --- Region: start and end ---
                // Detune knob (-50..50): start point, 0..100% of sample
                float startFraction = (detune + 50.0f) / 100.0f;
                double startSample  = startFraction * (numSamples - 1);

                // Phase knob (0..360): end point. 360 = full sample end, 0 = just past start.
                // Minimum window is 64 samples to avoid divide-by-zero / DC bursts.
                float endFraction  = phaseKnob / 360.0f;
                double endSample   = startSample + std::max (64.0,
                                         endFraction * (numSamples - 1 - startSample));
                endSample          = std::min (endSample, static_cast<double> (numSamples - 1));
                double regionLen   = endSample - startSample;
                if (regionLen < 1.0) regionLen = 1.0;

                // --- Play mode (freqMode re-used as play-mode selector) ---
                // freqMode: 0 = One-shot, 1 = Loop, 2 = Ping-pong, 3 = Stutter
                int playMode = freqMode;

                // --- foldKnob re-purposed: boundary behaviour per mode ---
                // One-shot:   fade-out length at end of region (0 = hard stop, 1 = full-region fade)
                // Loop:       crossfade length at loop seam    (0 = hard loop, 1 = long blend)
                // Ping-pong:  crossfade length at turnarounds  (same semantics as loop)
                // Stutter:    grain window size                (256..32768 samples)
                float boundaryKnob = juce::jlimit (0.0f, 1.0f, foldKnob + foldModOffset);

                // --- Advance playback position ---
                double advance = modSpeed / (oversamplingFactor > 0 ? oversamplingFactor : 1);
                float sampleOut = 0.0f;

                // Helper: linearly interpolated sample read at an absolute position
                auto readSample = [&] (double pos) -> float
                {
                    pos = juce::jlimit (0.0, static_cast<double> (numSamples - 1), pos);
                    int   idx0 = static_cast<int> (pos);
                    int   idx1 = juce::jmin (idx0 + 1, numSamples - 1);
                    float frac = static_cast<float> (pos - std::floor (pos));
                    if (numChannels >= 2)
                    {
                        float s0 = buf->getSample (0, idx0) * (1.0f - frac) + buf->getSample (0, idx1) * frac;
                        float s1 = buf->getSample (1, idx0) * (1.0f - frac) + buf->getSample (1, idx1) * frac;
                        return (s0 + s1) * 0.5f;
                    }
                    return buf->getSample (0, idx0) * (1.0f - frac) + buf->getSample (0, idx1) * frac;
                };

                if (playMode == 3) // if scatter ...
                {
                    double minWindow     = std::min(2048.0, regionLen * 0.25);
                    double maxWindow     = regionLen * 0.5;
                    maxWindow            = std::max(maxWindow, minWindow + 1.0);
                    double windowSamples = minWindow + (boundaryKnob * (maxWindow - minWindow));
                    windowSamples        = std::min(windowSamples, regionLen);
                
                    // When the grain window completes, jump to a new start position
                    if (samplePlayPos >= windowSamples)
                    {
                        samplePlayPos -= windowSamples;
                
                        // Scatter: pick a new random offset within the region
                        // boundaryKnob == 0 → always restarts at startSample (no scatter)
                        // boundaryKnob == 1 → jumps anywhere in the region
                        double scatterRange  = boundaryKnob * (regionLen - windowSamples);
                        scatterWindowOffset  = (regionLen > windowSamples)
                                               ? (std::rand() / (double)RAND_MAX) * scatterRange
                                               : 0.0;
                    }
                
                    double playPos = startSample + scatterWindowOffset + samplePlayPos;
                    sampleOut      = readSample(playPos);
                    samplePlayPos += advance;
                }
		else if (playMode == 0) // One-shot with fade-out
                {
                    double playPos = startSample + samplePlayPos;
                    sampleOut = readSample (playPos);

                    samplePlayPos += advance;

                    if (samplePlayPos >= regionLen)
                    {
                        // Clamp at the very last valid position and kill the voice.
                        samplePlayPos = regionLen - 1.0;
                        envelope.noteOff(); // stop the envelope — operator is done
                    }
                    else
                    {
                        // Fade-out: apply gain ramp over the tail defined by boundaryKnob.
                        // boundaryKnob == 0 → no fade (hard stop at end).
                        // boundaryKnob == 1 → fade starts from the very beginning of the region.
                        double fadeLen = boundaryKnob * regionLen;
                        if (fadeLen > 1.0)
                        {
                            double distFromEnd = regionLen - samplePlayPos;
                            if (distFromEnd < fadeLen)
                            {
                                float fadeGain = static_cast<float> (distFromEnd / fadeLen);
                                sampleOut *= fadeGain;
                            }
                        }
                    }
                }
                else if (playMode == 1) // Loop with crossfade at seam
                {
                    // Crossfade length: boundaryKnob maps 0..1 → 0..25% of region.
                    // Keeping it capped at 25% avoids the two heads overlapping past the midpoint.
                    double xfadeLen = boundaryKnob * regionLen * 0.25;

                    double playPos = startSample + samplePlayPos;
                    sampleOut = readSample (playPos);

                    if (xfadeLen > 1.0 && samplePlayPos >= (regionLen - xfadeLen))
                    {
                        // Blend with a second head that is xfadeLen samples behind the loop start.
                        double xPos        = startSample + (samplePlayPos - (regionLen - xfadeLen));
                        float  xSample     = readSample (startSample + xPos);
                        float  xfadeAlpha  = static_cast<float> ((samplePlayPos - (regionLen - xfadeLen)) / xfadeLen);
                        xfadeAlpha         = juce::jlimit (0.0f, 1.0f, xfadeAlpha);
                        sampleOut          = sampleOut * (1.0f - xfadeAlpha) + xSample * xfadeAlpha;
                    }

                    samplePlayPos += advance;
                    while (samplePlayPos >= regionLen)
                        samplePlayPos -= regionLen;
                }
                else // Ping-pong with crossfade at turnarounds
                {
                    // Crossfade length: same cap as loop mode.
                    double xfadeLen = boundaryKnob * regionLen * 0.25;

                    double playPos = startSample + samplePlayPos;
                    sampleOut = readSample (playPos);

                    // Blend near either turnaround point using a mirrored read-head.
                    if (xfadeLen > 1.0)
                    {
                        double distFromEdge = (pingPongDir > 0.0f)
                                              ? (regionLen - samplePlayPos)  // approaching end
                                              : samplePlayPos;               // approaching start

                        if (distFromEdge < xfadeLen)
                        {
                            // Mirror position: reflects off whichever wall we're near.
                            double mirrorPos = (pingPongDir > 0.0f)
                                              ? (startSample + regionLen - distFromEdge)
                                              : (startSample + distFromEdge);
                            float  mirrorSample = readSample (mirrorPos);
                            float  xfadeAlpha   = static_cast<float> (1.0 - (distFromEdge / xfadeLen));
                            xfadeAlpha          = juce::jlimit (0.0f, 1.0f, xfadeAlpha);
                            sampleOut           = sampleOut * (1.0f - xfadeAlpha) + mirrorSample * xfadeAlpha;
                        }
                    }

                    samplePlayPos += pingPongDir * advance;
                    if (samplePlayPos >= regionLen)
                    {
                        samplePlayPos = (2.0 * regionLen) - samplePlayPos; // reflect, no clamp
                        if (samplePlayPos < 0.0) samplePlayPos = 0.0;      // guard extreme overshoot only
                        pingPongDir = -1.0f;
                    }
                    else if (samplePlayPos <= 0.0)
                    {
                        samplePlayPos = -samplePlayPos; // reflect, no clamp
                        if (samplePlayPos > regionLen) samplePlayPos = regionLen; // guard extreme overshoot
                        pingPongDir = 1.0f;
                    }

                }

                outputSample = std::isfinite (sampleOut)
                                   ? std::tanh (sampleOut + audioInputSum)
                                   : 0.0f;
            }
            else
            {
                // No sample loaded — pass audio matrix input through silently
                outputSample = std::isfinite (audioInputSum) ? audioInputSum : 0.0f;
            }
        }
        else if (mode == 3) // effect
        {
            if (effectType == 0) // passthrough
            {
                outputSample = std::isfinite (audioInputSum) ? audioInputSum : 0.0f;
            }
            else if (effectType == 1 || effectType == 2 || effectType == 3) // SVF - Lowpass, Highpass, Bandpass
            {
                // say it with me. keytracking!
                float normalizedRatio  = (ratio - 0.01f) / (16.0f - 0.01f);
                float baseFreq         = 20.0f * std::pow(1000.0f, normalizedRatio);
                float keytrackAmt      = phaseKnob / 360.0f;
                float tunedFreq        = baseFreq + keytrackAmt * (baseFrequency - baseFreq);
                float dampingAmt       = juce::jlimit(0.001f, 0.95f, (detune + 50.0f) / 100.0f);
                // process actual inputs for outputting!
                float processedModSum  = std::tanh(modulationSum * 0.2f) * 5.0f;
                float coupledResonance = dampingAmt * dampingAmt;
                internalEffect.setResonance(coupledResonance);
                float currentK         = internalEffect.getPrecalculatedK();
                float dynamicCutoff    = tunedFreq + (processedModSum * 5000.0f) + (cutoffModOffset * 4000.0f);
                dynamicCutoff          = juce::jlimit(20.0f, static_cast<float>(currentSampleRate) * 0.49f, dynamicCutoff);
                // output!
                float output           = internalEffect.processSampleAudioRate(audioInputSum, dynamicCutoff, currentK);
                outputSample           = std::isfinite(output) ? std::tanh(output) : 0.0f;
            }
            else if (effectType == 4) // Filter + Drive                  
            {
                float cutoff   = ratio; // 0.0 -> 1.0
                float res      = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float drive    = phaseKnob / 360.0f;
                float LP_or_HP = juce::jlimit (0.0f, 1.0f, foldKnob); // < 0.5 is LP, >= 0.5 is HP
                
                float output = internalEffect.processSampleFilterDrive (audioInputSum, cutoff, res, drive, LP_or_HP, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (effectType == 5) // Comb
            {
                // set up keytracking
                float normalizedRatio = (ratio - 0.01f) / (16.0f - 0.01f);
                float baseFreq        = 20.0f * std::pow(1000.0f, normalizedRatio);
                float keytrackAmt     = phaseKnob / 360.0f;
                float tunedFreq       = baseFreq + keytrackAmt * (baseFrequency - baseFreq);
                // set up actual comb input vars
                float modDepth    = 1.0f - keytrackAmt;
                float combFreq    = tunedFreq + modDepth * (modulationSum * 200.0f + cutoffModOffset * 4000.0f);
                combFreq          = juce::jlimit(20.0f, static_cast<float>(currentSampleRate) * 0.49f, combFreq);
                float dampingAmt = juce::jlimit(0.001f, 0.95f, (foldKnob * 2.0f) - 1.0f);
                float feedbackAmt  = juce::jlimit(-0.95f, 0.95f, (detune + 50.0f) / 100.0f);
                // output
                float output = internalEffect.processSampleComb(audioInputSum, combFreq, feedbackAmt, dampingAmt);
                outputSample = std::isfinite(output) ? std::tanh(output) : 0.0f;
            }
            else if (effectType == 6) // Formant
            {
                // Vowel (Ratio knob): Scale 0.01 - 16.0 to 0.0 - 4.0 (A-E-I-O-U)
                float normalizedRatio = (ratio - 0.01f) / (16.0f - 0.01f);
                float baseVowel       = normalizedRatio * 4.0f;
                // Vowel Mod (Phase knob): How much modulation affects the vowel sweep
                float modDepth = phaseKnob / 360.0f;
                // Apply modulation (scaled so full modulation can sweep the whole table)
                float dynamicVowel = baseVowel + (modulationSum * modDepth * 4.0f);
                dynamicVowel       = juce::jlimit(0.0f, 4.0f, dynamicVowel);
                // Resonance/Nasality (Detune knob): Map -50.0 - 50.0 to a Q-Factor of 2.0 to 15.0
                float normalizedDetune = (detune + 50.0f) / 100.0f;
                float qFactor          = juce::jmap(normalizedDetune, 0.0f, 1.0f, 2.0f, 15.0f);
                // Drive (Fold knob): Pushes the input into a soft-clipper BEFORE the effect.
                float drive       = 1.0f + (foldKnob * 4.0f); // 1.0x to 5.0x drive
                float drivenInput = std::tanh(audioInputSum * drive);
                // output
                float output = internalEffect.processSampleFormant(drivenInput, dynamicVowel, qFactor);
                outputSample = std::isfinite(output) ? std::tanh(output) : 0.0f;
            }
            else if (effectType == 7) // Compressor
            {
                float threshold = (ratio - 0.01f) / (16.0f - 0.01f);
                float compRatio = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float attack    = phaseKnob / 360.0f;
                float release   = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output = internalEffect.processSampleCompressor (audioInputSum, threshold,
                                                                        compRatio, attack, release,
                                                                        currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (effectType == 8) // Three-Lane EQ
            {
                float lowGain  = ratio;
                float midGain  = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float highGain = phaseKnob / 360.0f;
                float master   = juce::jlimit (0.0f, 1.0f, foldKnob); // Master Output Ceiling
            
                float output = internalEffect.processSampleThreeBandEQ (audioInputSum, lowGain, midGain, highGain, master, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (effectType == 9) // OTT
            {
                float depth   = (ratio - 0.01f) / (16.0f - 0.01f);
                float timeKnob = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f); 
                float upward  = phaseKnob / 360.0f;
                float tone    = juce::jlimit (0.0f, 1.0f, foldKnob);
          
                float output = internalEffect.processSampleOTT (audioInputSum, depth, timeKnob,
                                                                 upward, tone, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (effectType == 10) // Lo-Fi Effect
            {
                float decimate = ratio;                  // Knob 1: Downsampling
                float bits     = (detune + 50.0f) / 100.0f; // Knob 2: Bitcrush
                float wear     = phaseKnob / 360.0f;     // Knob 3: Tape Saturation & Vinyl Dust
                float tone     = juce::jlimit (0.0f, 1.0f, foldKnob); // Knob 4: Gramophone Bandpass
            
                outputSample = internalEffect.processSampleLoFi (audioInputSum, decimate, bits, wear, tone, currentSampleRate);
            }
            else if (effectType == 11) // Tape
            {
                float wobbleRate = (ratio - 0.01f) / (16.0f - 0.01f);
                float age        = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float saturation = phaseKnob / 360.0f;
                float bias       = juce::jlimit (0.0f, 1.0f, foldKnob); // 0.5 = optimal
                float output = internalEffect.processSampleTape (audioInputSum, wobbleRate, age,
                                                                  saturation, bias, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (effectType == 12) // Chorus
            {
                float rate   = (ratio - 0.01f) / (16.0f - 0.01f);
                float depth  = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float spread = phaseKnob / 360.0f;
                float voices = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output = internalEffect.processSampleChorus (audioInputSum, rate, depth,
                                                                    spread, voices, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (effectType == 13) // Old Chorus
            {
                float rate    = (ratio - 0.01f) / (16.0f - 0.01f);
                float depth   = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float modeKnob = phaseKnob / 360.0f;
                float warmth  = juce::jlimit (0.0f, 1.0f, foldKnob);

                float output = internalEffect.processSampleOldChorus (audioInputSum, rate, depth,
                                                                       modeKnob, warmth,
                                                                       currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (effectType == 14) // Distortion
            {
                float drive       = (ratio - 0.01f) / (16.0f - 0.01f);
                float flavor      = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float toneKnob    = phaseKnob / 360.0f;
                float degradation = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output = internalEffect.processSampleDistortion (audioInputSum, drive, flavor,
                                                                        toneKnob, degradation,
                                                                        currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;    
            }
            else if (effectType == 15) // Bitcrush
            {
                float bits   = (ratio - 0.01f) / (16.0f - 0.01f);
                float rate   = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float jitter = phaseKnob / 360.0f;
                float noise  = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output = internalEffect.processSampleBitcrush (audioInputSum, bits, rate,
                                                                      jitter, noise, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (effectType == 16) // Ring Modulator
            {
                // Frequency: ratio knob maps to 0.1Hz - 5000Hz exponentially
                float normalizedRatio = (ratio - 0.01f) / (16.0f - 0.01f);
                float frequency       = 20.0f * std::pow (250.0f, normalizedRatio);
		float shape           = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float depth           = phaseKnob / 360.0f;
                float feedback        = juce::jlimit (0.0f, 0.95f, foldKnob);
                float output = internalEffect.processSampleRingMod (audioInputSum, frequency, shape,
                                                                     depth, feedback, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (effectType == 17) // Allpass Reverb
            {
                float size      = (ratio - 0.01f) / (16.0f - 0.01f);
                float decay     = juce::jlimit (0.0f, 0.98f, (detune + 50.0f) / 100.0f);
                float diffusion = phaseKnob / 360.0f;
                float damping   = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output = internalEffect.processSampleAllpassReverb (audioInputSum, size, decay,
                                                                           diffusion, damping, currentSampleRate);
                outputSample = std::isfinite (output) ? std::tanh (output) : 0.0f;
            }
            else if (effectType == 18) // Allpass Delay
            {
                float time      = (ratio - 0.01f) / (16.0f - 0.01f);
                float feedback  = juce::jlimit (0.0f, 0.95f, (detune + 50.0f) / 100.0f);
                float diffusion = phaseKnob / 360.0f;
                float damping   = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output = internalEffect.processSampleAllpassDelay (audioInputSum, time, feedback,
                                                                          diffusion, damping, currentSampleRate);
                outputSample = std::isfinite (output) ? std::tanh (output) : 0.0f;
            }
            else if (effectType == 19) // Timeshift Delay
            {
                float delayTime = ratio;
                float feedback  = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float damping   = phaseKnob / 360.0f;
                float drive     = juce::jlimit (0.0f, 1.0f, foldKnob);
                
                float output = internalEffect.processSampleTimeCtrlDelay (audioInputSum, delayTime, feedback, damping, drive, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (effectType == 20) // Ambient Delay
            {
                float time      = (ratio - 0.01f) / (16.0f - 0.01f);
                float feedbk    = juce::jlimit (0.0f, 0.98f, (detune + 50.0f) / 100.0f);
                float shimmerAmt = phaseKnob / 360.0f;
                float diffusion  = juce::jlimit (0.0f, 1.0f, foldKnob);

                float output = internalEffect.processSampleAmbientDelay (audioInputSum, time, feedbk,
                                                                          shimmerAmt, diffusion,
                                                                          currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (effectType == 21) // DJFX Delay
            {
                float bufferAmt = (ratio - 0.01f) / (16.0f - 0.01f);
                float speed     = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float on        = phaseKnob / 360.0f;
                float drift     = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output = internalEffect.processSampleDJFXDelay (audioInputSum, bufferAmt, speed,
                                                                       on, drift, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (effectType == 22) // Scatter
            {
                float pattern = (ratio - 0.01f) / (16.0f - 0.01f);
                float size    = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float speed   = phaseKnob / 360.0f;
                float depth   = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output = internalEffect.processSampleScatter (audioInputSum, pattern, size,
                                                                     speed, depth, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (effectType == 23) // Granular
            {                                                              
                // we assume keytracking so we can track scatter and grain size
                float granularFreq = baseFrequency + (modulationSum * 200.0f);
                granularFreq = juce::jlimit(20.0f, static_cast<float>(currentSampleRate) * 0.49f, granularFreq);
                // set floats for each variable to current knobs, scale knobs to scale required for vars
                float grainDurationMs = juce::jmap((ratio - 0.01f) / (16.0f - 0.01f), 0.0f, 1.0f, 10.0f, 1000.0f);
		float scatterAmt      = juce::jlimit(0.0f, 1.0f, (detune + 50.0f) / 100.0f);
		float dampingAmt      = juce::jlimit(0.001f, 0.95f, phaseKnob / 360.0f);
                float feedbackAmt     = juce::jlimit(-0.95f, 0.95f, (foldKnob * 2.0f) - 1.0f);
                // output
                float output = internalEffect.processSampleGranular(audioInputSum, granularFreq, grainDurationMs, scatterAmt, dampingAmt, feedbackAmt);
		outputSample = std::isfinite(output) ? std::tanh(output) : 0.0f;
            }
            else if (effectType == 24) // Color Bass
            {
                float drive   = juce::jlimit (0.0f, 1.0f, (ratio - 0.01f) / (16.0f - 0.01f));
                float shimmer = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
		float tone    = juce::jlimit (0.0f, 1.0f, phaseKnob / 360.0f);
                float decay   = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output  = internalEffect.processSampleColorBass (audioInputSum, drive,
                                                                        shimmer, tone,
                                                                        decay, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (effectType == 25) // Spectral Freeze
            {
                float freeze = (ratio - 0.01f) / (16.0f - 0.01f);
                float blend  = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float pitch  = phaseKnob / 360.0f;
                float blur   = juce::jlimit (0.0f, 1.0f, foldKnob);
            
                float output = internalEffect.processSampleSpectralFreeze (audioInputSum, freeze,
                                                                            blend, pitch, blur,
                                                                            currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
        }
        else // Here are our oscillators, Wave and Additive. They all need these:
        {
            // keytracking, fm, tempo sync, osc hz set
            float nodeTargetFrequency;
            switch (freqMode)
            {
                case 1: // Sync — tempo relative
                    nodeTargetFrequency = currentBpm / 60.0f * ratio;
                    break;
            
                case 2: // Hz — absolute frequency
                {
                    nodeTargetFrequency = ratio * 1000.0f;
                    break;
                }
            
                case 3: // LFO — 0.01Hz to 16Hz directly
                    nodeTargetFrequency = ratio;
                    break;
            
                default: // Standard — ratio * base frequency
                    nodeTargetFrequency = baseFrequency * ratio;
                    break;
            }
            float totalSemitones = (detune / 100.0f) + (pitchModOffset * 12.0f);
            float modulatedFreq  = nodeTargetFrequency * std::pow(2.0f, totalSemitones / 12.0f);
            modulatedFreq = juce::jlimit(0.1f, static_cast<float>(currentSampleRate) * 0.49f, modulatedFreq);
            // Corrected phaseIncrement to take into account oversampling.
            double phaseIncrement = (modulatedFreq * juce::MathConstants<double>::twoPi) 
                        / (currentSampleRate * oversamplingFactor);
            phase += phaseIncrement;
            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
            float processedModSum = std::tanh(modulationSum * 0.15f) * (juce::MathConstants<float>::pi * 2.0f);
            float phaseOffsetRad  = ((phaseKnob + (phaseModOffset * 360.0f)) / 360.0f) * juce::MathConstants<float>::twoPi;
            float lookupPhase     = static_cast<float>(phase) + processedModSum + phaseOffsetRad;
            float wrappedPhase    = std::fmod(lookupPhase, juce::MathConstants<float>::twoPi);
            if (wrappedPhase < 0.0f)
                wrappedPhase += juce::MathConstants<float>::twoPi;
            float rawSample = 0.0f;
            if (mode == 1) // Additive
            {
                // required constants
                const int numPartials  = 32;
                float additiveSum      = 0.0f;
                float gainCompensation = 0.0f;

                // Convert knobs to proper versions for additive specifically
                float tiltKnob    = (detune + 50.0f) / 100.0f;
                float stretchKnob = phaseKnob / 360.0f;
                float oddEvenKnob = foldKnob;

                float tiltExponent  = (tiltKnob - 0.5f) * -3.0f;
                float stretchFactor = std::pow(2.0f, (stretchKnob - 0.5f) * 1.5f);

                float oddWeight   = 1.0f - juce::jlimit(0.0f, 1.0f, oddEvenKnob * 2.0f);
                float evenWeight  = juce::jlimit(0.0f, 1.0f, (oddEvenKnob - 0.5f) * 2.0f);
                float blendWeight = 1.0f - std::abs(oddEvenKnob - 0.5f) * 2.0f;

                float dt = static_cast<float>(phaseIncrement / juce::MathConstants<double>::twoPi);

                for (int k = 1; k <= numPartials; ++k) // Process the partials to create output
                {
                    bool isEven = (k % 2 == 0);
                    float oeAmp = juce::jlimit(0.0f, 1.0f, isEven
                        ? (evenWeight + blendWeight)
                        : (oddWeight  + blendWeight));
                    float tiltAmp   = std::pow(static_cast<float>(k), tiltExponent);
                    float amplitude = (1.0f / static_cast<float>(k)) * tiltAmp * oeAmp;
                    float stretchedK    = std::pow(static_cast<float>(k), stretchFactor);
                    float harmonicPhase = std::fmod(lookupPhase * stretchedK, juce::MathConstants<float>::twoPi);
                    if (harmonicPhase < 0.0f)
                        harmonicPhase += juce::MathConstants<float>::twoPi;

                    float partialSample = waveTable->lookupSine(harmonicPhase);
                    additiveSum      += amplitude * partialSample;
                    gainCompensation += amplitude;
                }
                // output
                rawSample    = (gainCompensation > 0.0f) ? additiveSum / gainCompensation : 0.0f;
                outputSample = std::isfinite(rawSample + audioInputSum)
                               ? rawSample + audioInputSum : 0.0f;
            }
            else // Wave
            {
                switch (waveShape)
                {
                    case 0:  rawSample = waveTable->lookupSine     (wrappedPhase); break;
                    case 1:  rawSample = waveTable->lookupTriangle (wrappedPhase); break;
                    case 2:  rawSample = waveTable->lookupSaw      (wrappedPhase); break;
                    case 3:  rawSample = waveTable->lookupSquare   (wrappedPhase); break;
                    case 4: // Pulse with PWM + PolyBLEP
                    {
                        float dutyCycle  = juce::jmap (phaseKnob, 0.0f, 360.0f, 0.05f, 0.95f);
                        float threshold  = dutyCycle * juce::MathConstants<float>::twoPi;
                        rawSample        = (wrappedPhase < threshold) ? 1.0f : -1.0f;
                    
                        float dt = static_cast<float> (phaseIncrement / juce::MathConstants<double>::twoPi);
                        float t  = wrappedPhase / juce::MathConstants<float>::twoPi;
                        float t2 = std::fmod (t + (1.0f - dutyCycle), 1.0f);
                    
                        rawSample += polyBlep (t,  dt);  // rising edge at 0
                        rawSample -= polyBlep (t2, dt);  // falling edge at duty cycle point
                        break;
                    }
                    case 5: // Square with PWM — phase knob controls duty cycle
                    {
                        float dutyCycle  = juce::jmap (phaseKnob, 0.0f, 360.0f, 0.05f, 0.95f);
                        float threshold  = dutyCycle * juce::MathConstants<float>::twoPi;
                        rawSample        = (wrappedPhase < threshold) ? 1.0f : -1.0f;
                    
                        // PolyBLEP for both edges
                        float dt = static_cast<float> (phaseIncrement / juce::MathConstants<double>::twoPi);
                        float t  = wrappedPhase / juce::MathConstants<float>::twoPi;
                        float t2 = std::fmod (t + (1.0f - dutyCycle), 1.0f);
                    
                        rawSample += polyBlep (t,  dt);
                        rawSample -= polyBlep (t2, dt);
                        break;
                    }
                    case 6:  rawSample = random.nextFloat() * 2.0f - 1.0f;         break; // white noise
                    case 7: // pink
                    {
                        float white = random.nextFloat() * 2.0f - 1.0f;
                        pinkB0 = 0.99886f * pinkB0 + white * 0.0555179f;
                        pinkB1 = 0.99332f * pinkB1 + white * 0.0750759f;
                        pinkB2 = 0.96900f * pinkB2 + white * 0.1538520f;
                        pinkB3 = 0.86650f * pinkB3 + white * 0.3104856f;
                        pinkB4 = 0.55000f * pinkB4 + white * 0.5329522f;
                        pinkB5 = -0.7616f * pinkB5 - white * 0.0168980f;
                        rawSample = (pinkB0 + pinkB1 + pinkB2 + pinkB3 + pinkB4 + pinkB5 + pinkB6 + white * 0.5362f) * 0.11f;
                        pinkB6 = white * 0.115926f;
                        break;
                    }
                    default: rawSample = waveTable->lookupSine (wrappedPhase); break;
                }

                float finalFoldDepth = juce::jlimit(0.0f, 1.0f, foldKnob + foldModOffset);
                if (finalFoldDepth > 0.001f)
                {
                    float drive        = 1.0f + (finalFoldDepth * 5.0f);
                    // BUG FIX: was using rawSample here instead of foldedSample
                    float foldedSample = std::sin (rawSample * drive) * (1.0f / std::sqrt (drive));
                    outputSample = std::isfinite (foldedSample + audioInputSum)
                                   ? foldedSample + audioInputSum : 0.0f;
                }
                else
                {
                    outputSample = std::isfinite (rawSample + audioInputSum) 
                                   ? rawSample + audioInputSum : 0.0f;
                }
            }
        }

        return outputSample * envelope.getNextSample();
    }

private:
    // Shared pointer to loaded sample audio. Set from the main thread before playback.
    std::shared_ptr<juce::AudioBuffer<float>> sampleBuffer;
    std::shared_ptr<juce::AudioBuffer<float>> blockSampleBuffer { nullptr };
    double samplePlayPos = 0.0;
    // Secondary playhead used for crossfade blending (loop seam / ping-pong turnarounds).
    // Not needed for one-shot or stutter — those modes do their blending inline.
    double xfadePlayPos  = 0.0;
    bool   xfadeActive   = false;
    float  xfadePingDir  = 1.0f; // direction of the mirror head in ping-pong xfade
    float scatterWindowOffset = 0.0f; //offset that the scatter function uses to figure out how scattered it is from the front of the sample
    float  pingPongDir   = 1.0f; // +1 = forward, -1 = backward (ping-pong mode)
    WaveTable* waveTable  = nullptr;
    double phase = 0.0;
    double currentSampleRate = 44100.0;
    int oversamplingFactor = 1;
    juce::ADSR envelope;
    juce::Random random; // white noise
    float pinkB0 = 0.0f, pinkB1 = 0.0f, pinkB2 = 0.0f;
    float pinkB3 = 0.0f, pinkB4 = 0.0f, pinkB5 = 0.0f, pinkB6 = 0.0f;
    float polyBlep (float t, float dt)
    {
        // t is phase normalized to [0, 1), dt is phase increment normalized to [0, 1)
        if (t < dt) // Near rising discontinuity
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        else if (t > 1.0f - dt) // Near falling discontinuity
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }
    SynthEffect internalEffect;
};
