// SynthFilter.h
#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <vector>

struct Grain {
    float position;
    float phase;
    float durationInSamples; 
    bool  isActive;
};

struct Vowel { float f1, f2, f3; };
static constexpr Vowel VOWEL_TABLE[5] = {
    { 730.0f, 1090.0f, 2440.0f }, // 0.0 -> A
    { 440.0f, 1800.0f, 2500.0f }, // 1.0 -> E
    { 290.0f, 2290.0f, 3010.0f }, // 2.0 -> I
    { 450.0f,  800.0f, 2830.0f }, // 3.0 -> O
    { 300.0f,  870.0f, 2240.0f }  // 4.0 -> U
};

class SynthFilter
{
public:
    enum FilterType { Lowpass = 0, Highpass, Bandpass, Comb, Granular, Formant };
    
    SynthFilter() 
    {
        grains.resize(maxGrains, {0.0f, 0.0f, 0.0f, false});
        reset(); 
    }
    virtual ~SynthFilter() = default; 

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        updateCoefficients();

        // 10 Hz requires a huge buffer. Added margin for Hermite/Scatter math.
        int requiredBufferSize = static_cast<int>(newSampleRate / 10.0) + 10;
        combBuffer.assign (static_cast<size_t>(requiredBufferSize), 0.0f);
        writePtr = 0;

