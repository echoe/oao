//FMVoice.cpp
#include "Constants.h"
#include "FMVoice.h"
#include "WaveTable.h"

FMVoice::FMVoice()
{
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        for (int j = 0; j < ProjectConfig::numOperators; ++j)
        {
            matrixParams[i][j] = nullptr;
            audioMatrixParams[i][j] = nullptr;
        }
    }
}

bool FMVoice::canPlaySound (juce::SynthesiserSound* sound) 
{ 
    return true; 
}

void FMVoice::prepare (double sampleRate, int samplesPerBlock, WaveTable* wt)
{
    waveTable = wt;
    double safeRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        operators[i].prepare (safeRate, wt);
    }
}

void FMVoice::initParameters (juce::AudioProcessorValueTreeState& apvts)
{
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        juce::String opNum = juce::String (i + 1);
	// Debugigng check to make sure we have parameters
        auto check = [&](juce::String id, std::atomic<float>*& ptr) {
            ptr = apvts.getRawParameterValue(id);
            if (ptr == nullptr) 
                DBG("CRITICAL: Parameter " + id + " not found!");
        };

        check("MODE_" + opNum, opParams[i].mode);
        check("WAVE_SHAPE_" + opNum, opParams[i].wave);
        check("FILTER_TYPE_" + opNum, opParams[i].filterType);
        check("RATIO_" + opNum, opParams[i].ratio);
        check("DETUNE_" + opNum, opParams[i].detune);
        check("PHASE_" + opNum, opParams[i].phase);
        check("FOLD_" + opNum, opParams[i].fold);
        check("OUT_" + opNum, opParams[i].out);
        check("ATTACK_" + opNum, opParams[i].attack);
        check("DECAY_" + opNum, opParams[i].decay);
        check("SUSTAIN_" + opNum, opParams[i].sustain);
        check("RELEASE_" + opNum, opParams[i].release);
	check("FREQ_MODE_" +opNum, opParams[i].freqMode);
	for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
        {
            matrixParams[i][dest] = apvts.getRawParameterValue ("MOD_" + juce::String (i) + "_" + juce::String (dest));
            audioMatrixParams[i][dest] = apvts.getRawParameterValue ("AUDIO_ROUTE_" + juce::String (i) + "_" + juce::String (dest));

        }
    }
    // Wire up the 6 mod slots
    for (int slot = 0; slot < ProjectConfig::numModSlots; ++slot)
    {
        juce::String s = juce::String (slot + 1);
        modSlotSrc[slot] = apvts.getRawParameterValue ("MOD_SRC_" + s);
        modSlotTgt[slot] = apvts.getRawParameterValue ("MOD_TGT_" + s);
        modSlotAmt[slot] = apvts.getRawParameterValue ("MOD_AMT_" + s);

        if (!modSlotSrc[slot]) DBG ("CRITICAL: MOD_SRC_" + s + " not found!");
        if (!modSlotTgt[slot]) DBG ("CRITICAL: MOD_TGT_" + s + " not found!");
        if (!modSlotAmt[slot]) DBG ("CRITICAL: MOD_AMT_" + s + " not found!");
    }
}

void FMVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
    prepare (newRate, 0, waveTable);
}

void FMVoice::setOversamplingFactor (int factor)
{
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
        operators[i].setOversamplingFactor (factor);
}

void FMVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int)
{
    baseFrequency = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    level = velocity;
    resetVoiceState();
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        if (opParams[i].attack != nullptr)
        {
            float initPhase = opParams[i].phase->load (std::memory_order_relaxed);
            operators[i].resetPhase (initPhase);
            
            juce::ADSR::Parameters p;
            p.attack  = opParams[i].attack->load (std::memory_order_relaxed);
            p.decay   = opParams[i].decay->load (std::memory_order_relaxed);
            p.sustain = opParams[i].sustain->load (std::memory_order_relaxed);
            p.release = opParams[i].release->load (std::memory_order_relaxed);
            
            operators[i].noteOn (p);
        }
    }
}

void FMVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff) 
    { 
        for (auto& op : operators) op.noteOff(); 
    }
    else 
    { 
        clearCurrentNote(); 
        resetVoiceState();
    }
}

void FMVoice::pitchWheelMoved (int) {}
void FMVoice::controllerMoved (int, int) {}

void FMVoice::setDAWTempo (float newBPM) noexcept
{
    currentBPM.store (newBPM, std::memory_order_relaxed);
}

