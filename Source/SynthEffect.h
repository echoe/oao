// SynthEffect.h
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

struct StereoOutput { float L; float R; };

class SynthEffect
{
public:
    enum EffectType { Lowpass = 0, Highpass, Bandpass, Comb, Granular, Formant };
    
    SynthEffect() : freezeFFT (freezeFFTOrder)
    {
        grains.resize (maxGrains, {0.0f, 0.0f, 0.0f, false});
    
        // Hann window
        for (int i = 0; i < freezeFFTSize; ++i)
            freezeWindow[i] = 0.5f * (1.0f - std::cos (
                2.0f * juce::MathConstants<float>::pi
                * static_cast<float> (i) / static_cast<float> (freezeFFTSize - 1)));
    
        reset();
    }
    virtual ~SynthEffect() = default; 

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
        currentType = static_cast<EffectType>(juce::jlimit (0, 5, typeIndex));
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
	// Color Bass
        resS1.fill (0.0f); resS2.fill (0.0f);
        resB0.fill (0.0f); resB2.fill (0.0f);
        resA1.fill (0.0f); resA2.fill (0.0f);
        cachedDrive = -1.0f; cachedTone = -1.0f;
        cachedDecay = -1.0f; cachedDetectedFreq = 0.0f;
        grainBuf.fill (0.0f); grainWritePos = 0;
        pitchPhase = 0.0f; tiltLowZ = 0.0f;
        zcPrev = 0.0f; zcPeriodSamples = 0.0f;
        zcSampleCount = 0; detectedFreq = 80.0f;
	// ambient shimmer
        ambientBuffer.fill (0.0f);
        ambientWritePtr   = 0;
        ambientLastSample = 0.0f;
        shimmerBuffer.fill (0.0f);
        shimmerWritePtr = 0;
        shimmerReadPtr  = 0.0f;
        shimmerPhase    = 0.0f;
        for (int i = 0; i < numAmbientStages; ++i)
        {
            ambientStageBuffers[i].fill (0.0f);
            ambientStageWritePtrs[i] = 0;
        }
	//OldChorus
        oldChorusDelayBuffer.fill (0.0f);
        oldChorusWritePtr   = 0;
        oldChorusLFOPhase   = 0.0f;
        oldChorusLastSample = 0.0f;
	//OTT
        for (int i = 0; i < numOTTBands; ++i)
        {
            ottLP1_s1[i] = ottLP1_s2[i] = 0.0f;
            ottLP2_s1[i] = ottLP2_s2[i] = 0.0f;
            ottHP1_s1[i] = ottHP1_s2[i] = 0.0f;
            ottHP2_s1[i] = ottHP2_s2[i] = 0.0f;
            ottEnvUp[i]   = 0.0f;
            ottEnvDown[i] = 0.0f;
        }
	ottToneLast = 0.0f;
	// spectral freeze
        freezeInputBuffer.fill  (0.0f);
        freezeFrozenFrame.fill  (0.0f);
        freezeOutputBuffer.fill (0.0f);
        freezeWorkBuffer.fill   (0.0f);
        freezeSynthFrame.fill   (0.0f);
        freezeInputWritePos = 0;
        freezeOutputReadPos = 0;
        freezeHopCounter = 0;
        freezeHasFrozenFrame = false;
        freezeMix = 0.0f;
        // effect + drive
	fd_s1 = 0.0f; fd_s2 = 0.0f;
	// 3-lane eq
        eq_s1_low = 0.0f; eq_s2_low = 0.0f;
        eq_s1_high = 0.0f; eq_s2_high = 0.0f;
	// timeshift delay
        tcDelayBuffer.fill (0.0f);
        tcWritePtr = 0; tcSmoothedSamples = 0.3f; //safer baseline
	tcDampingState = 0.0f; tcHighPassState = 0.0f;
	// lofi
	lofiDownsampleCounter = 0.0f; lofiHoldSample = 0.0f;
        lofiLPState = 0.0f; lofiHPState = 0.0f; lofiNoiseSeed = 123456789; //mono
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

    // Allpass Filter (Schroeder implementation to avoid gain loop)
    template <size_t BufferSize>
    float processAllpass (float input, float coeff,
                          std::array<float, BufferSize>& buffer,
                          int& writePtr, int delaySamples)
    {
        int size = static_cast<int> (BufferSize);
        
        int readPtr = (writePtr - delaySamples + size) % size;
        float delayed = buffer[readPtr];
        float v_n = input + coeff * delayed;
        float output = -coeff * v_n + delayed;
        buffer[writePtr] = v_n;
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
        // 1. HARD CLAMP the cutoff to 45% of the sample rate.
        float safeCutoff = juce::jlimit(20.0f, static_cast<float>(sampleRate) * 0.45f, cutoffHz);

        // 2. Stable Fast Tangent Approximation
        float x = 3.1415926535f * safeCutoff / static_cast<float>(sampleRate);
        float x2 = x * x;
        float g_mod = x * (15.0f - x2) / (15.0f - 6.0f * x2);

        // 3. TPT Denominator
        float h_mod = 1.0f / (1.0f + g_mod * (g_mod + precalculatedK));

        // 4. State Variable Filter processing
        float hp = (input - (g_mod + precalculatedK) * s1 - s2) * h_mod;
        float bp = g_mod * hp + s1;
        float lp = g_mod * bp + s2;

        // 5. Update state variables
        s1 = 2.0f * bp - s1;
        s2 = 2.0f * lp - s2;

        // 6. Prevent NaNs and infinite feedback
        if (std::isnan(s1) || std::isinf(s1) || std::abs(s1) < 1e-6f)
        {
            if (std::isnan(s1) || std::isinf(s1)) reset();
            else { s1 = 0.0f; s2 = 0.0f; } // Snap denormals to zero
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
            // 1. HARD CLAMP the cutoff to 45% of the sample rate.
            float safeCutoff = juce::jlimit(20.0f, static_cast<float>(sampleRate) * 0.45f, freqs[i]);

            // 2. Stable Fast Tangent Approximation
            float x = juce::MathConstants<float>::pi * safeCutoff / static_cast<float>(sampleRate);
            float x2 = x * x;
            float g_mod = x * (15.0f - x2) / (15.0f - 6.0f * x2);

            // 3. TPT Denominator
            float h_mod = 1.0f / (1.0f + g_mod * (g_mod + precalcK));

            // 4. State Variable Filter processing
            float hp = (input - (g_mod + precalcK) * f_s1[i] - f_s2[i]) * h_mod;
            float bp = g_mod * hp + f_s1[i];
            float lp = g_mod * bp + f_s2[i];

            // 5. Update state variables
            f_s1[i] = 2.0f * bp - f_s1[i];
            f_s2[i] = 2.0f * lp - f_s2[i];

            // 6. Prevent NaNs and infinite feedback
            if (std::isnan(f_s1[i]) || std::isinf(f_s1[i]) || std::abs(f_s1[i]) < 1e-6f)
            {
                if (std::isnan(f_s1[i]) || std::isinf(f_s1[i])) reset();
                else { f_s1[i] = 0.0f; f_s2[i] = 0.0f; } // Snap denormals to zero
                return 0.0f;
            }
            
            outputAccumulator += bp;
        }
	// Gain compensation for 3 parallel resonant peaks & soft clipper
        return std::tanh(outputAccumulator * 0.33f);
    }

    // Comb Filter
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

    // Granular Resonator / Glitch Effect
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
        // cascade allpass effects
	// Array of mutually prime numbers for smooth diffusion
        float diffCoeff = diffusion * 0.7f;
        float diffused  = input;
    
        for (int i = 0; i < numApStages; ++i)
        {
            // Pass the fixed prime number directly into the new 5-argument function
            diffused = processAllpass (diffused, diffCoeff,
                                       apStageBuffers[i], apStageWritePtrs[i],
                                       primeDelays[i]);
        }
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
        // Array of mutually prime numbers for smooth diffusion
        static constexpr std::array<int, 4> primeDelays = { 211, 347, 523, 701 };

        float diffCoeff = diffusion * 0.7f;
        float diffused  = input;