        for (auto& g : grains) {
            g.isActive = false;
        }
    }

    void setSampleRate (double newSampleRate) 
    { 
        prepare (newSampleRate); 
    }

    void setCutoff (float cutoffHz)
    {
        float maxCutoff = static_cast<float>(sampleRate) * 0.49f;
        targetCutoff = juce::jlimit (20.0f, juce::jmin (20000.0f, maxCutoff), cutoffHz);
        updateCoefficients();
    }

    void setResonance (float resonanceQ)
    {
        targetResonance = juce::jlimit (0.1f, 10.0f, resonanceQ);
        updateCoefficients();
    }

    void setType (int typeIndex)
    {
        currentType = static_cast<FilterType>(juce::jlimit (0, 5, typeIndex));
    }

    float getPrecalculatedK() const noexcept
    {
        return 1.0f / targetResonance;
    }

    void reset() // Reset all effect buffers
    {
        s1 = 0.0f;
        s2 = 0.0f;
        for (int i = 0; i < 3; ++i) { //formant buffer reset
            f_s1[i] = 0.0f;
            f_s2[i] = 0.0f;
        }
        lastCombDamping = 0.0f; //comb
        
        if (!combBuffer.empty())
            std::fill (combBuffer.begin(), combBuffer.end(), 0.0f);
            
        writePtr = 0;

        for (auto& g : grains) {
            g.isActive = false; //granular
        }
        //tape
        tapePhase    = 0.0f;
        tapeWritePtr = 0;
        tapeReadPtr  = 0.0f;
        tapeLastSample = 0.0f;
        std::fill (std::begin (tapeDelayBuffer), std::end (tapeDelayBuffer), 0.0f);
        //bitcrush
        crushHeld    = 0.0f;
        crushCounter = 0;
        // Allpass delay reset
        apDelayBuffer.fill (0.0f);
        apDelayWritePtr   = 0;
        apDelayLastSample = 0.0f;
        for (int i = 0; i < numApStages; ++i)
        {
            apStageBuffers[i].fill (0.0f);
            apStageWritePtrs[i] = 0;
        }
        
        // Allpass reverb reset
        apRevBuffer.fill (0.0f);
        apRevWritePtr   = 0;
        apRevLastSample = 0.0f;
        for (int i = 0; i < numRevStages; ++i)
        {
            apRevStageBuffers[i].fill (0.0f);
            apRevStageWritePtrs[i] = 0;
        }
        // compressor
        compEnvelope = 0.0f;
        compGainDb   = 0.0f;
        // varispeed
        std::fill (std::begin (varispeedBuffer), std::end (varispeedBuffer), 0.0f);
        varispeedWritePtr    = 0;
        varispeedReadPtr     = 0.0f;
        varispeedCurrentSpeed = 1.0f;
        // scatter
        scatterBuffer.fill (0.0f);
        scatterWritePtr    = 0;
        scatterReadPtr     = 0.0f;
        scatterPattern     = 0;
        scatterPhase       = 0;
        scatterCycleLength = 0;
        scatterActive      = false;
        scatterSpeed       = 1.0f;
        // ring mod
        ringPhase    = 0.0f;
        ringFeedback = 0.0f;
        // chorus
        chorusDelayBuffer.fill (0.0f);
        chorusWritePtr = 0;
        chorusLFOPhases.fill (0.0f);
        // phaser
        phaserAllpassState.fill (0.0f);
        phaserLFOPhase = 0.0f;
        phaserFeedback = 0.0f;
        // distortion
        distToneState  = 0.0f;
        distLastSign   = 1.0f;
        distGlitchCount = 0;
        // djfx
        djfxBuffer.fill (0.0f);
        djfxWritePtr    = 0;
        djfxReadPtr     = 0.0f;
        djfxCapturing   = false;
        djfxCaptureSize  = 0;
        djfxCaptureCount = 0;
        djfxDriftPhase   = 0.0f;
        // color resonator
        resS1.fill (0.0f);
        resS2.fill (0.0f);
    }

    void noteStarted()
    {
        std::fill(combBuffer.begin(), combBuffer.end(), 0.0f);
        writePtr = 0;
        lastCombDamping = 0.0f;

        // For formant
        for (int i = 0; i < 3; ++i) {
            f_s1[i] = 0.0f;
            f_s2[i] = 0.0f;
        }

        for (auto& g : grains) {
            g.isActive = false;
        }
    }

    // Single allpass filter stage — the core building block for the delay/reverb
    template <size_t BufferSize>
    float processAllpass (float input, float coeff,
                          std::array<float, BufferSize>& buffer,
                          int& writePtr)
    {
        int size     = static_cast<int> (BufferSize);
        int readPtr  = (writePtr - static_cast<int> (coeff * size) + size) % size;
        float delayed = buffer[readPtr];
        float output  = -input + delayed;
        buffer[writePtr] = input + delayed * coeff;
        writePtr = (writePtr + 1) % size;
        return output;
    }

    // Standard Block Rate SVF
    virtual float processSample (float input)
    {
        float hp = (input - (g + k) * s1 - s2) * h;
        float bp = g * hp + s1;
        float lp = g * bp + s2;

        s1 = 2.0f * bp - s1;
        s2 = 2.0f * lp - s2;

        if (std::isnan(s1) || std::isnan(s2) || std::isinf(s1) || std::isinf(s2))
        {
            reset(); 
            return 0.0f;
        }

        switch (currentType)
        {
            case Highpass: return hp;
            case Bandpass: return bp;
            case Lowpass:  
            default:       return lp;
        }
    }

    // Fast Audio-Rate modulated SVF
    virtual float processSampleAudioRate (float input, float cutoffHz, float precalculatedK)
    {
        float x = 3.1415926535f * cutoffHz / static_cast<float>(sampleRate);
        float x2 = x * x;
        
        float g_mod = x * (945.0f - 105.0f * x2 + x2 * x2) / (945.0f - 420.0f * x2 + 15.0f * x2 * x2);
        float h_mod = 1.0f / (1.0f + g_mod * (g_mod + precalculatedK));
    
        float hp = (input - (g_mod + precalculatedK) * s1 - s2) * h_mod;
        float bp = g_mod * hp + s1;
        float lp = g_mod * bp + s2;
    
        s1 = 2.0f * bp - s1;
        s2 = 2.0f * lp - s2;
    
        if (std::isnan(s1) || std::isinf(s1)) 
        { 
            reset(); 
            return 0.0f; 
        }
    
        switch (currentType)
        {
            case Highpass: return hp;
            case Bandpass: return bp;
            case Lowpass:  
            default:       return lp;
        }
    }

    // Formant Filter
    float processSampleFormant(float input, float morphIndex, float qFactor)
    {
        // morphIndex: 0.0 to 4.0 maps to A-E-I-O-U
        float safeMorph = juce::jlimit(0.0f, 4.0f, morphIndex);
        int indexTarget = static_cast<int>(safeMorph);
        int indexNext = juce::jmin(4, indexTarget + 1);
        float frac = safeMorph - static_cast<float>(indexTarget);

        // Linear interpolation of frequencies
        float freqs[3];
        freqs[0] = VOWEL_TABLE[indexTarget].f1 + frac * (VOWEL_TABLE[indexNext].f1 - VOWEL_TABLE[indexTarget].f1);
        freqs[1] = VOWEL_TABLE[indexTarget].f2 + frac * (VOWEL_TABLE[indexNext].f2 - VOWEL_TABLE[indexTarget].f2);
        freqs[2] = VOWEL_TABLE[indexTarget].f3 + frac * (VOWEL_TABLE[indexNext].f3 - VOWEL_TABLE[indexTarget].f3);

        float outputAccumulator = 0.0f;
        float precalcK = 1.0f / juce::jmax(0.1f, qFactor); // Needs high Q, usually pass 5.0 to 15.0 here

        // Run 3 parallel SVF Bandpasses using the fast polynomial approximation
        for (int i = 0; i < 3; ++i)
        {
            float x = juce::MathConstants<float>::pi * freqs[i] / static_cast<float>(sampleRate);
            float x2 = x * x;
            float g_mod = x * (945.0f - 105.0f * x2 + x2 * x2) / (945.0f - 420.0f * x2 + 15.0f * x2 * x2);
            float h_mod = 1.0f / (1.0f + g_mod * (g_mod + precalcK));

            float hp = (input - (g_mod + precalcK) * f_s1[i] - f_s2[i]) * h_mod;
            float bp = g_mod * hp + f_s1[i];
            float lp = g_mod * bp + f_s2[i];

            f_s1[i] = 2.0f * bp - f_s1[i];
            f_s2[i] = 2.0f * lp - f_s2[i];

            if (std::isnan(f_s1[i]) || std::isinf(f_s1[i])) 
            { 
                reset(); 
                return 0.0f; 
            }
            outputAccumulator += bp;
        }
        // Gain compensation for 3 parallel resonant peaks & soft clipper
        return std::tanh(outputAccumulator * 0.33f);
    }

    // Standard Comb Filter
    float processSampleComb (float input, float freqHz, float feedback, float dampingNormalized)
    {
        if (combBuffer.empty()) return input;
        int bufferSize = static_cast<int>(combBuffer.size());
        
        float delaySamples = static_cast<float>(sampleRate) / juce::jmax (10.0f, freqHz);
        float maxDelay = static_cast<float>(bufferSize - 4);
        delaySamples = juce::jlimit (1.0f, maxDelay, delaySamples);
        float readPtrVal = static_cast<float>(writePtr) - delaySamples;
        
        while (readPtrVal < 0.0f) readPtrVal += static_cast<float>(bufferSize);
        float delayedSample = hermiteInterp (combBuffer.data(), bufferSize, readPtrVal);
        
        lastCombDamping = (delayedSample * (1.0f - dampingNormalized)) + (lastCombDamping * dampingNormalized);
        float safeFeedback = juce::jlimit (-0.995f, 0.995f, feedback);
        float output = input + (lastCombDamping * safeFeedback);
        
        combBuffer[static_cast<size_t>(writePtr)] = std::tanh (output);
        writePtr = (writePtr + 1) % bufferSize;
        
        return output;
    }

    // Granular Resonator / Glitch Filter
    float processSampleGranular (float input, float freqHz, float scatterAmount, float grainDurationMs, float feedback, float dampingNormalized)
    {
        if (combBuffer.empty()) return input;
        
        int bSize = static_cast<int>(combBuffer.size());
        float baseDelaySamples = static_cast<float>(sampleRate) / juce::jmax(10.0f, freqHz);
        float maxDelay = static_cast<float>(bSize - 4);
        
        float outputAccumulator = 0.0f;
        int activeGrainCount = 0;

        for (auto& grain : grains)
        {
            // Stochastic spawning
            if (!grain.isActive && fastRandom() < 0.02f) 
            {
                grain.isActive = true;
                grain.phase = 0.0f;
                
                // Set duration safely
                float durationSamples = (grainDurationMs / 1000.0f) * static_cast<float>(sampleRate);
                grain.durationInSamples = juce::jmax(10.0f, durationSamples);
                
                // Jitter position based on scatter parameter
                float jitter = (fastRandom() * 2.0f - 1.0f) * scatterAmount * baseDelaySamples;
		float delaySamples = juce::jlimit(1.0f, maxDelay, baseDelaySamples + jitter);
                
                grain.position = static_cast<float>(writePtr) - delaySamples;
                while (grain.position < 0.0f) grain.position += static_cast<float>(bSize);
            }

            if (grain.isActive)
            {
                activeGrainCount++;
                
                // Moving read head for the grain
                float readPos = grain.position + grain.phase; 
                while (readPos >= bSize) readPos -= bSize;

                float sample = hermiteInterp (combBuffer.data(), bSize, readPos);
                
                // Hanning Window to prevent clicks
                float window = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * grain.phase / grain.durationInSamples));
                
                outputAccumulator += sample * window;
                
                // Advance phase
                grain.phase += 1.0f;
                if (grain.phase >= grain.durationInSamples)
                {
                    grain.isActive = false;
                }
            }
        }
        
        // Energy compensation to prevent loud spikes when many grains overlap
        if (activeGrainCount > 0)
        {
            outputAccumulator /= std::sqrt(static_cast<float>(activeGrainCount)); 
        }

        // Apply feedback and damping to write buffer
        lastCombDamping = (outputAccumulator * (1.0f - dampingNormalized)) + (lastCombDamping * dampingNormalized);
        float safeFeedback = juce::jlimit (-0.995f, 0.995f, feedback);
        float toBuffer = input + (lastCombDamping * safeFeedback);
        
        combBuffer[static_cast<size_t>(writePtr)] = std::tanh(toBuffer); // Soft-clip
        writePtr = (writePtr + 1) % bSize;
        
        return outputAccumulator + input; // Wet + Dry
    }

    float processSampleTape (float input, float wobbleRate, float age,
                         float saturation, float bias,
                         double sampleRate)
    {
        // 1. BIAS — affects how the tape magnetizes before saturation
        // bias: 0.0 = underbias (harsh/bright), 0.5 = optimal, 1.0 = overbias (dull/soft)
        float biasOffset  = (bias - 0.5f) * 2.0f;
        float biasedInput = input + biasOffset * std::abs (input) * input;
        biasedInput       = juce::jlimit (-1.0f, 1.0f, biasedInput);
        // Then saturation acts on biasedInput:
        float driveAmt  = 1.0f + saturation * 8.0f;
        float saturated = std::tanh (biasedInput * driveAmt) / std::tanh (driveAmt);
        // 3. WOW/FLUTTER — modulate delay time with an LFO
        float baseDelaySamples = static_cast<float> (sampleRate) * 0.005f; // 5ms base
    
        float wowRate    = wobbleRate * 2.0f;
        float wowAmt     = std::sin (tapePhase) * 0.6f;
        float flutterAmt = std::sin (tapePhase * 7.0f) * 0.4f;
        float wobble     = (wowAmt + flutterAmt) * wobbleRate;
    
        tapePhase += (wowRate * juce::MathConstants<float>::twoPi)
                     / static_cast<float> (sampleRate);
        if (tapePhase >= juce::MathConstants<float>::twoPi)
            tapePhase -= juce::MathConstants<float>::twoPi;
    
        int bufferSize = 8192;
        tapeDelayBuffer[tapeWritePtr] = saturated;
        tapeWritePtr = (tapeWritePtr + 1) % bufferSize;
    
        float delaySamples = juce::jlimit (1.0f, static_cast<float> (bufferSize - 4),
                                            baseDelaySamples + wobble * baseDelaySamples);
        tapeReadPtr = static_cast<float> (tapeWritePtr) - delaySamples;
        while (tapeReadPtr < 0.0f) tapeReadPtr += static_cast<float> (bufferSize);
        float output = hermiteInterp (tapeDelayBuffer, bufferSize, tapeReadPtr);
    
        // 4. AGE — one-pole low-pass for HF loss
        float ageCoeff = 1.0f - (age * 0.92f);
        output         = output * ageCoeff + tapeLastSample * (1.0f - ageCoeff);
        tapeLastSample = output;
    
        return std::isfinite (output) ? output : 0.0f;
    }

    float processSampleBitcrush (float input, float bits, float rate,
                                  float jitter, float noise,
                                  double sampleRate)
    {
        // sample rate reduction
        int downsampleFactor = juce::jlimit (1, 64,
                                   static_cast<int> (1.0f + rate * 63.0f));
    
        crushCounter++;
        if (crushCounter >= downsampleFactor)
        {
            crushCounter = 0;
            crushHeld    = input;
        }
        float downsampled = crushHeld;
    
        // jitter - clock instability and random sample timing
        if (jitter > 0.001f)
        {
            // Randomly retrigger the hold with probability based on jitter
            if (crushRandom.nextFloat() < jitter * 0.1f)
                crushHeld = input;
    
            // Add random timing offset to the held value
            float jitterAmt = (crushRandom.nextFloat() * 2.0f - 1.0f) * jitter * 0.05f;
            downsampled     = crushHeld + jitterAmt * crushHeld;
        }
    
        // bitdepth reduction
        float bitDepth = juce::jmap (bits, 0.0f, 1.0f, 1.0f, 16.0f);
        float levels   = std::pow (2.0f, bitDepth);
        float crushed  = std::round (downsampled * levels) / levels;
    
        // noise go brr
        if (noise > 0.001f)
        {
            float dither = (crushRandom.nextFloat() * 2.0f - 1.0f) * noise * (1.0f / levels);
            crushed     += dither;
            crushed      = juce::jlimit (-1.0f, 1.0f, crushed);
        }
    
        return crushed;
    }

    float processSampleAllpassDelay (float input, float time, float feedback,
                                      float diffusion, float damping,
                                      double sampleRate)
    {
        // cascade allpass stages to smear the echo
        float diffCoeff = diffusion * 0.7f;
        float diffused  = input;
        for (int i = 0; i < numApStages; ++i)
            diffused = processAllpass (diffused, diffCoeff,
                                       apStageBuffers[i], apStageWritePtrs[i]);
    
        // main delay line
        float delayMs      = juce::jmap (time, 0.0f, 1.0f, 10.0f, 1000.0f);
        int   delaySamples = juce::jlimit (1, apDelayBufferSize - 1,
                                 static_cast<int> (delayMs / 1000.0f * sampleRate));
    
        int   readPtr = (apDelayWritePtr - delaySamples + apDelayBufferSize) % apDelayBufferSize;
        float delayed = apDelayBuffer[readPtr];
    
        // damping — frequency-mapped one-pole LP in feedback path
        float dampCutoff  = juce::jmap (1.0f - damping, 0.0f, 1.0f, 20.0f, 20000.0f);
        float dampAlpha   = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi
                                             * dampCutoff / static_cast<float> (sampleRate));
        apDelayLastSample += dampAlpha * (delayed - apDelayLastSample);
    
        // feedback
	float feedbackAmt = juce::jlimit (0.0f, 0.95f, feedback);
        apDelayBuffer[apDelayWritePtr] = diffused + apDelayLastSample * feedbackAmt;
        apDelayWritePtr = (apDelayWritePtr + 1) % apDelayBufferSize;
    
        return delayed + input;
    }
    
    float processSampleAllpassReverb (float input, float size, float decay,
                                       float diffusion, float damping,
                                       double sampleRate)
    {
        // pre-diffusion ...
        float diffCoeff = juce::jlimit (0.0f, 0.75f, diffusion * 0.75f);
        float diffused  = input;
        for (int i = 0; i < numRevStages / 2; ++i)
            diffused = processAllpass (diffused, diffCoeff,
                                       apRevStageBuffers[i], apRevStageWritePtrs[i]);
    
        // reverb tank
        float sizeMs      = juce::jmap (size, 0.0f, 1.0f, 20.0f, 500.0f);
        int   tankSamples = juce::jlimit (1, apRevBufferSize - 1,
                                 static_cast<int> (sizeMs / 1000.0f * sampleRate));
    
        int   readPtr = (apRevWritePtr - tankSamples + apRevBufferSize) % apRevBufferSize;
        float tankOut = apRevBuffer[readPtr];
    
        // damping — frequency-mapped one-pole LP in feedback path
        float dampCutoff  = juce::jmap (1.0f - damping, 0.0f, 1.0f, 20.0f, 20000.0f);
        float dampAlpha   = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi
                                             * dampCutoff / static_cast<float> (sampleRate));
        apRevLastSample  += dampAlpha * (tankOut - apRevLastSample);
    
        // decay
        float decayAmt = juce::jlimit (0.0f, 0.93f, decay);
        apRevBuffer[apRevWritePtr] = diffused + apRevLastSample * decayAmt;
        apRevWritePtr = (apRevWritePtr + 1) % apRevBufferSize;
    
        // post-diffusion
        float output = tankOut;
        for (int i = numRevStages / 2; i < numRevStages; ++i)
            output = processAllpass (output, diffCoeff,
                                     apRevStageBuffers[i], apRevStageWritePtrs[i]);
    
        return output + input * 0.1f;
    }

    float processSampleCompressor (float input, float threshold, float ratio,
                                float attack, float release,
                                double sampleRate)
    {
        // threshold — map 0-1 to -40dB to 0dB
        float thresholdDb = juce::jmap (threshold, 0.0f, 1.0f, -40.0f, 0.0f);
    
        // envelope follower — tracks signal level
        float inputLevel = std::abs (input);
        float attackCoeff  = std::exp (-1.0f / (static_cast<float> (sampleRate)
                                       * juce::jmap (attack,   0.0f, 1.0f, 0.001f, 0.5f)));
        float releaseCoeff = std::exp (-1.0f / (static_cast<float> (sampleRate)
                                       * juce::jmap (release, 0.0f, 1.0f, 0.01f,  2.0f)));
    
        if (inputLevel > compEnvelope)
            compEnvelope = attackCoeff  * compEnvelope + (1.0f - attackCoeff)  * inputLevel;
        else
            compEnvelope = releaseCoeff * compEnvelope + (1.0f - releaseCoeff) * inputLevel;
    
        // compute gain
        float inputDb   = juce::Decibels::gainToDecibels (compEnvelope, -60.0f);
        float targetDb  = 0.0f;
    
        // map ratio knob 0-1 to compression ratio 1:1 to 20:1
        float compRatio = juce::jmap (ratio, 0.0f, 1.0f, 1.0f, 20.0f);
    
        if (inputDb > thresholdDb)
            targetDb = thresholdDb + (inputDb - thresholdDb) / compRatio;
        else
            targetDb = inputDb;
    
        float gainDb   = targetDb - inputDb;
        compGainDb     = gainDb; // store for metering if needed
    
        // apply gain
        float gain = juce::Decibels::decibelsToGain (gainDb);
        return input * gain;
    }

    float processSampleVarispeed (float input, float speed, float acceleration,
                                   float depth, float mode,
                                   double sampleRate)
    {
        // speed. mode: 0.0 = slow down, 0.5 = neutral, 1.0 = speed up
        float targetSpeed;
        if (mode < 0.45f)
            targetSpeed = juce::jmap (speed, 0.0f, 1.0f, 0.25f, 0.99f); // slow down
        else if (mode > 0.55f)
            targetSpeed = juce::jmap (speed, 0.0f, 1.0f, 1.01f, 4.0f);  // speed up
        else
            targetSpeed = 1.0f; // neutral passthrough
    
        // speed acceleration. depth controls how much the speed varies around the target
        float depthAmt    = depth * 0.5f;
        float accelCoeff  = std::exp (-1.0f / (static_cast<float> (sampleRate)
                                      * juce::jmap (acceleration, 0.0f, 1.0f, 0.001f, 2.0f)));
    
        // Apply depth as subtle speed modulation around target
        float speedMod       = std::sin (varispeedReadPtr * 0.0001f) * depthAmt;
        float modulatedSpeed = targetSpeed + speedMod;
        modulatedSpeed       = juce::jlimit (0.1f, 4.0f, modulatedSpeed);
    
        // Smooth speed changes with acceleration coefficient
        varispeedCurrentSpeed = accelCoeff * varispeedCurrentSpeed 
                                + (1.0f - accelCoeff) * modulatedSpeed;
    
        int bufferSize = 96000;
        varispeedBuffer[varispeedWritePtr] = input;
        varispeedWritePtr = (varispeedWritePtr + 1) % bufferSize;
    
        // read at modded speed with hermite interpolation
	varispeedReadPtr += varispeedCurrentSpeed;
        while (varispeedReadPtr >= static_cast<float> (bufferSize))
            varispeedReadPtr -= static_cast<float> (bufferSize);
        while (varispeedReadPtr < 0.0f)
            varispeedReadPtr += static_cast<float> (bufferSize);
        float output = hermiteInterp (varispeedBuffer, bufferSize, varispeedReadPtr);
        return std::isfinite (output) ? output : 0.0f;
    }

    float processSampleScatter (float input, float pattern, float size,
                                 float speed, float depth,
                                 double sampleRate)
    {
        // write input to buffer
        scatterBuffer[scatterWritePtr] = input;
        scatterWritePtr = (scatterWritePtr + 1) % scatterBufferSize;
    
        // may have to get this changed. for now:
        // Pattern 0.0-0.25: Stutter (repeat short segment)
        // Pattern 0.25-0.5: Reverse (play segment backwards)
        // Pattern 0.5-0.75: Skip (jump forward in buffer)
        // Pattern 0.75-1.0: Loop (loop a segment at different speed)
        // -------------------------------------------------------
        int patternIdx = juce::jlimit (0, 3, static_cast<int> (pattern * 4.0f));
    
        // Size: 0.0 = very short (10ms), 1.0 = long (500ms)
        float sizeMs      = juce::jmap (size, 0.0f, 1.0f, 10.0f, 500.0f);
        int   segmentSize = juce::jlimit (1, scatterBufferSize - 1,
                                static_cast<int> (sizeMs / 1000.0f * sampleRate));
    
        // Speed: 0.0 = half speed, 0.5 = normal, 1.0 = double speed
        float playbackSpeed = juce::jmap (speed, 0.0f, 1.0f, 0.5f, 2.0f);
    
        // Update cycle length based on segment size
        scatterCycleLength = segmentSize;
        scatterPhase       = (scatterPhase + 1) % scatterCycleLength;
    
        // computer read pos based on pattern
	float readPos = 0.0f;
    
        switch (patternIdx)
        {
            case 0: // Stutter — repeat the same segment
            {
                float segStart = static_cast<float> (scatterWritePtr) - segmentSize;
                readPos = segStart + static_cast<float> (scatterPhase) * playbackSpeed;
                break;
            }
            case 1: // Reverse — play segment backwards
            {
                float segStart = static_cast<float> (scatterWritePtr) - segmentSize;
                readPos = segStart + static_cast<float> (scatterCycleLength - scatterPhase) * playbackSpeed;
                break;
            }
            case 2: // Skip — jump to a random position each cycle
            {
                if (scatterPhase == 0)
                {
                    // Pick a new random position at the start of each cycle
                    int maxLookback = juce::jmin (scatterBufferSize - 1, segmentSize * 4);
                    scatterReadPtr  = static_cast<float> (scatterWritePtr)
                                      - static_cast<float> (scatterRandom.nextInt (maxLookback) + segmentSize);
                }
                readPos = scatterReadPtr + static_cast<float> (scatterPhase) * playbackSpeed;
                break;
            }
            case 3: // Loop — loop segment at playback speed
            {
                float segStart = static_cast<float> (scatterWritePtr) - segmentSize;
                float loopPos  = std::fmod (static_cast<float> (scatterPhase) * playbackSpeed,
                                            static_cast<float> (segmentSize));
                readPos = segStart + loopPos;
                break;
            }
        }
    
        // Wrap read position into valid buffer range
        while (readPos < 0.0f)
            readPos += static_cast<float> (scatterBufferSize);
        while (readPos >= static_cast<float> (scatterBufferSize))
            readPos -= static_cast<float> (scatterBufferSize);
    
        float scattered = hermiteInterp (scatterBuffer.data(), scatterBufferSize, readPos);
 
        // hanning window to prevent clicks at segment boundaries
        float window = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi
                                                 * static_cast<float> (scatterPhase)
                                                 / static_cast<float> (scatterCycleLength)));
        scattered *= window;
    
        // depth mix — blend scattered with dry
        return scattered * depth + input * (1.0f - depth);
    }

    float processSampleRingMod (float input, float frequency, float shape,
                                 float depth, float feedback,
                                 double sampleRate)
    {
        // advance osc phase
        float phaseIncrement = (frequency * juce::MathConstants<float>::twoPi)
                               / static_cast<float> (sampleRate);
        ringPhase += phaseIncrement;
        if (ringPhase >= juce::MathConstants<float>::twoPi)
            ringPhase -= juce::MathConstants<float>::twoPi;
    
        // Shape 0.0-0.33: Sine
        // Shape 0.33-0.66: Saw
        // Shape 0.66-1.0: Square
        float carrier = 0.0f;
        if (shape < 0.33f)
        {
            // Sine
            carrier = std::sin (ringPhase);
        }
        else if (shape < 0.66f)
        {
            // Saw — blend from sine to saw
            float blend = (shape - 0.33f) / 0.33f;
            float sine  = std::sin (ringPhase);
            float saw   = (ringPhase / juce::MathConstants<float>::pi) - 1.0f;
            carrier     = sine * (1.0f - blend) + saw * blend;
        }
        else
        {
            // Square — blend from saw to square
            float blend  = (shape - 0.66f) / 0.34f;
            float saw    = (ringPhase / juce::MathConstants<float>::pi) - 1.0f;
            float square = ringPhase < juce::MathConstants<float>::pi ? 1.0f : -1.0f;
            carrier      = saw * (1.0f - blend) + square * blend;
        }
    
        // feedback - feed output back into input for extra harmonics
        float feedbackAmt = juce::jlimit (0.0f, 0.95f, feedback);
        float modInput    = input + ringFeedback * feedbackAmt;
    
        // ringmod — multiply input by carrier
        float modulated = modInput * carrier;
        ringFeedback    = modulated;
    
        // depth
        return modulated * depth + input * (1.0f - depth);
    }

    float processSampleChorus (float input, float rate, float depth,
                                float spread, float voices,
                                double sampleRate)
    {
        // write to delay buffer
        chorusDelayBuffer[chorusWritePtr] = input;
        chorusWritePtr = (chorusWritePtr + 1) % chorusDelaySize;
    
        // voice#
        int numVoices = juce::jlimit (1, numChorusVoices,
                           static_cast<int> (1.0f + voices * 3.0f)); // 1-4 voices
    
        // base parameters
	// Rate: 0.0 = 0.1Hz, 1.0 = 5Hz
        float lfoRate   = 0.1f + rate * 4.9f;
    
        // Depth: controls delay modulation range (1ms - 20ms)
        float depthMs   = 1.0f + depth * 19.0f;
        float depthSamples = depthMs / 1000.0f * static_cast<float> (sampleRate);
    
        // Base delay: center of modulation (5ms - 30ms)
        float baseDelayMs      = 5.0f + depth * 25.0f;
        float baseDelaySamples = baseDelayMs / 1000.0f * static_cast<float> (sampleRate);
    
        // accumulate voices ...
	float output = 0.0f;
    
        for (int v = 0; v < numVoices; ++v)
        {
            // Each voice has a phase offset based on spread
            float phaseOffset = spread * juce::MathConstants<float>::twoPi
                                * static_cast<float> (v) / static_cast<float> (numVoices);
    
            // Advance LFO
            float lfoPhaseIncrement = lfoRate * juce::MathConstants<float>::twoPi
                                      / static_cast<float> (sampleRate);
            chorusLFOPhases[v] += lfoPhaseIncrement;
            if (chorusLFOPhases[v] >= juce::MathConstants<float>::twoPi)
                chorusLFOPhases[v] -= juce::MathConstants<float>::twoPi;
    
            // LFO value with voice phase offset
            float lfo = std::sin (chorusLFOPhases[v] + phaseOffset);
    
            // Modulated delay time
            float delaySamples = juce::jlimit (1.0f, static_cast<float> (chorusDelaySize - 4),
                                     baseDelaySamples + lfo * depthSamples);
    
            // Read position
            float readPos = static_cast<float> (chorusWritePtr) - delaySamples;
            while (readPos < 0.0f) readPos += static_cast<float> (chorusDelaySize);
            output += hermiteInterp (chorusDelayBuffer.data(), chorusDelaySize, readPos);
        }
    
        // Normalize by voice count
        output /= static_cast<float> (numVoices);
    
        // Mix with dry signal
        return output * 0.7f + input * 0.3f;
    }

