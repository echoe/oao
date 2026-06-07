//FMOperator.h
#pragma once
#include <JuceHeader.h>
#include "SynthFilter.h"
#include "WaveTable.h"

class FMOperator
{
public:
    void prepare (double sampleRate, WaveTable* wt)
    {
        currentSampleRate = sampleRate;
        waveTable = wt;
        envelope.setSampleRate (sampleRate * oversamplingFactor);
        phase = 0.0;
        internalFilter.prepare (sampleRate);
        internalFilter.reset();
    }

    void setEnvelopeParameters (const juce::ADSR::Parameters& p)
    {
        envelope.setParameters (p);
    }

    void noteOn (const juce::ADSR::Parameters& envParams)
    {
        envelope.setParameters (envParams);
        envelope.noteOn();
        phase = 0.0;
        internalFilter.reset();
    }

    void noteOff() { envelope.noteOff(); }

    void resetPhase (float phaseInDegrees)
    {
        phase = (phaseInDegrees / 360.0) * juce::MathConstants<double>::twoPi;
    }

    void resetVoiceState()
    {
        envelope.reset();
        pinkB0 = pinkB1 = pinkB2 = pinkB3 = pinkB4 = pinkB5 = pinkB6 = 0.0f; // reset pink noise state. if this doesn't work put it in noteon
        internalFilter.reset();
    }

    void setOversamplingFactor (int factor)
    {
        oversamplingFactor = factor;
        envelope.setSampleRate (currentSampleRate * factor);
    }

    void setExternalAudioSample (float left, float right) noexcept
    {
        externalAudioL = left;
        externalAudioR = right;
    }

    bool isActive() const { return envelope.isActive(); }