        for (int i = 0; i < numApStages; ++i)
        {
            // Pass the fixed prime number directly into the new 5-argument function
            diffused = processAllpass (diffused, diffCoeff,
                                       apStageBuffers[i], apStageWritePtrs[i],
                                       primeDelays[i]);
        }
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
        {
            // Make sure your index 'i' maps correctly to the prime array! 
            // If numRevStages is 8, and you start at 4, you might need to use [i - (numRevStages / 2)]
            int primeIndex = i - (numRevStages / 2);
            
            output = processAllpass (output, diffCoeff,
                                     apRevStageBuffers[i], apRevStageWritePtrs[i],
                                     revPrimeDelays[primeIndex]);
        }
    
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
    
        // 4. TONE — post-distortion one-pole effect
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
        // 1. ALWAYS WRITE TO BUFFER CONTINUOUSLY
        djfxBuffer[djfxWritePtr] = input;
        djfxWritePtr = (djfxWritePtr + 1) % djfxBufferSize;
    
        // 2. SPEED & DRIFT 
        float normalizedSpeed = speed * 2.0f - 1.0f; 
        float playbackSpeed   = normalizedSpeed * 2.0f; //so you can speed up or slow down more if you want  

        if (drift > 0.001f)
        {
            djfxDriftPhase += 0.003f; 
            if (djfxDriftPhase >= juce::MathConstants<float>::twoPi)
                djfxDriftPhase -= juce::MathConstants<float>::twoPi;
    
            float driftLFO    = std::sin (djfxDriftPhase) * drift * 0.15f;
            float driftRandom = (djfxRandom.nextFloat() * 2.0f - 1.0f) * drift * 0.05f;
            playbackSpeed    += driftLFO + driftRandom;
        }
    
        // 3. ADVANCE READ POINTER
        djfxReadPtr += playbackSpeed;
    
        // Calculate window size (Shared by both modes)
        float bufferMs    = juce::jmap (bufferAmt, 0.0f, 1.0f, 10.0f, 2000.0f);
        int   captureSize = juce::jlimit (1, djfxBufferSize - 1,
                                          static_cast<int> (bufferMs / 1000.0f * sampleRate));
    
        // 4A. STUTTER LOOP MODE
	if (on >= 0.5f)
        {
            // Latch the loop start position ONLY once when entering the loop
            if (djfxLoopStart < 0.0f)
            {
                float bufferMs = juce::jmap (bufferAmt, 0.0f, 1.0f, 10.0f, 2000.0f);
                int captureSize = juce::jlimit (1, djfxBufferSize - 1, static_cast<int>(bufferMs / 1000.0f * sampleRate));

                // Lock the start point to where the write pointer is right now
                djfxLoopStart = static_cast<float>(djfxWritePtr);
                djfxReadPtr = djfxLoopStart;
            }

            // Advance read pointer
            djfxReadPtr += playbackSpeed;

            // Calculate capture size based on the current bufferAmt
            float bufferMs = juce::jmap (bufferAmt, 0.0f, 1.0f, 10.0f, 2000.0f);
            int captureSize = static_cast<int>(bufferMs / 1000.0f * sampleRate);

            // Keep read pointer strictly within the LATCHED window
            // This ensures the loop doesn't move with the live input
            float end = djfxLoopStart + (float)captureSize;

            while (djfxReadPtr >= end) djfxReadPtr -= (float)captureSize;
            while (djfxReadPtr < djfxLoopStart) djfxReadPtr += (float)captureSize;
        }
        // 4B. VARISPEED WITH RHYTHMIC JUMP
        else
        {
            djfxLoopStart = -1.0f; 
            djfxJumpTimer++;
            // When the timer hits the buffer size, clear the built-up delay
            if (djfxJumpTimer >= captureSize)
            {
                djfxJumpTimer = 0;
                // Snap read pointer to exactly 1 sample behind the write pointer
                djfxReadPtr = static_cast<float>(djfxWritePtr) - 1.0f;
            }
        } 
        // 5. GLOBAL BUFFER WRAPPING
        while (djfxReadPtr >= static_cast<float>(djfxBufferSize)) djfxReadPtr -= static_cast<float>(djfxBufferSize);
        while (djfxReadPtr < 0.0f)                                djfxReadPtr += static_cast<float>(djfxBufferSize);
    