float processSamplePhaser (float input, float rate, float depth,
                            float stages, float feedback,
                            double sampleRate)
    {
        // lfo
        float lfoRate = 0.05f + rate * 4.95f; // 0.05Hz to 5Hz
        phaserLFOPhase += (lfoRate * juce::MathConstants<float>::twoPi)
                          / static_cast<float> (sampleRate);
        if (phaserLFOPhase >= juce::MathConstants<float>::twoPi)
            phaserLFOPhase -= juce::MathConstants<float>::twoPi;
    
        float lfo = (std::sin (phaserLFOPhase) + 1.0f) * 0.5f; // 0.0 to 1.0
    
        // 2. MODULATED ALLPASS COEFFICIENT
        // depth controls how wide the notch sweeps
        // 0.0 = subtle, 1.0 = full sweep
        float minFreq  = 200.0f;
        float maxFreq  = juce::jmap (depth, 0.0f, 1.0f, 800.0f, 8000.0f);
        float sweepFreq = minFreq + lfo * (maxFreq - minFreq);
    
        // Convert frequency to allpass coefficient
        float w = std::tan (juce::MathConstants<float>::pi * sweepFreq
                            / static_cast<float> (sampleRate));
        float coeff = (w - 1.0f) / (w + 1.0f);
    
        // 3. NUMBER OF STAGES
        // stages 0.0-1.0 maps to 2, 4, 6, 8 stages
        int numStages = 2 + (static_cast<int> (stages * 3.0f) * 2);
        numStages     = juce::jlimit (2, numPhaserStages, numStages);
    
        // feedback — feed output back into input
        float feedbackAmt = juce::jlimit (-0.95f, 0.95f, (feedback * 2.0f) - 1.0f);
        float inputWithFeedback = input + phaserFeedback * feedbackAmt;
    
        // 5. CASCADE ALLPASS STAGES
        float output = inputWithFeedback;
        for (int i = 0; i < numStages; ++i)
        {
            float delayed        = phaserAllpassState[i];
            phaserAllpassState[i] = output + delayed * (-coeff);
            output               = delayed + output * coeff;
        }
    
        phaserFeedback = output;
    
        // 6. MIX — classic phaser is 50% dry + 50% wet
        return (input + output) * 0.5f;
    }

    float processSampleDistortion (float input, float drive, float flavor,
                                    float tone, float degradation,
                                    double sampleRate)
    {
        // drive — input gain
        float driveAmt = 1.0f + drive * 40.0f; // 1x to 41x
        float driven   = input * driveAmt;
    
        // 2. FLAVOR — morphs between distortion types
        // 0.0-0.25: Soft clip (tanh)
        // 0.25-0.5: Hard clip
        // 0.5-0.75: Foldback
        // 0.75-1.0: Digital (sign function / bit mangle)
        float distorted = 0.0f;
    
        if (flavor < 0.25f)
        {
            // Soft clip — smooth tanh saturation
            float blend   = flavor / 0.25f;
            float soft    = std::tanh (driven);
            float harder  = std::tanh (driven * 2.0f) * 0.5f;
            distorted     = soft * (1.0f - blend) + harder * blend;
        }
        else if (flavor < 0.5f)
        {
            // Hard clip
            float blend   = (flavor - 0.25f) / 0.25f;
            float soft    = std::tanh (driven * 2.0f) * 0.5f;
            float hard    = juce::jlimit (-1.0f, 1.0f, driven);
            distorted     = soft * (1.0f - blend) + hard * blend;
        }
        else if (flavor < 0.75f)
        {
            // Foldback distortion
            float blend   = (flavor - 0.5f) / 0.25f;
            float hard    = juce::jlimit (-1.0f, 1.0f, driven);
            float folded  = driven;
            // Fold the signal back when it exceeds -1/+1
            while (folded > 1.0f)  folded = 2.0f  - folded;
            while (folded < -1.0f) folded = -2.0f - folded;
            distorted     = hard * (1.0f - blend) + folded * blend;
        }
        else
        {
            // Digital — blend from foldback to sign-function harshness
            float blend   = (flavor - 0.75f) / 0.25f;
            float folded  = driven;
            while (folded > 1.0f)  folded = 2.0f  - folded;
            while (folded < -1.0f) folded = -2.0f - folded;
            // Sign function with drive — very harsh digital sound
            float digital = driven >= 0.0f ? 1.0f : -1.0f;
            digital      *= juce::jlimit (0.0f, 1.0f, std::abs (driven));
            distorted     = folded * (1.0f - blend) + digital * blend;
        }
    
        // 3. DEGRADATION — digital artifacts
        // Three layers: zero-crossing glitch, sample mangling, 
        // and random dropout
        if (degradation > 0.001f)
        {
            // Layer 1: Zero-crossing glitch — freezes signal at crossings
            float currentSign = distorted >= 0.0f ? 1.0f : -1.0f;
            if (currentSign != distLastSign)
            {
                distGlitchCount++;
                if (distRandom.nextFloat() < degradation * 0.3f)
                {
                    // Freeze at zero for a few samples
                    distorted = distorted * (1.0f - degradation * 0.5f);
                }
            }
            distLastSign = currentSign;
    
            // Layer 2: Sample mangling — randomly replace samples with
            // held or inverted values
            if (distRandom.nextFloat() < degradation * 0.05f)
            {
                // Randomly invert, zero, or square the sample
                int manglerType = distRandom.nextInt (3);
                if      (manglerType == 0) distorted = -distorted;
                else if (manglerType == 1) distorted = 0.0f;
                else                       distorted = distorted >= 0.0f ? 1.0f : -1.0f;
            }
    
            // Layer 3: Random dropout — brief silences
            if (distRandom.nextFloat() < degradation * 0.02f)
                distorted = 0.0f;
        }
    
        // 4. TONE — post-distortion one-pole filter
        // 0.0 = dark (heavy low-pass), 0.5 = neutral, 1.0 = bright
        float toneCutoff = 200.0f + tone * 19800.0f; // 200Hz to 20kHz
        float toneAlpha  = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi
                                            * toneCutoff / static_cast<float> (sampleRate));
        distToneState   += toneAlpha * (distorted - distToneState);
    
        // Blend between low-passed (dark) and original (bright)
        float toned = distToneState * (1.0f - tone) + distorted * tone;
    
        // 5. COMPENSATE GAIN — normalize output level
        float gainCompensation = 1.0f / std::sqrt (driveAmt);
        return juce::jlimit (-1.0f, 1.0f, toned * gainCompensation);
    }

    float processSampleDJFXDelay (float input, float bufferAmt, float speed,
                                   float on, float drift,
                                   double sampleRate)
    {
        // 1. ON/OFF — below 0.5 passes dry signal through
        if (on < 0.5f)
        {
            // Still write to buffer so it's ready when engaged
            djfxBuffer[djfxWritePtr] = input;
            djfxWritePtr = (djfxWritePtr + 1) % djfxBufferSize;
            djfxReadPtr  = static_cast<float> (djfxWritePtr);
            return input;
        }
    
        // 2. BUFFER SIZE — how much audio is captured
        float bufferMs    = juce::jmap (bufferAmt, 0.0f, 1.0f, 10.0f, 2000.0f);
        int   captureSize = juce::jlimit (1, djfxBufferSize - 1,
                                static_cast<int> (bufferMs / 1000.0f * sampleRate));
    
        // Write input to buffer continuously
        djfxBuffer[djfxWritePtr] = input;
        djfxWritePtr = (djfxWritePtr + 1) % djfxBufferSize;
    
        // 3. SPEED — maps -100 to +100
        // -100 = full reverse, 0 = stopped, 100 = normal speed
        float normalizedSpeed = speed * 2.0f - 1.0f; // 0-1 knob to -1 to +1
        float playbackSpeed   = normalizedSpeed;      // -1.0 to 1.0
    
        // 4. DRIFT — random pitch instability
        if (drift > 0.001f)
        {
            djfxDriftPhase += 0.003f; // slow drift LFO
            if (djfxDriftPhase >= juce::MathConstants<float>::twoPi)
                djfxDriftPhase -= juce::MathConstants<float>::twoPi;
    
            float driftLFO    = std::sin (djfxDriftPhase) * drift * 0.15f;
            float driftRandom = (djfxRandom.nextFloat() * 2.0f - 1.0f) * drift * 0.05f;
            playbackSpeed    += driftLFO + driftRandom;
        }
    
        // 5. ADVANCE READ POINTER
        djfxReadPtr += playbackSpeed;
    
        // Keep read pointer within capture window
        float rawCaptureStart = static_cast<float> (djfxWritePtr - captureSize + djfxBufferSize);
        float captureStart = std::fmodf (rawCaptureStart, static_cast<float> (djfxBufferSize));
    
        // Wrap within the capture window rather than the whole buffer
        while (djfxReadPtr >= captureStart + captureSize)
            djfxReadPtr -= captureSize;
        while (djfxReadPtr < captureStart)
            djfxReadPtr += captureSize;
    
        // Keep in valid buffer range
        while (djfxReadPtr >= static_cast<float> (djfxBufferSize))
            djfxReadPtr -= static_cast<float> (djfxBufferSize);
        while (djfxReadPtr < 0.0f)
            djfxReadPtr += static_cast<float> (djfxBufferSize);
    
        float output = hermiteInterp (djfxBuffer.data(), djfxBufferSize, djfxReadPtr);
        return std::isfinite (output) ? output : 0.0f;
    }

    float processSampleHarmonicResonator (float input, float root, float scale,
                                           float brightness, float depth,
                                           double sampleRate)
    {
        // 1. ROOT NOTE — maps 0-1 to 12 semitones (A=0 to G#=1)
        // Base frequency derived from root note, centered around A3 (220Hz)
        int   rootSemitone = juce::jlimit (0, 11, static_cast<int> (root * 12.0f));
        float rootFreq     = 220.0f * std::pow (2.0f, rootSemitone / 12.0f);
    
        // 2. SCALE — interval patterns for each scale type
        // Each array contains semitone intervals above the root
        const int scaleIntervals[5][7] = {
            { 0, 2, 4, 7, 9, 11, 12 }, // Major
            { 0, 2, 3, 7, 9, 10, 12 }, // Minor
            { 0, 2, 4, 7, 9,  0,  0 }, // Pentatonic (5 notes)
            { 0, 2, 4, 6, 8, 10, 12 }, // Whole Tone
            { 0, 1, 2, 3, 4,  5,  6 }, // Chromatic (first 7)
        };
        const int scaleNoteCounts[5] = { 7, 7, 5, 7, 7 };
    
        int scaleIdx  = juce::jlimit (0, 4, static_cast<int> (scale * 5.0f));
        int noteCount = scaleNoteCounts[scaleIdx];
    
        // 3. BRIGHTNESS — which octave register the resonators sit in
        // 0.0 = fundamental octave, 1.0 = 3 octaves up (shimmery)
        float octaveShift = 1.0f + brightness * 3.0f; // 1x to 8x frequency multiplier
    
        // 4. DEPTH — resonance Q factor
        // 0.0 = subtle, 1.0 = very resonant/ringing
        float q = juce::jmap (depth, 0.0f, 1.0f, 1.0f, 40.0f);
    
        // 5. PROCESS EACH SCALE NOTE through a resonant bandpass
        float output    = 0.0f;
        int   bandIdx   = 0;
    
        for (int note = 0; note < noteCount && bandIdx < numResonatorBands; ++note)
        {
            // Frequency for this scale note, shifted by brightness octave
            float semitones = static_cast<float> (scaleIntervals[scaleIdx][note]);
            float bandFreq  = rootFreq * std::pow (2.0f, semitones / 12.0f) * octaveShift;
            bandFreq        = juce::jlimit (20.0f, static_cast<float> (sampleRate) * 0.45f, bandFreq);
    
            // Biquad bandpass coefficients
            float w0    = 2.0f * juce::MathConstants<float>::pi * bandFreq
                          / static_cast<float> (sampleRate);
            float sinW0 = std::sin (w0);
            float cosW0 = std::cos (w0);
            float alpha = sinW0 / (2.0f * q);
    
            float b0 =  alpha;
            float b2 = -alpha;
            float a0 =  1.0f + alpha;
            float a1 = -2.0f * cosW0;
            float a2 =  1.0f - alpha;
    
            // Normalize
            b0 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
    
            // Process biquad (transposed direct form II)
            float bandOut    = b0 * input + resS1[bandIdx];
            resS1[bandIdx]   = -a1 * bandOut + resS2[bandIdx];
            resS2[bandIdx]   = -a2 * bandOut + b2 * input;
    
            output += bandOut;
            bandIdx++;
        }
    
        // Also add the root one octave up for shimmer
        if (bandIdx < numResonatorBands)
        {
            float shimmerFreq = rootFreq * octaveShift * 2.0f;
            shimmerFreq       = juce::jlimit (20.0f, static_cast<float> (sampleRate) * 0.45f, shimmerFreq);
    
            float w0    = 2.0f * juce::MathConstants<float>::pi * shimmerFreq
                          / static_cast<float> (sampleRate);
            float sinW0 = std::sin (w0);
            float cosW0 = std::cos (w0);
            float alpha = sinW0 / (2.0f * q * 0.5f); // slightly wider for shimmer
    
            float b0 =  alpha / (1.0f + alpha);
            float b2 = -alpha / (1.0f + alpha);
            float a1 = -2.0f * cosW0 / (1.0f + alpha);
            float a2 = (1.0f - alpha) / (1.0f + alpha);
    
            float bandOut      = b0 * input + resS1[bandIdx];
            resS1[bandIdx]     = -a1 * bandOut + resS2[bandIdx];
            resS2[bandIdx]     = -a2 * bandOut + b2 * input;
            output            += bandOut * 0.5f; // shimmer at half level
        }
    
        // 6. MIX — normalize by band count and blend with dry
        float wetGain = 1.0f / std::sqrt (static_cast<float> (noteCount + 1));
        float wet     = output * wetGain;
    
        // Add dry signal underneath for the "transform" feel
        return std::isfinite (wet) ? (wet + input * 0.3f) : input;
    }

    int getCurrentType() const noexcept { return static_cast<int>(currentType); }