    float processSample (double baseFrequency, float currentBpm,
                         float ratio, float detune, float phaseKnob, float foldKnob,
                         float audioInputSum, float modulationSum,
                         float pitchModOffset, float phaseModOffset, float cutoffModOffset, float foldModOffset,
                         int mode, int waveShape, int filterType, bool isSynced)
    {
        if (!envelope.isActive()) return 0.0f;

        float outputSample = 0.0f;
	if (mode == 3) // wave
        {
            // Ratio knob → input gain
            float inputGain = (ratio - 0.01f) / (16.0f - 0.01f) * 2.0f;
            
	    // Mix stereo to mono and apply gain
            float extSample = (externalAudioL + externalAudioR) * 0.5f * inputGain;

            // Detune knob → one-pole tone filter
            float toneAmt     = (detune + 50.0f) / 100.0f;
            float filterCoeff = juce::jlimit (0.01f, 0.99f, toneAmt);
            extSample         = extSample * filterCoeff + externalAudioL * (1.0f - filterCoeff);
            
	    // Phase knob → FM modulation sensitivity
            float modSensitivity = phaseKnob / 360.0f;
            float modDepth       = std::tanh (modulationSum * 0.15f * modSensitivity);
            extSample           *= (1.0f + modDepth);
        
            // Fold knob → wavefold depth
            float foldDepth = juce::jlimit (0.0f, 1.0f, foldKnob + foldModOffset);
            if (foldDepth > 0.001f)
            {
                float drive = 1.0f + (foldDepth * 5.0f);
                extSample   = std::sin (extSample * drive) * (1.0f / std::sqrt (drive));
            }
        
            // Feed through audioInputSum so the audio matrix works
            outputSample = std::isfinite (extSample)
                               ? std::tanh (extSample + audioInputSum)
                               : 0.0f;
        }
        else if (mode == 2) // filter. These all have soft clippers at the end to avoid being /too/ hot.
        {
            if (filterType == 0) // passthrough
            {
                outputSample = std::isfinite (audioInputSum) ? audioInputSum : 0.0f;
            }
            else if (filterType == 1 || filterType == 2 || filterType == 3) // SVF - Lowpass, Highpass, Bandpass
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
                internalFilter.setResonance(coupledResonance);
                float currentK         = internalFilter.getPrecalculatedK();
                float dynamicCutoff    = tunedFreq + (processedModSum * 5000.0f) + (cutoffModOffset * 4000.0f);
                dynamicCutoff          = juce::jlimit(20.0f, static_cast<float>(currentSampleRate) * 0.49f, dynamicCutoff);
                // output!
                float output           = internalFilter.processSampleAudioRate(audioInputSum, dynamicCutoff, currentK);
                outputSample           = std::isfinite(output) ? std::tanh(output) : 0.0f;
            }
            else if (filterType == 4) // Comb
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
                float feedbackAmt = juce::jlimit(-0.95f, 0.95f, (foldKnob * 2.0f) - 1.0f);
                float dampingAmt  = juce::jlimit(0.001f, 0.95f, (detune + 50.0f) / 100.0f);
                // output
                float output = internalFilter.processSampleComb(audioInputSum, combFreq, feedbackAmt, dampingAmt);
                outputSample = std::isfinite(output) ? std::tanh(output) : 0.0f;
            }
            else if (filterType == 5) // Granular
            {
                // we assume keytracking so we can track scatter and grain size
                float granularFreq = baseFrequency + (modulationSum * 200.0f);
                granularFreq = juce::jlimit(20.0f, static_cast<float>(currentSampleRate) * 0.49f, granularFreq);
                // set floats for each variable to current knobs, scale knobs to scale required for vars
                float scatterAmt      = juce::jlimit(0.0f, 1.0f, phaseKnob / 360.0f);
                float grainDurationMs = juce::jmap((ratio - 0.01f) / (16.0f - 0.01f), 0.0f, 1.0f, 10.0f, 1000.0f);
                float feedbackAmt     = juce::jlimit(-0.95f, 0.95f, (foldKnob * 2.0f) - 1.0f);
                float dampingAmt      = juce::jlimit(0.001f, 0.95f, (detune + 50.0f) / 100.0f);
                // output
                float output = internalFilter.processSampleGranular(audioInputSum, granularFreq, scatterAmt, grainDurationMs, feedbackAmt, dampingAmt);
                outputSample = std::isfinite(output) ? std::tanh(output) : 0.0f;
            }
            else if (filterType == 6) // Formant
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
                // Drive (Fold knob): Pushes the input into a soft-clipper BEFORE the filter.
                float drive       = 1.0f + (foldKnob * 4.0f); // 1.0x to 5.0x drive
                float drivenInput = std::tanh(audioInputSum * drive);
                // output
                float output = internalFilter.processSampleFormant(drivenInput, dynamicVowel, qFactor);
                outputSample = std::isfinite(output) ? std::tanh(output) : 0.0f;
            }
            else if (filterType == 7) // Tape
            {
                float wobbleRate = (ratio - 0.01f) / (16.0f - 0.01f);
                float age        = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float saturation = phaseKnob / 360.0f;
                float bias       = juce::jlimit (0.0f, 1.0f, foldKnob); // 0.5 = optimal
                float output = internalFilter.processSampleTape (audioInputSum, wobbleRate, age,
                                                                  saturation, bias, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (filterType == 8) // Bitcrush
            {
                float bits   = (ratio - 0.01f) / (16.0f - 0.01f);
                float rate   = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float jitter = phaseKnob / 360.0f;
                float noise  = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output = internalFilter.processSampleBitcrush (audioInputSum, bits, rate,
                                                                      jitter, noise, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (filterType == 9) // Allpass Delay
            {
                float time      = (ratio - 0.01f) / (16.0f - 0.01f);
                float feedback  = juce::jlimit (0.0f, 0.95f, (detune + 50.0f) / 100.0f);
                float diffusion = phaseKnob / 360.0f;
                float damping   = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output = internalFilter.processSampleAllpassDelay (audioInputSum, time, feedback,
                                                                          diffusion, damping, currentSampleRate);
                outputSample = std::isfinite (output) ? std::tanh (output) : 0.0f;
            }
            else if (filterType == 10) // Allpass Reverb
            {
                float size      = (ratio - 0.01f) / (16.0f - 0.01f);
                float decay     = juce::jlimit (0.0f, 0.98f, (detune + 50.0f) / 100.0f);
                float diffusion = phaseKnob / 360.0f;
                float damping   = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output = internalFilter.processSampleAllpassReverb (audioInputSum, size, decay,
                                                                           diffusion, damping, currentSampleRate);
                outputSample = std::isfinite (output) ? std::tanh (output) : 0.0f;
            }
            else if (filterType == 11) // Compressor
            {
                float threshold = (ratio - 0.01f) / (16.0f - 0.01f);
                float compRatio = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float attack    = phaseKnob / 360.0f;
                float release   = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output = internalFilter.processSampleCompressor (audioInputSum, threshold,
                                                                        compRatio, attack, release,
                                                                        currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (filterType == 12) // Varispeed
            {
                float speed        = (ratio - 0.01f) / (16.0f - 0.01f);
                float acceleration = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float depth        = phaseKnob / 360.0f;
                float mode         = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output = internalFilter.processSampleVarispeed (audioInputSum, speed, acceleration,
                                                                       depth, mode, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (filterType == 13) // Scatter
            {
                float pattern = (ratio - 0.01f) / (16.0f - 0.01f);
                float size    = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float speed   = phaseKnob / 360.0f;
                float depth   = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output = internalFilter.processSampleScatter (audioInputSum, pattern, size,
                                                                     speed, depth, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (filterType == 14) // Ring Modulator
            {
                // Frequency: ratio knob maps to 0.1Hz - 5000Hz exponentially
                float normalizedRatio = (ratio - 0.01f) / (16.0f - 0.01f);
                float frequency       = 0.1f * std::pow (50000.0f, normalizedRatio);
                float shape           = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float depth           = phaseKnob / 360.0f;
                float feedback        = juce::jlimit (0.0f, 0.95f, foldKnob);
                float output = internalFilter.processSampleRingMod (audioInputSum, frequency, shape,
                                                                     depth, feedback, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (filterType == 15) // Chorus
            {
                float rate   = (ratio - 0.01f) / (16.0f - 0.01f);
                float depth  = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float spread = phaseKnob / 360.0f;
                float voices = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output = internalFilter.processSampleChorus (audioInputSum, rate, depth,
                                                                    spread, voices, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (filterType == 16) // Distortion
            {
                float drive       = (ratio - 0.01f) / (16.0f - 0.01f);
                float flavor      = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float toneKnob    = phaseKnob / 360.0f;
                float degradation = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output = internalFilter.processSampleDistortion (audioInputSum, drive, flavor,
                                                                        toneKnob, degradation,
                                                                        currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
	    else if (filterType == 17) // DJFX Delay
            {
                float bufferAmt = (ratio - 0.01f) / (16.0f - 0.01f);
                float speed     = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float on        = phaseKnob / 360.0f;
                float drift     = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output = internalFilter.processSampleDJFXDelay (audioInputSum, bufferAmt, speed,
                                                                       on, drift, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (filterType == 18) // Harmonic Resonator
            {
                float root       = (ratio - 0.01f) / (16.0f - 0.01f);
                float scaleKnob  = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float brightness = phaseKnob / 360.0f;
                float resonDepth = juce::jlimit (0.0f, 1.0f, foldKnob);
                float output = internalFilter.processSampleHarmonicResonator (audioInputSum, root,
                                                                                scaleKnob, brightness,
                                                                                resonDepth, currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (filterType == 19) // Ambient Delay
            {
                float time      = (ratio - 0.01f) / (16.0f - 0.01f);
                float feedbk    = juce::jlimit (0.0f, 0.98f, (detune + 50.0f) / 100.0f);
                float shimmerAmt = phaseKnob / 360.0f;
                float diffusion  = juce::jlimit (0.0f, 1.0f, foldKnob);
            
                float output = internalFilter.processSampleAmbientDelay (audioInputSum, time, feedbk,
                                                                          shimmerAmt, diffusion,
                                                                          currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
            else if (filterType == 20) // Old Chorus
            {
                float rate    = (ratio - 0.01f) / (16.0f - 0.01f);
                float depth   = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                float modeKnob = phaseKnob / 360.0f;
                float warmth  = juce::jlimit (0.0f, 1.0f, foldKnob);
            
                float output = internalFilter.processSampleOldChorus (audioInputSum, rate, depth,
                                                                       modeKnob, warmth,
                                                                       currentSampleRate);
                outputSample = std::isfinite (output) ? output : 0.0f;
            }
	}
        else // Here are our oscillators, Wave and Additive. They all need these:
        {
	    // keytracking, fm, and making sure that you can sync the osc speed to DAW speed
            float nodeTargetFrequency = isSynced ? currentBpm / 60.0f : baseFrequency;
            nodeTargetFrequency *= ratio;
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

                for (int k = 1; k <= numPartials; ++k) //Process the partials to create output
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
		//output
                rawSample    = (gainCompensation > 0.0f) ? additiveSum / gainCompensation : 0.0f;
                outputSample = std::tanh(rawSample + audioInputSum);
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
		    case 6:  rawSample = random.nextFloat() * 2.0f - 1.0f;         break; //white noise
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
                    default: rawSample = waveTable->lookupSine     (wrappedPhase); break;
                }

                float finalFoldDepth = juce::jlimit(0.0f, 1.0f, foldKnob + foldModOffset);
                if (finalFoldDepth > 0.001f)
                {
                    float drive        = 1.0f + (finalFoldDepth * 5.0f);
                    float foldedSample = std::sin(rawSample * drive) * (1.0f / std::sqrt(drive));
                    outputSample       = std::tanh(foldedSample + audioInputSum);
                }
                else
                {
                    outputSample = std::tanh(rawSample + audioInputSum);
                }
            }
        }

        return outputSample * envelope.getNextSample();
    }

private:
    float externalAudioL = 0.0f;
    float externalAudioR = 0.0f;
    WaveTable* waveTable = nullptr;
    double phase = 0.0;
    double currentSampleRate = 44100.0;
    int oversamplingFactor = 1;
    juce::ADSR envelope;
    juce::Random random; //white noise
    float pinkB0 = 0.0f, pinkB1 = 0.0f, pinkB2 = 0.0f;
    float pinkB3 = 0.0f, pinkB4 = 0.0f, pinkB5 = 0.0f, pinkB6 = 0.0f;
    float polyBlep(float t, float dt)
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
    SynthFilter internalFilter;
};