void FMVoice::resetVoiceState()
{
    lastOpOutputs.fill (0.0f);
    previousOpOutputs.fill (0.0f);
    processedOpOutputs.fill (0.0f);

    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        operators[i].resetPhase (0.0f); 
    }
}

void FMVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (!isVoiceActive()) return;

    float currentBpmValue = currentBPM.load(std::memory_order_relaxed);

    auto safeLoad = [](std::atomic<float>* ptr, float fallback = 0.0f) -> float {
        return ptr != nullptr ? ptr->load(std::memory_order_relaxed) : fallback;
    };

    // Refresh ADSR parameters every block in case they changed
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        juce::ADSR::Parameters p;
        p.attack  = safeLoad (opParams[i].attack,  0.1f);
        p.decay   = safeLoad (opParams[i].decay,   0.2f);
        p.sustain = safeLoad (opParams[i].sustain, 0.8f);
        p.release = safeLoad (opParams[i].release, 0.5f);
        operators[i].setEnvelopeParameters (p);
    }

    // Cache parameters for all operators.
    std::array<float, ProjectConfig::numOperators> cachedRatios, cachedDetunes, cachedPhases, cachedFolds, cachedOuts;
    std::array<int, ProjectConfig::numOperators> cachedModes, cachedShapes, cachedFilterTypes;
    std::array<bool, ProjectConfig::numOperators> cachedFreqModes;
    std::array<float, ProjectConfig::numOperators> cachedAttacks, cachedDecays, cachedSustains, cachedReleases;

    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        cachedRatios[i]    = safeLoad(opParams[i].ratio, 1.0f); 
        cachedDetunes[i]   = safeLoad(opParams[i].detune, 0.0f);
        cachedPhases[i]    = safeLoad(opParams[i].phase, 0.0f);
        cachedFolds[i]     = safeLoad(opParams[i].fold, 0.0f);
        cachedOuts[i]      = safeLoad(opParams[i].out, 0.0f);
        cachedModes[i]       = static_cast<int>(safeLoad(opParams[i].mode, 0.0f));
        cachedShapes[i]      = static_cast<int>(safeLoad(opParams[i].wave, 0.0f));
        cachedFilterTypes[i] = static_cast<int>(safeLoad(opParams[i].filterType, 0.0f));
        cachedFreqModes[i] = static_cast<int> (safeLoad (opParams[i].freqMode, 0.0f));
    }

    // Here's our sample-rate DSP loop
    for (int sample = 0; sample < numSamples; ++sample)
    {
	// Pass external audio to operators
	for (int i = 0; i < ProjectConfig::numOperators; ++i)
            operators[i].setExternalAudioSample (externalAudioL, externalAudioR);

	// set outputs and offsets, to be updated later in this loop
        std::array<float, ProjectConfig::numOperators> opOutputs { 0.0f };
        std::array<float, ProjectConfig::numOperators> pitchModOffsets { 0.0f };
        std::array<float, ProjectConfig::numOperators> phaseModOffsets { 0.0f };
        std::array<float, ProjectConfig::numOperators> foldModOffsets { 0.0f };
        std::array<float, ProjectConfig::numOperators> levelModOffsets { 0.0f };
        std::array<float, ProjectConfig::numOperators> cutoffModOffsets { 0.0f };

        // Per-operator mod accumulators
        std::array<float, ProjectConfig::numOperators> ratioModOffsets  { 0.0f };
        std::array<float, ProjectConfig::numOperators> detuneModOffsets { 0.0f };
        
        for (int slot = 0; slot < ProjectConfig::numModSlots; ++slot)
        {
            if (!modSlotSrc[slot] || !modSlotTgt[slot] || !modSlotAmt[slot])
                continue;
        
            int   srcIdx = static_cast<int> (modSlotSrc[slot]->load (std::memory_order_relaxed));
            int   tgtIdx = static_cast<int> (modSlotTgt[slot]->load (std::memory_order_relaxed));
            float amt    = modSlotAmt[slot]->load (std::memory_order_relaxed);
        
            // Index 0 = "None" for either source or target — skip
            if (srcIdx == 0 || tgtIdx == 0 || std::abs (amt) < 0.0001f)
                continue;
        
            // Source signal: Op 1 = index 1, so operator index = srcIdx - 1
            float srcSignal = lastOpOutputs[srcIdx - 1] * amt;
        
            // tgtIdx 1-30: laid out as blocks of 5 per operator
	    if (tgtIdx >= 1 && tgtIdx <= 30)
            {
                int opIdx    = (tgtIdx - 1) / 5;   // 0-5
                int paramIdx = (tgtIdx - 1) % 5;   // 0-4
        
                switch (paramIdx)
                {
                    case 0: ratioModOffsets[opIdx]  += srcSignal; break;
                    case 1: detuneModOffsets[opIdx] += srcSignal; break;
                    case 2: phaseModOffsets[opIdx]  += srcSignal; break;
                    case 3: foldModOffsets[opIdx]   += srcSignal; break;
                    case 4: levelModOffsets[opIdx]  += srcSignal; break;
                }
            }
	    else if (tgtIdx >= 31 && tgtIdx <= 45) // 31-45 for the effect slots, which you can target with modulation
            {
                int fxIdx    = (tgtIdx - 31) / 5;  // 0-2 (which fx slot)
                int paramIdx = (tgtIdx - 31) % 5;  // 0-4 (which parameter)
            
                switch (paramIdx)
                {
                    case 0: fxRatioMods[fxIdx].store  (fxRatioMods[fxIdx].load()  + srcSignal); break;
                    case 1: fxDetuneMods[fxIdx].store (fxDetuneMods[fxIdx].load() + srcSignal); break;
                    case 2: fxPhaseMods[fxIdx].store  (fxPhaseMods[fxIdx].load()  + srcSignal); break;
                    case 3: fxFoldMods[fxIdx].store   (fxFoldMods[fxIdx].load()   + srcSignal); break;
                    case 4: fxLevelMods[fxIdx].store  (fxLevelMods[fxIdx].load()  + srcSignal); break;
                }
            }
        }

        // Process the operators!
        for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
        {
            float modulationSum = 0.0f;
            float audioInputSum = 0.0f;

            for (int src = 0; src < ProjectConfig::numOperators; ++src)
            {
                float modIndex = safeLoad(matrixParams[src][dest]); 
                if (modIndex > 0.0f)
                {
                    float modSignal = (src == dest) ? (lastOpOutputs[src] + previousOpOutputs[src]) * 0.5f : lastOpOutputs[src];
                    modulationSum += modSignal * modIndex;
                }

                float audioGain = safeLoad(audioMatrixParams[src][dest]); 
                if (audioGain > 0.0f)
                {
                    audioInputSum += lastOpOutputs[src] * audioGain;
                }
            }
            audioInputSum = std::tanh (audioInputSum);

            // Throw everything into the operator
            opOutputs[dest] = operators[dest].processSample (
                baseFrequency, 
                currentBpmValue,
                cachedRatios[dest] + ratioModOffsets[dest], 
                cachedDetunes[dest] + detuneModOffsets[dest], 
                cachedPhases[dest], 
                cachedFolds[dest],
                audioInputSum, 
                modulationSum,
                pitchModOffsets[dest], 
                phaseModOffsets[dest], 
                cutoffModOffsets[dest], 
                foldModOffsets[dest],
                cachedModes[dest], 
                cachedShapes[dest], 
                cachedFilterTypes[dest], 
                cachedFreqModes[dest]
            );

            // Process outputs and send them out
            processedOpOutputs[dest] = opOutputs[dest];

            if (std::abs(levelModOffsets[dest]) > 0.001f)
            {
                processedOpOutputs[dest] *= juce::jlimit(0.0f, 2.0f, 1.0f + levelModOffsets[dest]);
            }
        }

        previousOpOutputs = lastOpOutputs;
        lastOpOutputs = processedOpOutputs;

        // Mixbus main output
        float mixSample = 0.0f;
        int   activeOps = 0;
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            if (cachedOuts[i] > 0.001f)
            {
                mixSample += processedOpOutputs[i] * cachedOuts[i];
                activeOps++;
            }
        }
        
        float polyphonyCushion = activeOps > 0
            ? 1.0f / std::sqrt (static_cast<float> (activeOps))
            : 1.0f;
        float finalGain = level * polyphonyCushion * 0.5f;
        
        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
            outputBuffer.addSample (channel, startSample + sample, mixSample * finalGain);
    }
    // Cleanup - check if voice is finished
    bool isAnyEnvelopeActive = false;
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        if (operators[i].isActive())
        {
            isAnyEnvelopeActive = true;
            break;
        }
    }

    if (!isAnyEnvelopeActive && !alwaysActive)
    {
        clearCurrentNote();
        resetVoiceState();
    }
}