protected:
    void updateCoefficients()
    {
        g = std::tan (juce::MathConstants<float>::pi * targetCutoff / static_cast<float>(sampleRate));
        k = 1.0f / targetResonance;
        h = 1.0f / (1.0f + g * (g + k));
    }

    // Fast lock-free random generator (Linear Congruential Generator)
    float fastRandom() 
    {
        lcgState = 1664525 * lcgState + 1013904223;
        return static_cast<float>(lcgState) / static_cast<float>(0xFFFFFFFF);
    }

    static float hermiteInterp (const float* buffer, int bufferSize, float readPos)
    {
        int   idx1 = static_cast<int> (readPos) % bufferSize;
        float frac = readPos - std::floor (readPos);
        int   idx0 = (idx1 - 1 + bufferSize) % bufferSize;
        int   idx2 = (idx1 + 1) % bufferSize;
        int   idx3 = (idx1 + 2) % bufferSize;

        float y0 = buffer[idx0];
        float y1 = buffer[idx1];
        float y2 = buffer[idx2];
        float y3 = buffer[idx3];

        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    // Filter Coefficients & Internal State Registers
    float g { 0.0f };
    float k { 0.0f };
    float h { 0.0f };
    float s1 { 0.0f };
    float s2 { 0.0f };

    // Formant Filter State Registers (3 parallel bands)
    float f_s1[3] { 0.0f, 0.0f, 0.0f };
    float f_s2[3] { 0.0f, 0.0f, 0.0f };

    // Comb & Granular Memory
    std::vector<float> combBuffer;
    int writePtr { 0 };
    float lastCombDamping { 0.0f };
    
    // Granular System
    static constexpr int maxGrains = 16; // Adjust based on CPU limits
    std::vector<Grain> grains;
    uint32_t lcgState { 12345 }; // Seed for fastRandom()

    // Tape filter state
    float tapePhase        = 0.0f;  // wobble LFO phase
    float tapeDelayBuffer[8192] { 0.0f }; // short delay for wow/flutter
    int   tapeWritePtr     = 0;
    float tapeReadPtr      = 0.0f;
    float tapeLastSample   = 0.0f; // for age/HF loss one-pole filter
    float tapeSatDrive     = 1.0f;

    // Bitcrusher state
    juce::Random crushRandom; // for random noise
    float crushHeld      = 0.0f; // held sample for downsampling
    int   crushCounter   = 0;    // sample counter for downsampling

    // Allpass Delay state
    static constexpr int apDelayBufferSize = 96000; // 2 seconds at 48kHz
    std::array<float, apDelayBufferSize> apDelayBuffer { 0.0f };
    int   apDelayWritePtr  = 0;
    float apDelayLastSample = 0.0f;
    
    // Allpass stages for delay diffusion (4 stages)
    static constexpr int numApStages = 4;
    static constexpr int apStageSize = 4096;
    std::array<std::array<float, apStageSize>, numApStages> apStageBuffers {};
    std::array<int, numApStages> apStageWritePtrs { 0, 0, 0, 0 };
    
    // Allpass Reverb state — separate buffers so delay and reverb don't share
    static constexpr int apRevBufferSize = 96000;
    std::array<float, apRevBufferSize> apRevBuffer { 0.0f };
    int   apRevWritePtr   = 0;
    float apRevLastSample = 0.0f;
    
    static constexpr int numRevStages = 8; // more stages = denser reverb
    static constexpr int apRevStageSize = 4096;
    std::array<std::array<float, apRevStageSize>, numRevStages> apRevStageBuffers {};
    std::array<int, numRevStages> apRevStageWritePtrs { 0, 0, 0, 0, 0, 0, 0, 0 };

    // Compressor state
    float compEnvelope  = 0.0f;  // envelope follower
    float compGainDb    = 0.0f;  // current gain in dB

    // Varispeed state
    float varispeedBuffer[96000] { 0.0f };
    int   varispeedWritePtr = 0;
    float varispeedReadPtr  = 0.0f;
    float varispeedCurrentSpeed = 1.0f; // current playback speed

    // Scatter state
    static constexpr int scatterBufferSize = 88200; // 2 seconds at 44.1kHz
    std::array<float, scatterBufferSize> scatterBuffer { 0.0f };
    int   scatterWritePtr    = 0;
    float scatterReadPtr     = 0.0f;
    int   scatterPattern     = 0;
    int   scatterPhase       = 0;      // current position in pattern cycle
    int   scatterCycleLength = 0;      // length of one pattern cycle in samples
    bool  scatterActive      = false;  // whether currently scattering
    float scatterSpeed       = 1.0f;
    juce::Random scatterRandom;

    // Ring Mod
    float ringPhase     = 0.0f;
    float ringFeedback  = 0.0f;

    // Chorus state
    static constexpr int chorusDelaySize = 48000;
    static constexpr int numChorusVoices = 4;
    std::array<float, chorusDelaySize> chorusDelayBuffer { 0.0f };
    int   chorusWritePtr = 0;
    std::array<float, numChorusVoices> chorusLFOPhases { 0.0f, 0.0f, 0.0f, 0.0f };

    static constexpr int numPhaserStages = 8;
    std::array<float, numPhaserStages> phaserAllpassState { 0.0f };
    float phaserLFOPhase  = 0.0f;
    float phaserFeedback  = 0.0f;

    // Distortion state
    float distToneState  = 0.0f; // one-pole tone filter state
    float distLastSign   = 1.0f; // for zero-crossing degradation
    int   distGlitchCount = 0;   // glitch counter
    juce::Random distRandom;

    // DJFX Delay state
    static constexpr int djfxBufferSize = 96000; // 2 seconds at 48kHz
    std::array<float, djfxBufferSize> djfxBuffer { 0.0f };
    int   djfxWritePtr     = 0;
    float djfxReadPtr      = 0.0f;
    bool  djfxCapturing    = false;
    int   djfxCaptureSize  = 0;
    int   djfxCaptureCount = 0;
    float djfxDriftPhase   = 0.0f;
    juce::Random djfxRandom;

    // Harmonic Resonator state
    static constexpr int numResonatorBands = 8;
    std::array<float, numResonatorBands> resS1 { 0.0f };
    std::array<float, numResonatorBands> resS2 { 0.0f };

    // Parameters Cache
    double sampleRate { 44100.0 };
    float targetCutoff { 1000.0f };
    float targetResonance { 0.707f };
    FilterType currentType { Lowpass };
};