        // 6. INTERPOLATE & OUTPUT
        float output = hermiteInterp (djfxBuffer.data(), djfxBufferSize, djfxReadPtr);
        return std::isfinite (output) ? output : 0.0f;
    }

    float processSampleColorBass (float  input,
                                   float  drive,
                                   float  shimmer,
                                   float  tone,
                                   float  decay,
                                   double sampleRate)
    {
        // pitch detection (zero-crossing period estimator)
        ++zcSampleCount;
        if (zcPrev <= 0.0f && input > 0.0f)
        {
            if (zcSampleCount > 16)
            {
                zcPeriodSamples = zcPeriodSamples * 0.85f
                                  + static_cast<float> (zcSampleCount) * 0.15f;
                float freq = static_cast<float> (sampleRate) / zcPeriodSamples;
                if (freq >= 30.0f && freq <= 400.0f)
                    detectedFreq = freq;
            }
            zcSampleCount = 0;
        }
        zcPrev = input;
    
        float rootFreq = detectedFreq;
    
        // 2. PITCH-SHIFTED EXCITER  (fixed perfect fifth = +7 semitones)
        //    Hardcoded to P5 as the most universally musical shimmer interval.
        //    Change the semitones constant to 4 (M3), 5 (P4), or 12 (Oct)
        //    if you want a different default.
        constexpr float kShimmerSemitones = 7.0f;   // perfect fifth
        constexpr float kPitchRatio       = 1.4983f; // std::pow(2, 7/12) baked out
    
        int   writeMod = grainWritePos & (kGrainSize * 2 - 1);
        grainBuf[writeMod] = input;
    
        pitchPhase += kPitchRatio;
        if (pitchPhase >= static_cast<float> (kGrainSize))
            pitchPhase -= static_cast<float> (kGrainSize);
    
        int   readInt  = static_cast<int> (pitchPhase);
        float readFrac = pitchPhase - static_cast<float> (readInt);
        int   r0 = (grainWritePos - kGrainSize + readInt)     & (kGrainSize * 2 - 1);
        int   r1 = (grainWritePos - kGrainSize + readInt + 1) & (kGrainSize * 2 - 1);
    
        float windowPhase  = pitchPhase / static_cast<float> (kGrainSize);
        float hannWindow   = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi
                                                      * windowPhase);
        float shiftedInput = (grainBuf[r0] + readFrac * (grainBuf[r1] - grainBuf[r0]))
                             * hannWindow;
    
        ++grainWritePos;
    
        // 3. RESONATOR COEFFICIENT UPDATE  (only on knob change or pitch drift)
        const float harmonicRatios[7] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f };
        const int   numHarmonics      = 7;
    
        float q        = juce::jmap (decay, 0.0f, 1.0f, 8.0f, 220.0f);
        float shimmerQ = q * 3.0f;
    
        bool needsUpdate = (drive != cachedDrive ||
                            tone  != cachedTone  ||
                            decay != cachedDecay);
    
        float freqRatio = (detectedFreq > 0.0f)
                          ? detectedFreq / std::max (1.0f, cachedDetectedFreq)
                          : 1.0f;
        if (freqRatio > 1.029f || freqRatio < 0.972f)
            needsUpdate = true;
    
        if (needsUpdate)
        {
            cachedDrive        = drive;
            cachedTone         = tone;
            cachedDecay        = decay;
            cachedDetectedFreq = detectedFreq;
    
            int bandIdx = 0;
    
            for (int h = 0; h < numHarmonics && bandIdx < numResonatorBands; ++h)
            {
                float bandFreq = juce::jlimit (
                    20.0f, static_cast<float> (sampleRate) * 0.45f,
                    rootFreq * harmonicRatios[h]);
    
                float w0    = 2.0f * juce::MathConstants<float>::pi * bandFreq
                              / static_cast<float> (sampleRate);
                float sinW0 = std::sin (w0);
                float cosW0 = std::cos (w0);
                float alpha = sinW0 / (2.0f * q);
                float a0    = 1.0f + alpha;
    
                resB0[bandIdx] =  alpha / a0;
                resB2[bandIdx] = -alpha / a0;
                resA1[bandIdx] = -2.0f * cosW0 / a0;
                resA2[bandIdx] = (1.0f - alpha) / a0;
                ++bandIdx;
            }
    
            // Shimmer band 1: P5 above root (× kPitchRatio × 2)
            if (bandIdx < numResonatorBands)
            {
                float shimFreq = juce::jlimit (
                    20.0f, static_cast<float> (sampleRate) * 0.45f,
                    rootFreq * kPitchRatio * 2.0f);
    
                float w0    = 2.0f * juce::MathConstants<float>::pi * shimFreq
                              / static_cast<float> (sampleRate);
                float sinW0 = std::sin (w0);
                float cosW0 = std::cos (w0);
                float alpha = sinW0 / (2.0f * shimmerQ);
                float a0    = 1.0f + alpha;
    
                resB0[bandIdx] =  alpha / a0;
                resB2[bandIdx] = -alpha / a0;
                resA1[bandIdx] = -2.0f * cosW0 / a0;
                resA2[bandIdx] = (1.0f - alpha) / a0;
                ++bandIdx;
            }
    
            // Shimmer band 2: P5 two octaves up (× kPitchRatio × 4)
            if (bandIdx < numResonatorBands)
            {
                float shimFreq = juce::jlimit (
                    20.0f, static_cast<float> (sampleRate) * 0.45f,
                    rootFreq * kPitchRatio * 4.0f);
    
                float w0    = 2.0f * juce::MathConstants<float>::pi * shimFreq
                              / static_cast<float> (sampleRate);
                float sinW0 = std::sin (w0);
                float cosW0 = std::cos (w0);
                float alpha = sinW0 / (2.0f * shimmerQ * 1.5f);
                float a0    = 1.0f + alpha;
    
                resB0[bandIdx] =  alpha / a0;
                resB2[bandIdx] = -alpha / a0;
                resA1[bandIdx] = -2.0f * cosW0 / a0;
                resA2[bandIdx] = (1.0f - alpha) / a0;
            }
        }
    
        // 4. PROCESS RESONATORS
        float bodyOut    = 0.0f;
        float shimmerOut = 0.0f;
        int   bandIdx    = 0;
    
        for (int h = 0; h < numHarmonics && bandIdx < numResonatorBands; ++h)
        {
            float bandOut  = resB0[bandIdx] * input + resS1[bandIdx];
            resS1[bandIdx] = -resA1[bandIdx] * bandOut + resS2[bandIdx];
            resS2[bandIdx] = -resA2[bandIdx] * bandOut + resB2[bandIdx] * input;
            bodyOut       += bandOut;
            ++bandIdx;
        }
    
        if (bandIdx < numResonatorBands)
        {
            float bandOut  = resB0[bandIdx] * shiftedInput + resS1[bandIdx];
            resS1[bandIdx] = -resA1[bandIdx] * bandOut + resS2[bandIdx];
            resS2[bandIdx] = -resA2[bandIdx] * bandOut + resB2[bandIdx] * shiftedInput;
            shimmerOut    += bandOut * 1.2f;
            ++bandIdx;
        }
    
        if (bandIdx < numResonatorBands)
        {
            float bandOut  = resB0[bandIdx] * shiftedInput + resS1[bandIdx];
            resS1[bandIdx] = -resA1[bandIdx] * bandOut + resS2[bandIdx];
            resS2[bandIdx] = -resA2[bandIdx] * bandOut + resB2[bandIdx] * shiftedInput;
            shimmerOut    += bandOut * 0.8f;
        }
    
        // 5. TILT EQ
        float tiltAmt  = (tone - 0.5f) * 2.0f;
        float tiltFreq = 800.0f;
        float tiltCoef = std::exp (-2.0f * juce::MathConstants<float>::pi
                                   * tiltFreq / static_cast<float> (sampleRate));
    
        tiltLowZ       = tiltLowZ * tiltCoef + bodyOut * (1.0f - tiltCoef);
        float tiltHigh = bodyOut - tiltLowZ;
        float tiltedBody = bodyOut + tiltAmt * (tiltHigh - tiltLowZ) * 0.5f;
    
        // 6. SATURATION
        float driveGain  = 1.0f + drive * 15.0f;
        float bodyDriven = std::tanh (tiltedBody * driveGain) / std::tanh (driveGain);
        float shimDriven = std::tanh (shimmerOut * driveGain) / std::tanh (driveGain);
    
        // 7. MIX
        float bodyNorm    = bodyDriven / std::sqrt (static_cast<float> (numHarmonics));
        float shimmerNorm = shimDriven / std::sqrt (2.0f);
    
        float wet = bodyNorm + shimmerNorm * shimmer;
        float out = wet * 0.7f + input * 0.3f;
    
        return std::isfinite (out) ? out : input;
    }

    float processSampleAmbientDelay (float input, float time, float feedback,
                                      float shimmer, float diffusion,
                                      double sampleRate)
    {
        // diffuse through allpass stages
        // Array of mutually prime numbers for smooth diffusion
        float diffCoeff = diffusion * 0.7f;
        float diffused  = input;

        for (int i = 0; i < numApStages; ++i)
        {
            // Pass the fixed prime number directly into the new 5-argument function
            diffused = processAllpass (diffused, diffCoeff,
                                       apStageBuffers[i], apStageWritePtrs[i],
                                       primeDelays[i]);
        }
    
        // delay — very long, 100ms to 16 seconds
        float delayMs      = juce::jmap (time, 0.0f, 1.0f, 100.0f, 16000.0f);
        int   delaySamples = juce::jlimit (1, ambientBufferSize - 1,
                                 static_cast<int> (delayMs / 1000.0f * sampleRate));
    
        int   readPtr  = (ambientWritePtr - delaySamples + ambientBufferSize) % ambientBufferSize;
        float delayed  = ambientBuffer[readPtr];
    
        // shimmer. uses a simple granular pitch shift — reads buffer at 2x speed
        float shimmered = 0.0f;
        if (shimmer > 0.001f)
        {
            // Write to shimmer buffer
            shimmerBuffer[shimmerWritePtr] = delayed;
            shimmerWritePtr = (shimmerWritePtr + 1) % shimmerBufferSize;
    
            // Read at 2x speed for octave up
            shimmerReadPtr += 2.0f;
            if (shimmerReadPtr >= static_cast<float> (shimmerBufferSize))
                shimmerReadPtr -= static_cast<float> (shimmerBufferSize);
    
            shimmered = hermiteInterp (shimmerBuffer.data(), shimmerBufferSize, shimmerReadPtr);
    
            // Crossfade window to prevent clicks from the looping read head
            shimmerPhase += 2.0f / static_cast<float> (shimmerBufferSize);
            if (shimmerPhase >= 1.0f) shimmerPhase -= 1.0f;
            float window  = 0.5f * (1.0f - std::cos (shimmerPhase * juce::MathConstants<float>::twoPi));
            shimmered    *= window;
        }
    
        // feedback with shimmer mixed in
        float feedbackAmt    = juce::jlimit (0.0f, 0.98f, feedback);
        float feedbackSignal = delayed + shimmered * shimmer;
        feedbackSignal       = std::tanh (feedbackSignal); // soft clip to prevent blowup
    
        // write to delay
        ambientBuffer[ambientWritePtr] = diffused + feedbackSignal * feedbackAmt;
        ambientWritePtr = (ambientWritePtr + 1) % ambientBufferSize;
    
        //  smear the output into a wash
        float output = delayed; // Avoid redefining 'output' if in the same scope
        
        for (int i = numAmbientStages / 2; i < numAmbientStages; ++i)
        {
            int primeIndex = i - (numAmbientStages / 2);
            
            output = processAllpass (output, diffCoeff,
                                         ambientStageBuffers[i], ambientStageWritePtrs[i],
                                         ambientPrimeDelays[primeIndex]);
        }
        // mix
        return std::isfinite (output) ? output + input * 0.3f : input;
    }

    float processSampleOldChorus (float input, float rate, float depth,
                                   float mode, float warmth,
                                   double sampleRate)
    {
        // Single shared tri-wave LFO.
        float lfoRate = 0.1f + rate * 4.9f;
        oldChorusLFOPhase += (lfoRate * juce::MathConstants<float>::twoPi)
                              / static_cast<float> (sampleRate);
        if (oldChorusLFOPhase >= juce::MathConstants<float>::twoPi)
            oldChorusLFOPhase -= juce::MathConstants<float>::twoPi;
    
        float lfoNorm     = oldChorusLFOPhase / juce::MathConstants<float>::twoPi;
        float triangleLFO = (lfoNorm < 0.5f)
                                ? (lfoNorm * 4.0f - 1.0f)
                                : (3.0f - lfoNorm * 4.0f);
        // 1.7ms to 16ms sweep, then write to buffer
        float minDelayMs    = 1.7f;
        float maxDelayMs    = juce::jmap (depth, 0.0f, 1.0f, 4.0f, 16.0f);
        float centerDelayMs = (minDelayMs + maxDelayMs) * 0.5f;
        float sweepMs       = (maxDelayMs - minDelayMs) * 0.5f;
        oldChorusDelayBuffer[oldChorusWritePtr] = input;
        oldChorusWritePtr = (oldChorusWritePtr + 1) % oldChorusDelaySize;
    
        // two modes — below 0.5 = Mode I (1 voice), above = Mode II (2 voices)
        int   numVoices = (mode > 0.5f) ? 2 : 1;
        float output    = 0.0f;
        for (int v = 0; v < numVoices; ++v)
        {
            // Second voice 180° out of phase
            float lfo = (v == 0) ? triangleLFO : -triangleLFO;
            float delayMs      = centerDelayMs + lfo * sweepMs;
            float delaySamples = juce::jlimit (1.0f, static_cast<float> (oldChorusDelaySize - 4),
                                     delayMs / 1000.0f * static_cast<float> (sampleRate));
            float readPos = static_cast<float> (oldChorusWritePtr) - delaySamples;
            while (readPos < 0.0f) readPos += static_cast<float> (oldChorusDelaySize);
            output += hermiteInterp (oldChorusDelayBuffer.data(), oldChorusDelaySize, readPos);
        }
    
        output /= static_cast<float> (numVoices);
    
        // warmth — subtle smear from previous sample
        float bbd = warmth * 0.1f;
        float wet  = output * (1.0f - bbd) + oldChorusLastSample * bbd;
        oldChorusLastSample = wet;
    
        // Equal dry/wet, no feedback
        return wet * 0.5f + input * 0.5f;
    }

    float processSampleOTT (float input, float depth, float timeKnob,
                             float upward, float tone,
                             double sampleRate)
    {
        // 1. CROSSOVER — split into 3 bands using SVF LP/HP
        // Low: 0 to 200Hz, Mid: 200Hz to 2500Hz, High: 2500Hz+
        float crossFreqs[2] = { ottLowMidFreq, ottMidHighFreq };
        float bands[3] = { 0.0f, 0.0f, 0.0f };
    
        float sig = input;
    
        // Low band — LP at 200Hz
        {
            float x  = juce::MathConstants<float>::pi * crossFreqs[0]
                       / static_cast<float> (sampleRate);
            float g  = std::tan (x);
            float gk = g + 1.0f;
            float h  = 1.0f / (1.0f + g * gk);
            // First LP stage
            float hp = (sig - gk * ottLP1_s1[0] - ottLP1_s2[0]) * h;
            float bp = g * hp + ottLP1_s1[0];
            float lp = g * bp + ottLP1_s2[0];
            ottLP1_s1[0] = 2.0f * bp - ottLP1_s1[0];
            ottLP1_s2[0] = 2.0f * lp - ottLP1_s2[0];
            // Second LP stage for steeper slope
            float hp2 = (lp - gk * ottLP2_s1[0] - ottLP2_s2[0]) * h;
            float bp2 = g * hp2 + ottLP2_s1[0];
            float lp2 = g * bp2 + ottLP2_s2[0];
            ottLP2_s1[0] = 2.0f * bp2 - ottLP2_s1[0];
            ottLP2_s2[0] = 2.0f * lp2 - ottLP2_s2[0];
            bands[0] = lp2; // Low band
            float remainder = sig - lp2;
            // Mid band — LP at 2500Hz from remainder
            float x2  = juce::MathConstants<float>::pi * crossFreqs[1]
                        / static_cast<float> (sampleRate);
            float g2  = std::tan (x2);
            float gk2 = g2 + 1.0f;
            float h2  = 1.0f / (1.0f + g2 * gk2);
            float hp3 = (remainder - gk2 * ottLP1_s1[1] - ottLP1_s2[1]) * h2;
            float bp3 = g2 * hp3 + ottLP1_s1[1];
            float lp3 = g2 * bp3 + ottLP1_s2[1];
            ottLP1_s1[1] = 2.0f * bp3 - ottLP1_s1[1];
            ottLP1_s2[1] = 2.0f * lp3 - ottLP1_s2[1];
            float hp4 = (lp3 - gk2 * ottLP2_s1[1] - ottLP2_s2[1]) * h2;
            float bp4 = g2 * hp4 + ottLP2_s1[1];
            float lp4 = g2 * bp4 + ottLP2_s2[1];
            ottLP2_s1[1] = 2.0f * bp4 - ottLP2_s1[1];
            ottLP2_s2[1] = 2.0f * lp4 - ottLP2_s2[1];
            bands[1] = lp4;           // Mid band
            bands[2] = remainder - lp4; // High band
        }
    
        // PER-BAND COMPRESSION. Downward: reduces loud signals. Upward: boosts quiet signals
        float attackMs  = juce::jmap (timeKnob, 0.0f, 1.0f, 0.1f, 80.0f);
        float releaseMs = juce::jmap (timeKnob, 0.0f, 1.0f, 10.0f, 400.0f);
    
        float attackCoeff  = std::exp (-1.0f / (static_cast<float> (sampleRate)
                                       * attackMs / 1000.0f));
        float releaseCoeff = std::exp (-1.0f / (static_cast<float> (sampleRate)
                                       * releaseMs / 1000.0f));
    
        // Band gain multipliers
        float bandGains[3] = { 0.8f, 1.0f, 1.2f };
        float output = 0.0f;
        for (int band = 0; band < numOTTBands; ++band)
        {
            float bandSig  = bands[band];
            float bandAbs  = std::abs (bandSig);
            // Downward compression envelope
            if (bandAbs > ottEnvDown[band])
                ottEnvDown[band] = attackCoeff  * ottEnvDown[band] + (1.0f - attackCoeff)  * bandAbs;
            else
                ottEnvDown[band] = releaseCoeff * ottEnvDown[band] + (1.0f - releaseCoeff) * bandAbs;
            // Upward compression envelope (slower)
            float upAttack  = std::exp (-1.0f / (static_cast<float> (sampleRate) * 0.05f));
            float upRelease = std::exp (-1.0f / (static_cast<float> (sampleRate) * 0.5f));
            if (bandAbs > ottEnvUp[band])
                ottEnvUp[band] = upAttack  * ottEnvUp[band] + (1.0f - upAttack)  * bandAbs;
            else
                ottEnvUp[band] = upRelease * ottEnvUp[band] + (1.0f - upRelease) * bandAbs;
    
            // Downward gain — compress signals above threshold
            float downThresh = 0.3f;
            float downRatio  = 4.0f;
            float downGain   = 1.0f;
    
            if (ottEnvDown[band] > downThresh)
            {
                float excessDb  = juce::Decibels::gainToDecibels (ottEnvDown[band] / downThresh);
                float reduceDb  = excessDb * (1.0f - 1.0f / downRatio);
                downGain        = juce::Decibels::decibelsToGain (-reduceDb * depth);
            }
    
            // Upward gain — boost signals below threshold
            float upThresh = 0.15f;
            float upGain   = 1.0f;
    
            if (ottEnvUp[band] < upThresh && ottEnvUp[band] > 0.0001f)
            {
                float belowDb = juce::Decibels::gainToDecibels (upThresh / ottEnvUp[band]);
                float boostDb = belowDb * upward * depth * 0.5f;
                upGain        = juce::Decibels::decibelsToGain (boostDb);
                upGain        = juce::jlimit (1.0f, 6.0f, upGain); // max 6x boost
            }
            // Apply gains and accumulate
            output += bandSig * downGain * upGain * bandGains[band];
        }
    
        // 3. TONE — slight tilt after compression
        float toneAlpha = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi
                                           * (500.0f + tone * 4500.0f)
                                           / static_cast<float> (sampleRate));
        // Simple tilt: blend between LP and original
        ottToneLast += toneAlpha * (output - ottToneLast);
        output = output * tone + ottToneLast * (1.0f - tone);
    
        // OUTPUT — soft clip to prevent blowup from upward comp
        return std::isfinite (output) ? std::tanh (output) : 0.0f;
    }

    float processSampleSpectralFreeze (float input, float freeze, float blend,
                                        float pitch, float blur,
                                        double sampleRate)
    {
        // write input to ring buffer
        freezeInputBuffer[freezeInputWritePos] = input;
        freezeInputWritePos = (freezeInputWritePos + 1) % freezeFFTSize;
        freezeHopCounter++;
    
        // process a new fft frame on every hop
        if (freezeHopCounter >= freezeHopSize)
        {
            freezeHopCounter = 0;
    
            // Copy windowed input into work buffer as interleaved complex
            for (int i = 0; i < freezeFFTSize; ++i)
            {
                int idx = (freezeInputWritePos + i) % freezeFFTSize;
                freezeWorkBuffer[i * 2]     = freezeInputBuffer[idx] * freezeWindow[i];
                freezeWorkBuffer[i * 2 + 1] = 0.0f; // imaginary = 0 for real input
            }
    
            // Forward FFT — use RealOnly to get complex output with phase info
            freezeFFT.performRealOnlyForwardTransform (freezeWorkBuffer.data());
    
            // capture frozen frame when freeze first engages
            if (freeze > 0.5f && !freezeHasFrozenFrame)
            {
                // Copy current complex spectrum into frozen frame
                // Randomize phases for a more musical freeze
                for (int i = 0; i < freezeFFTSize; ++i)
                {
                    float re  = freezeWorkBuffer[i * 2];
                    float im  = freezeWorkBuffer[i * 2 + 1];
                    float mag = std::sqrt (re * re + im * im);
    
                    // Random phase to avoid comb effecting artifacts
                    float phase = freezeRandom.nextFloat()
                                  * juce::MathConstants<float>::twoPi;
                    freezeFrozenFrame[i * 2]     = mag * std::cos (phase);
                    freezeFrozenFrame[i * 2 + 1] = mag * std::sin (phase);
                }
                freezeHasFrozenFrame = true;
            }
            else if (freeze <= 0.5f)
            {
                freezeHasFrozenFrame = false;
            }
            // 4. BUILD SYNTHESIS FRAME
            if (freezeHasFrozenFrame)
            {
                // Convert semitones to a linear pitch ratio
                // pitch = 0 means no shift, +12 = one octave up, -12 = one octave down
                float pitchRatio = std::pow (2.0f, pitch / 12.0f);
            
                // Clear the synthesis frame first
                std::fill (freezeSynthFrame.begin(), freezeSynthFrame.end(), 0.0f);
            
                int numBins = freezeFFTSize / 2 + 1; // only positive frequencies matter
            
                for (int i = 0; i < numBins; ++i)
                {
                    // Find the source bin in the frozen frame for this output bin
                    float sourceBinF = static_cast<float> (i) / pitchRatio;
                    int   sourceBin  = static_cast<int> (sourceBinF);
            
                    if (sourceBin >= numBins) break;
            
                    // Interpolate magnitude between adjacent bins for smoother shifting
                    float frac    = sourceBinF - static_cast<float> (sourceBin);
                    int   nextBin = std::min (sourceBin + 1, numBins - 1);
            
                    float re0 = freezeFrozenFrame[sourceBin * 2];
                    float im0 = freezeFrozenFrame[sourceBin * 2 + 1];
                    float re1 = freezeFrozenFrame[nextBin * 2];
                    float im1 = freezeFrozenFrame[nextBin * 2 + 1];
            
                    float mag0 = std::sqrt (re0 * re0 + im0 * im0);
                    float mag1 = std::sqrt (re1 * re1 + im1 * im1);
                    float mag  = mag0 + frac * (mag1 - mag0); // linear interp
            
                    // Random phase per hop — same as your existing approach
                    float randomPhase = freezeRandom.nextFloat()
                                        * juce::MathConstants<float>::twoPi;
            
                    freezeSynthFrame[i * 2]     = mag * std::cos (randomPhase);
                    freezeSynthFrame[i * 2 + 1] = mag * std::sin (randomPhase);
                }
            }
	    else
            {
                // Pass through live spectrum unchanged
                for (int i = 0; i < freezeFFTSize * 2; ++i)
                    freezeSynthFrame[i] = freezeWorkBuffer[i];
            } 
            // 5. INVERSE FFT
            freezeFFT.performRealOnlyInverseTransform (freezeSynthFrame.data());
    
            // 6. OVERLAP-ADD into output ring buffer
            // Standard OLA normalization for Hann window with 75% overlap
            // normFactor = hopSize / sum(window^2) ≈ 0.667 for Hann at 4x overlap
            float normFactor = static_cast<float> (freezeHopSize) / (freezeFFTSize * 0.375f);
    
            for (int i = 0; i < freezeFFTSize; ++i)
            {
                int outIdx = (freezeOutputReadPos + i) % (freezeFFTSize * 2);
                freezeOutputBuffer[outIdx] += freezeSynthFrame[i * 2]
                                              * freezeWindow[i] * normFactor;
            }
        }
        // 7. READ ONE SAMPLE FROM OUTPUT BUFFER
        float frozen = freezeOutputBuffer[freezeOutputReadPos];
        freezeOutputBuffer[freezeOutputReadPos] = 0.0f; // clear after read
        freezeOutputReadPos = (freezeOutputReadPos + 1) % (freezeFFTSize * 2);
        // 8. SMOOTH MIX — 50ms ramp to prevent clicks
        float targetMix = (freeze > 0.5f) ? blend : 0.0f;
        float rampSpeed = 1.0f / (static_cast<float> (sampleRate) * 0.05f);
        freezeMix = freezeMix + (targetMix - freezeMix) * rampSpeed;
        freezeMix = juce::jlimit (0.0f, 1.0f, freezeMix);
    
        float output = frozen * freezeMix + input * (1.0f - freezeMix);
        return std::isfinite (output) ? output : input;
    }

    float processSampleFilterDrive (float input, float cutoffNorm, float resonanceNorm, float driveNorm, float modeNorm, double sampleRate)
    {
        // Knob 1: Cutoff Frequency mapped exponentially (20Hz to 16kHz)
        float cutoffHz = 20.0f + (cutoffNorm * cutoffNorm) * 15980.0f;
        
        // Knob 2: Resonance Q factor (0.5 to 7.0)
        float q = 0.5f + resonanceNorm * 6.5f;
        float precalcK = 1.0f / q;
        
        // Core SVF Coeffs
        float x = juce::MathConstants<float>::pi * cutoffHz / static_cast<float> (sampleRate);
        float g_mod = std::tan (x);
        float h_mod = 1.0f / (1.0f + g_mod * (g_mod + precalcK));
        
        // Filter processing
        float hp = (input - (g_mod + precalcK) * fd_s1 - fd_s2) * h_mod;
        float bp = g_mod * hp + fd_s1;
        float lp = g_mod * bp + fd_s2;
        
        fd_s1 = 2.0f * bp - fd_s1;
        fd_s2 = 2.0f * lp - fd_s2;
        
        // Latch stable mode selection
        float effected = (modeNorm < 0.5f) ? lp : hp;
        
        // Knob 3: Overdrive circuit multiplier (1x to 16x)
        float driveAmt = 1.0f + driveNorm * 15.0f;
        float driven = effected * driveAmt;
        
        // Nonlinear soft clipping saturation
        float saturated = std::tanh (driven);
        
        // Gain compensation to balance out severe drive amounts
        float outputGain = 1.0f / std::sqrt (driveAmt);
        return saturated * outputGain;
    }
    
    float processSampleThreeBandEQ (float input, float lowGainNorm, float midGainNorm, float highGainNorm, float masterGain, double sampleRate)
    {
        // Map Knob 1-3 from 0.0-1.0 to professional EQ cuts/boosts (-24 dB to +12 dB)
        float lowGain  = juce::Decibels::decibelsToGain (juce::jmap (lowGainNorm,  0.0f, 1.0f, -24.0f, 12.0f));
        float midGain  = juce::Decibels::decibelsToGain (juce::jmap (midGainNorm,  0.0f, 1.0f, -24.0f, 12.0f));
        float highGain = juce::Decibels::decibelsToGain (juce::jmap (highGainNorm, 0.0f, 1.0f, -24.0f, 12.0f));
        
        // Crossover 1: Low-to-Mid Crossover fixed at 240 Hz (Linkwitz-Riley Q layout approximation)
        float wl = juce::MathConstants<float>::pi * 240.0f / static_cast<float> (sampleRate);
        float gl = std::tan (wl);
        float hl = 1.0f / (1.0f + gl * (gl + 1.4142f));
        
        float hp_l = (input - (gl + 1.4142f) * eq_s1_low - eq_s2_low) * hl;
        float bp_l = gl * hp_l + eq_s1_low;
        float lp_l = gl * bp_l + eq_s2_low;
        eq_s1_low = 2.0f * bp_l - eq_s1_low;
        eq_s2_low = 2.0f * lp_l - eq_s2_low;
        
        // Crossover 2: Mid-to-High Crossover fixed at 2600 Hz
        float wh = juce::MathConstants<float>::pi * 2600.0f / static_cast<float> (sampleRate);
        float gh = std::tan (wh);
        float hh = 1.0f / (1.0f + gh * (gh + 1.4142f));
        
        float hp_h = (input - (gh + 1.4142f) * eq_s1_high - eq_s2_high) * hh;
        float bp_h = gh * hp_h + eq_s1_high;
        float lp_h = gh * bp_h + eq_s2_high;
        eq_s1_high = 2.0f * bp_h - eq_s1_high;
        eq_s2_high = 2.0f * lp_h - eq_s2_high;
        
        // Perfect phase-complementary split execution
        float lowBand  = lp_l;
        float highBand = hp_h;
        float midBand  = input - lowBand - highBand; 
        
        // Apply independent lane gains and mix down with Master Volume Trim (Knob 4)
        float outputMix = (lowBand * lowGain) + (midBand * midGain) + (highBand * highGain);
        return outputMix * masterGain;
    }

    float processSampleTimeCtrlDelay (float input, float timeNorm, float feedbackNorm, float dampingNorm, float driveNorm, double sampleRate)
    {
        // Target delay tracking (30ms to 1000ms)
        float targetDelayMs = juce::jmap (timeNorm, 0.0f, 1.0f, 30.0f, 1000.0f);
        float targetSamples = targetDelayMs / 1000.0f * static_cast<float> (sampleRate);
        float smoothingCoeff = 0.0005f; 
        tcSmoothedSamples   += smoothingCoeff * (targetSamples - tcSmoothedSamples);
        
        float readPos = static_cast<float> (tcWritePtr) - tcSmoothedSamples;
        while (readPos < 0.0f) readPos += static_cast<float> (tcDelayBufferSize);
        float delayedSample = hermiteInterp (tcDelayBuffer.data(), tcDelayBufferSize, readPos);
        
        // Filters inside the feedback loop
        // Lowpass (Damping)
        float dampCutoff = juce::jmap (1.0f - dampingNorm, 0.0f, 1.0f, 200.0f, 20000.0f);
        float dampAlpha  = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * dampCutoff / static_cast<float> (sampleRate));
        tcDampingState  += dampAlpha * (delayedSample - tcDampingState);
        
        // Strips out subsonic buildup so self-oscillation stays in the musical mid-range
        float hpAlpha = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 80.0f / static_cast<float> (sampleRate));
        tcHighPassState += hpAlpha * (tcDampingState - tcHighPassState);
        float effectedSample = tcDampingState - tcHighPassState;
    
        // Bounded Tape Saturation
        // Allow feedback to push slightly past 1.0 (up to 1.2) for true dub self-oscillation
        float dubFeedback = juce::jlimit (0.0f, 1.2f, feedbackNorm * 1.3f); 
        float driveAmt = 1.0f + driveNorm * 9.0f; 
        float tapeInput = input + (effectedSample * dubFeedback);
        float tapedSignal = std::tanh (tapeInput * driveAmt) / driveAmt;
        tcDelayBuffer[tcWritePtr] = tapedSignal;
        tcWritePtr = (tcWritePtr + 1) % tcDelayBufferSize;
        // Scale the heavily compressed buffer sample back up so it sounds loud
        return delayedSample * driveAmt; 
    }

    float processSampleLoFi (float input, float decimateNorm, float bitsNorm, float wearNorm, float effectNorm, double sampleRate)
    {
        // 1. DIGITAL DECIMATION (Sample Rate Reduction)
        // Maps from 1 (no downsampling) to 40 (heavy robotic downsampling)
        float maxSampleHold = 40.0f;
        float samplePeriod = 1.0f + (decimateNorm * (maxSampleHold - 1.0f));
        
        lofiDownsampleCounter += 1.0f;
        if (lofiDownsampleCounter >= samplePeriod)
        {
            lofiDownsampleCounter -= samplePeriod;
            lofiHoldSample = input;
        }
        float output = lofiHoldSample;
    
        // 2. BITCRUSHING (Bit Depth Quantization)
        if (bitsNorm > 0.0f)
        {
            // Smoothly map from 16-bit down to a gritty 2-bit crunch
            float levels = juce::jmap (1.0f - bitsNorm, 0.0f, 1.0f, 4.0f, 65536.0f);
            output = std::round (output * levels) / levels;
        }
    
        // 3. ANALOG WEAR (Tape Drive + Vinyl Dust Crackle)
        // Linear Congruential Generator for ultra-fast, thread-safe noise
        lofiNoiseSeed = lofiNoiseSeed * 196314165 + 907633515;
        float noise = static_cast<float> (lofiNoiseSeed) / 4294967295.0f; // 0.0 to 1.0
        
        // Generate random vinyl "dust spikes" based on the wear parameter
        float crackle = 0.0f;
        if (wearNorm > 0.05f)
        {
            float crackleThreshold = 1.0f - (wearNorm * 0.0005f); // Frequency of pops
            if (noise > crackleThreshold)
            {
                // Alternate between positive and negative transient pops
                crackle = ((noise - crackleThreshold) / (1.0f - crackleThreshold)) * 0.2f;
                if (lofiNoiseSeed % 2 == 0) crackle *= -1.0f;
            }
        }
        
        // Inject crackle and apply tape-style saturation drive
        float driveAmt = 1.0f + (wearNorm * 4.0f);
        output = std::tanh ((output + crackle) * driveAmt) / std::tanh (driveAmt);
    
        // 4. VINYL FILTER (Dynamic Vintage Bandpass Taper)
        // As effectNorm increases, it narrows the frequency spectrum into a gramophone/phone sound
        float lpCutoff = juce::jmap (1.0f - effectNorm, 0.0f, 1.0f, 400.0f, 18000.0f);
        float hpCutoff = juce::jmap (effectNorm, 0.0f, 1.0f, 20.0f, 500.0f);
        
        // 1-Pole Lowpass Stage
        float lpAlpha = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * lpCutoff / static_cast<float> (sampleRate));
        lofiLPState  += lpAlpha * (output - lofiLPState);
        output = lofiLPState;
        
        // 1-Pole Highpass Stage
        float hpAlpha = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * hpCutoff / static_cast<float> (sampleRate));
        lofiHPState  += hpAlpha * (output - lofiHPState);
        output = output - lofiHPState;
    
        return std::isfinite (output) ? output : 0.0f;
    }

    // TRUE STEREO EFFECTS

    StereoOutput processSampleStereoAllpassReverb (float inL, float inR, SynthEffect& rightCh, 
                                                   float size, float decay, float diffusion, float damping, double sr)
    {
        // 1. Pre-diffusion
        float diffCoeff = diffusion * 0.7f;
        float diffL = inL;
        float diffR = inR;

        for (int i = 0; i < numApStages; ++i)
        {
            diffL = processAllpass (diffL, diffCoeff, apStageBuffers[i], apStageWritePtrs[i], primeDelays[i]);
            diffR = rightCh.processAllpass (diffR, diffCoeff, rightCh.apStageBuffers[i], rightCh.apStageWritePtrs[i], primeDelays[i]);
        }

        // 2. Tank Read
        float sizeMs = juce::jmap (size, 0.0f, 1.0f, 20.0f, 500.0f);
        int tankSamples = juce::jlimit (1, apRevBufferSize - 1, static_cast<int> (sizeMs / 1000.0f * sr));

        int readPtrL = (apRevWritePtr - tankSamples + apRevBufferSize) % apRevBufferSize;
        int readPtrR = (rightCh.apRevWritePtr - tankSamples + apRevBufferSize) % apRevBufferSize;

        float tankOutL = apRevBuffer[readPtrL];
        float tankOutR = rightCh.apRevBuffer[readPtrR];

        // 3. Damping LP Filters
        float dampCutoff = juce::jmap (1.0f - damping, 0.0f, 1.0f, 20.0f, 20000.0f);
        float dampAlpha  = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * dampCutoff / static_cast<float> (sr));
        
        apRevLastSample += dampAlpha * (tankOutL - apRevLastSample);
        rightCh.apRevLastSample += dampAlpha * (tankOutR - rightCh.apRevLastSample);

        // 4. CROSS-TALK FEEDBACK
        // Blend 70% of the channel's own feedback with 30% of the opposite channel's feedback
        float crossL = apRevLastSample * 0.7f + rightCh.apRevLastSample * 0.3f;
        float crossR = rightCh.apRevLastSample * 0.7f + apRevLastSample * 0.3f;

        float decayAmt = juce::jlimit (0.0f, 0.93f, decay);
        apRevBuffer[apRevWritePtr] = diffL + crossL * decayAmt;
        rightCh.apRevBuffer[rightCh.apRevWritePtr] = diffR + crossR * decayAmt;

        apRevWritePtr = (apRevWritePtr + 1) % apRevBufferSize;
        rightCh.apRevWritePtr = (rightCh.apRevWritePtr + 1) % apRevBufferSize;

        // 5. Post-diffusion
        float outL = tankOutL;
        float outR = tankOutR;
        for (int i = numRevStages / 2; i < numRevStages; ++i)
        {
            int pIdx = i - (numRevStages / 2);
            outL = processAllpass (outL, diffCoeff, apRevStageBuffers[i], apRevStageWritePtrs[i], revPrimeDelays[pIdx]);
            outR = rightCh.processAllpass (outR, diffCoeff, rightCh.apRevStageBuffers[i], rightCh.apRevStageWritePtrs[i], revPrimeDelays[pIdx]);
        }

        return { outL + inL * 0.1f, outR + inR * 0.1f };
    }

    StereoOutput processSampleStereoAmbientDelay (float inL, float inR, SynthEffect& rightCh, 
                                                  float time, float feedback, float shimmer, float diffusion, double sr)
    {
        float diffCoeff = diffusion * 0.7f;
        float diffL = inL; float diffR = inR;

        for (int i = 0; i < numApStages; ++i)
        {
            diffL = processAllpass (diffL, diffCoeff, apStageBuffers[i], apStageWritePtrs[i], primeDelays[i]);
            diffR = rightCh.processAllpass (diffR, diffCoeff, rightCh.apStageBuffers[i], rightCh.apStageWritePtrs[i], primeDelays[i]);
        }
    
        float delayMs = juce::jmap (time, 0.0f, 1.0f, 100.0f, 16000.0f);
        int delaySamples = juce::jlimit (1, ambientBufferSize - 1, static_cast<int> (delayMs / 1000.0f * sr));
    
        int readPtrL = (ambientWritePtr - delaySamples + ambientBufferSize) % ambientBufferSize;
        int readPtrR = (rightCh.ambientWritePtr - delaySamples + ambientBufferSize) % ambientBufferSize;
        
        float delayedL = ambientBuffer[readPtrL];
        float delayedR = rightCh.ambientBuffer[readPtrR];
    
        // Calculate Shimmer (Independent per channel to widen the pitch variance)
        float shimmeredL = 0.0f; float shimmeredR = 0.0f;
        if (shimmer > 0.001f)
        {
            // Left Shimmer
            shimmerBuffer[shimmerWritePtr] = delayedL;
            shimmerWritePtr = (shimmerWritePtr + 1) % shimmerBufferSize;
            shimmerReadPtr += 2.0f;
            if (shimmerReadPtr >= static_cast<float>(shimmerBufferSize)) shimmerReadPtr -= static_cast<float>(shimmerBufferSize);
            shimmeredL = hermiteInterp (shimmerBuffer.data(), shimmerBufferSize, shimmerReadPtr);
            shimmerPhase += 2.0f / static_cast<float> (shimmerBufferSize);
            if (shimmerPhase >= 1.0f) shimmerPhase -= 1.0f;
            shimmeredL *= 0.5f * (1.0f - std::cos (shimmerPhase * juce::MathConstants<float>::twoPi));

            // Right Shimmer
            rightCh.shimmerBuffer[rightCh.shimmerWritePtr] = delayedR;
            rightCh.shimmerWritePtr = (rightCh.shimmerWritePtr + 1) % shimmerBufferSize;
            rightCh.shimmerReadPtr += 2.0f;
            if (rightCh.shimmerReadPtr >= static_cast<float>(shimmerBufferSize)) rightCh.shimmerReadPtr -= static_cast<float>(shimmerBufferSize);
            shimmeredR = hermiteInterp (rightCh.shimmerBuffer.data(), shimmerBufferSize, rightCh.shimmerReadPtr);
            rightCh.shimmerPhase += 2.0f / static_cast<float> (shimmerBufferSize);
            if (rightCh.shimmerPhase >= 1.0f) rightCh.shimmerPhase -= 1.0f;
            shimmeredR *= 0.5f * (1.0f - std::cos (rightCh.shimmerPhase * juce::MathConstants<float>::twoPi));
        }
    
        // PING-PONG CROSS-TALK: Feed Right delay into Left input, and Left into Right
        float feedbackAmt = juce::jlimit (0.0f, 0.98f, feedback);
        float feedL = std::tanh(delayedR + shimmeredR * shimmer);
        float feedR = std::tanh(delayedL + shimmeredL * shimmer);
    
        ambientBuffer[ambientWritePtr] = diffL + feedL * feedbackAmt;
        rightCh.ambientBuffer[rightCh.ambientWritePtr] = diffR + feedR * feedbackAmt;
        
        ambientWritePtr = (ambientWritePtr + 1) % ambientBufferSize;
        rightCh.ambientWritePtr = (rightCh.ambientWritePtr + 1) % ambientBufferSize;
    
        float outL = delayedL; float outR = delayedR;
        for (int i = numAmbientStages / 2; i < numAmbientStages; ++i)
        {
            int pIdx = i - (numAmbientStages / 2);
            outL = processAllpass (outL, diffCoeff, ambientStageBuffers[i], ambientStageWritePtrs[i], ambientPrimeDelays[pIdx]);
            outR = rightCh.processAllpass (outR, diffCoeff, rightCh.ambientStageBuffers[i], rightCh.ambientStageWritePtrs[i], ambientPrimeDelays[pIdx]);
        }
        
        return { std::isfinite(outL) ? outL + inL * 0.3f : inL, 
                 std::isfinite(outR) ? outR + inR * 0.3f : inR };
    }

    StereoOutput processSampleStereoAllpassDelay (float inL, float inR, SynthEffect& rightCh, 
                                                  float time, float feedback, float diffusion, float damping, double sr)
    {
        // Diffuse
        float diffCoeff = diffusion * 0.7f;
        float diffL = inL; float diffR = inR;
        for (int i = 0; i < numApStages; ++i)
        {
            diffL = processAllpass (diffL, diffCoeff, apStageBuffers[i], apStageWritePtrs[i], primeDelays[i]);
            diffR = rightCh.processAllpass (diffR, diffCoeff, rightCh.apStageBuffers[i], rightCh.apStageWritePtrs[i], primeDelays[i]);
        }

        // Delay lines
        float delayMs = juce::jmap (time, 0.0f, 1.0f, 10.0f, 1000.0f);
        int delaySamples = juce::jlimit (1, apDelayBufferSize - 1, static_cast<int> (delayMs / 1000.0f * sr));
        
        int readPtrL = (apDelayWritePtr - delaySamples + apDelayBufferSize) % apDelayBufferSize;
        int readPtrR = (rightCh.apDelayWritePtr - delaySamples + apDelayBufferSize) % apDelayBufferSize;
        
        float delayedL = apDelayBuffer[readPtrL];
        float delayedR = rightCh.apDelayBuffer[readPtrR];

        // Damping
        float dampCutoff = juce::jmap (1.0f - damping, 0.0f, 1.0f, 20.0f, 20000.0f);
        float dampAlpha  = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * dampCutoff / static_cast<float> (sr));
        apDelayLastSample += dampAlpha * (delayedL - apDelayLastSample);
        rightCh.apDelayLastSample += dampAlpha * (delayedR - rightCh.apDelayLastSample);

        // PING PONG FEEDBACK
        float feedbackAmt = juce::jlimit (0.0f, 0.95f, feedback);
        apDelayBuffer[apDelayWritePtr] = diffL + rightCh.apDelayLastSample * feedbackAmt;
        rightCh.apDelayBuffer[rightCh.apDelayWritePtr] = diffR + apDelayLastSample * feedbackAmt;
        
        apDelayWritePtr = (apDelayWritePtr + 1) % apDelayBufferSize;
        rightCh.apDelayWritePtr = (rightCh.apDelayWritePtr + 1) % apDelayBufferSize;

        return { delayedL + inL, delayedR + inR };
    }

    StereoOutput processSampleStereoChorus (float inL, float inR, SynthEffect& rightCh, 
                                            float rate, float depth, float spread, float voices, double sr)
    {
        // Force right channel's LFOs to be offset by 90 degrees per voice to create stereo width
        for (int v = 0; v < numChorusVoices; ++v) {
            rightCh.chorusLFOPhases[v] = chorusLFOPhases[v] + (juce::MathConstants<float>::pi * 0.5f);
            if (rightCh.chorusLFOPhases[v] >= juce::MathConstants<float>::twoPi) 
                rightCh.chorusLFOPhases[v] -= juce::MathConstants<float>::twoPi;
        }

        // Call the underlying mono processes now that phases are offset
        return { this->processSampleChorus (inL, rate, depth, spread, voices, sr),
                 rightCh.processSampleChorus (inR, rate, depth, spread, voices, sr) };
    }

    StereoOutput processSampleStereoOldChorus (float inL, float inR, SynthEffect& rightCh, 
                                               float rate, float depth, float mode, float warmth, double sr)
    {
        // Juno style: Invert the right channel LFO exactly 180 degrees (Pi)
        rightCh.oldChorusLFOPhase = oldChorusLFOPhase + juce::MathConstants<float>::pi;
        if (rightCh.oldChorusLFOPhase >= juce::MathConstants<float>::twoPi) 
            rightCh.oldChorusLFOPhase -= juce::MathConstants<float>::twoPi;

        return { this->processSampleOldChorus (inL, rate, depth, mode, warmth, sr),
                 rightCh.processSampleOldChorus (inR, rate, depth, mode, warmth, sr) };
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

    // Tape effect state
    float tapePhase        = 0.0f;  // wobble LFO phase
    float tapeDelayBuffer[8192] { 0.0f }; // short delay for wow/flutter
    int   tapeWritePtr     = 0;
    float tapeReadPtr      = 0.0f;
    float tapeLastSample   = 0.0f; // for age/HF loss one-pole effect
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

    // Distortion state
    float distToneState  = 0.0f; // one-pole tone effect state
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
    float djfxJumpTimer = 0.0f; float djfxLoopStart = -1.0f; // -1 means no loop active
    juce::Random djfxRandom;

    // Color Bass state
    static constexpr int numResonatorBands = 9;
    std::array<float, numResonatorBands> resS1 { 0.0f };
    std::array<float, numResonatorBands> resS2 { 0.0f };
    std::array<float, numResonatorBands> resB0 { 0.0f }, resB2 { 0.0f };
    std::array<float, numResonatorBands> resA1 { 0.0f }, resA2 { 0.0f };
    
    float cachedDrive        = -1.0f;
    float cachedTone         = -1.0f;
    float cachedDecay        = -1.0f;
    float cachedDetectedFreq =  0.0f;
    
    static constexpr int kGrainSize = 512;
    std::array<float, kGrainSize * 2> grainBuf {};
    int   grainWritePos = 0;
    float pitchPhase    = 0.0f;
    
    float tiltLowZ         = 0.0f;
    float zcPrev           = 0.0f;
    float zcPeriodSamples  = 0.0f;
    int   zcSampleCount    = 0;
    float detectedFreq     = 80.0f;

    // Ambient Delay state. First Delay itself
    static constexpr int ambientBufferSize = 384000; // 8 seconds at 48kHz
    std::array<float, ambientBufferSize> ambientBuffer { 0.0f };
    int   ambientWritePtr    = 0;
    float ambientLastSample  = 0.0f; 
    // Shimmer pitch shifter state (simple octave-up via phase vocoder approximation)
    static constexpr int shimmerBufferSize = 4096;
    std::array<float, shimmerBufferSize> shimmerBuffer { 0.0f };
    int   shimmerWritePtr = 0;
    float shimmerReadPtr  = 0.0f;
    float shimmerPhase    = 0.0f;
    // Ambient diffusion stages
    static constexpr int numAmbientStages = 6;
    static constexpr int ambientStageSize = 8192;
    std::array<std::array<float, ambientStageSize>, numAmbientStages> ambientStageBuffers {};
    std::array<int, numAmbientStages> ambientStageWritePtrs { 0, 0, 0, 0, 0, 0 };

    // Old Chorus (Juno-style BBD) state
    static constexpr int oldChorusDelaySize = 48000;
    std::array<float, oldChorusDelaySize> oldChorusDelayBuffer { 0.0f };
    int   oldChorusWritePtr  = 0;
    float oldChorusLFOPhase  = 0.0f;
    float oldChorusLastSample = 0.0f;

    // OTT state
    static constexpr int numOTTBands = 3;
    // Per-band crossover effects (Linkwitz-Riley style using two SVF stages)
    float ottLP1_s1[numOTTBands] { 0.0f }, ottLP1_s2[numOTTBands] { 0.0f };
    float ottLP2_s1[numOTTBands] { 0.0f }, ottLP2_s2[numOTTBands] { 0.0f };
    float ottHP1_s1[numOTTBands] { 0.0f }, ottHP1_s2[numOTTBands] { 0.0f };
    float ottHP2_s1[numOTTBands] { 0.0f }, ottHP2_s2[numOTTBands] { 0.0f };
    // Per-band compressor envelopes (upward and downward)
    float ottEnvUp[numOTTBands]   { 0.0f };
    float ottEnvDown[numOTTBands] { 0.0f };
    // Crossover frequencies
    static constexpr float ottLowMidFreq  = 200.0f;
    static constexpr float ottMidHighFreq = 2500.0f;
    float ottToneLast = 0.0f;

    // Spectral Freeze state
    static constexpr int freezeFFTOrder = 14; // this is the power of 2 that defines the FFT size. 2^11 = 2048. 2^14 = 16384
    static constexpr int freezeFFTSize  = 1 << freezeFFTOrder; // 2048
    static constexpr int freezeHopSize  = freezeFFTSize / 4;   // 512
    juce::dsp::FFT freezeFFT { freezeFFTOrder };
    std::array<float, freezeFFTSize * 2> freezeInputBuffer  { 0.0f };
    std::array<float, freezeFFTSize * 2> freezeFrozenFrame  { 0.0f };
    std::array<float, freezeFFTSize * 2> freezeOutputBuffer { 0.0f };
    std::array<float, freezeFFTSize>     freezeWindow       { 0.0f };
    std::array<float, freezeFFTSize * 2> freezeWorkBuffer   { 0.0f };
    std::array<float, freezeFFTSize * 2> freezeSynthFrame   { 0.0f }; // moved to member
    int   freezeInputWritePos  = 0;
    int   freezeOutputReadPos  = 0;
    int   freezeHopCounter     = 0;
    bool  freezeHasFrozenFrame = false;
    float freezeMix            = 0.0f;
    juce::Random freezeRandom; // dedicated RNG, not system random

    // Filter + Drive State
    float fd_s1 = 0.0f;
    float fd_s2 = 0.0f;

    // 3-Band EQ State
    float eq_s1_low = 0.0f;
    float eq_s2_low = 0.0f;
    float eq_s1_high = 0.0f;
    float eq_s2_high = 0.0f;

    // Time Control Delay State
    static constexpr int tcDelayBufferSize = 96000;
    std::array<float, tcDelayBufferSize> tcDelayBuffer { 0.0f };
    int tcWritePtr = 0; float tcSmoothedSamples = 0.0f;
    float tcDampingState = 0.0f; float tcHighPassState = 0.0f;
    // Lo-Fi Effect State
    float lofiDownsampleCounter = 0.0f;
    float lofiHoldSample        = 0.0f;
    float lofiLPState           = 0.0f;
    float lofiHPState           = 0.0f;
    uint32_t lofiNoiseSeed      = 123456789; // Fast LCG random seed for vinyl dust
    // primes for delays
    static constexpr std::array<int, 4> primeDelays        = { 211, 347, 523, 701 };
    static constexpr std::array<int, 4> revPrimeDelays     = { 883, 1031, 1153, 1301 };
    static constexpr std::array<int, 4> ambientPrimeDelays = { 1511, 1699, 1861, 2029 };
    // Parameters Cache
    double sampleRate { 44100.0 };
    float targetCutoff { 1000.0f };
    float targetResonance { 0.707f };
    EffectType currentType { Lowpass };
};
