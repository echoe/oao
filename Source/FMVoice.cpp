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
        check("FILTER_TYPE_" + opNum, opParams[i].effectType);
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

    // Wire up the macros
    for (int m = 0; m < ProjectConfig::numMacros; ++m)
    {
        juce::String s = juce::String (m + 1);
        macroVal[m]  = apvts.getRawParameterValue ("MACRO_VAL_"   + s);
        if (!macroVal[m])  DBG ("CRITICAL: MACRO_VAL_"   + s + " not found!");

        static const char* macroLetters[] = { "A", "B", "C", "D" };
        for (int t = 0; t < ProjectConfig::numMacroTargets; ++t)
        {
            juce::String letter = macroLetters[t];
            macroTgt[m][t] = apvts.getRawParameterValue ("MACRO_TGT_" + letter + "_" + s);
            macroAmt[m][t] = apvts.getRawParameterValue ("MACRO_AMT_" + letter + "_" + s);

            if (!macroTgt[m][t]) DBG ("CRITICAL: MACRO_TGT_" + letter + "_" + s + " not found!");
            if (!macroAmt[m][t]) DBG ("CRITICAL: MACRO_AMT_" + letter + "_" + s + " not found!");
        }
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
    currentVelocity.store(velocity, std::memory_order_relaxed); // so we can modulate things with this if desired
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
// So we can modulate things with the mod wheel ...
void FMVoice::controllerMoved(int controllerNumber, int newControllerValue)
{
    if (controllerNumber == 1) // CC1 = mod wheel
        currentModWheel.store(newControllerValue / 127.0f, std::memory_order_relaxed);
}

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
        operators[i].resetEnvelope(); 
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
    std::array<int, ProjectConfig::numOperators> cachedModes, cachedShapes, cachedEffectTypes, cachedFreqModes;
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
        cachedEffectTypes[i] = static_cast<int>(safeLoad(opParams[i].effectType, 0.0f));
        cachedFreqModes[i] = static_cast<int> (safeLoad (opParams[i].freqMode, 0.0f));
    }

    // prep for block audio handling
    for (int opIdx = 0; opIdx < ProjectConfig::numOperators; ++opIdx)
    {
        operators[opIdx].prepareForBlock();
    }

    // Here's our sample-rate DSP loop
    for (int sample = 0; sample < numSamples; ++sample)
    {
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
        
        // matrix modulation storage
        float matrixModOffsets[ProjectConfig::numOperators][ProjectConfig::numOperators] {};

        // Shared routing: given a target index (from ModChoices::targets()) and a signal,
        // adds that signal into the right per-operator/per-fx/per-matrix-cell accumulator.
        // Used both by the general mod matrix slots and by the macros below, so a macro's
        // two targets and a mod slot's single target all resolve identically.
        auto applyToTarget = [&] (int tgtIdx, float srcSignal)
        {
            if (tgtIdx == 0 || std::abs (srcSignal) < 0.0001f)
                return;

            const int opTargetEnd = 1 + (ProjectConfig::numOperators * ProjectConfig::numOpParams) - 1;
            const int fxTargetStart = opTargetEnd + 1;
            const int fxTargetEnd   = fxTargetStart + (ProjectConfig::numEffects * ProjectConfig::numFxParams) - 1;
            const int matrixTargetStart = fxTargetEnd + 1;
            const int matrixTargetEnd   = matrixTargetStart + (ProjectConfig::numOperators * ProjectConfig::numOperators) - 1;

            if (tgtIdx >= 1 && tgtIdx <= opTargetEnd) //operators
            {
                int opIdx    = (tgtIdx - 1) / ProjectConfig::numOpParams;
                int paramIdx = (tgtIdx - 1) % ProjectConfig::numOpParams;
                float scaled = srcSignal * ProjectConfig::modRangeForParam[paramIdx];

                switch (paramIdx)
                {
                    case 0: ratioModOffsets[opIdx]  += scaled; break;
                    case 1: detuneModOffsets[opIdx] += scaled; break;
                    case 2: phaseModOffsets[opIdx]  += scaled; break;
                    case 3: foldModOffsets[opIdx]   += scaled; break;
                    case 4: levelModOffsets[opIdx]  += scaled; break;
                }
            }
            else if (tgtIdx >= fxTargetStart && tgtIdx <= fxTargetEnd) //effects
            {
                int fxIdx    = (tgtIdx - fxTargetStart) / ProjectConfig::numFxParams;
                int paramIdx = (tgtIdx - fxTargetStart) % ProjectConfig::numFxParams;
                float scaled = srcSignal * ProjectConfig::modRangeForParam[paramIdx];

                switch (paramIdx)
                {
                    case 0: fxRatioMods[fxIdx].store  (fxRatioMods[fxIdx].load()  + scaled); break;
                    case 1: fxDetuneMods[fxIdx].store (fxDetuneMods[fxIdx].load() + scaled); break;
                    case 2: fxPhaseMods[fxIdx].store  (fxPhaseMods[fxIdx].load()  + scaled); break;
                    case 3: fxFoldMods[fxIdx].store   (fxFoldMods[fxIdx].load()   + scaled); break;
                    case 4: fxLevelMods[fxIdx].store  (fxLevelMods[fxIdx].load()  + scaled); break;
                }
            }
            else if (tgtIdx >= matrixTargetStart && tgtIdx <= matrixTargetEnd) //mod matrix
            {
                int matrixIdx = tgtIdx - matrixTargetStart;
                int src       = matrixIdx / ProjectConfig::numOperators;         // dynamic division based on operator count
                int dst       = matrixIdx % ProjectConfig::numOperators;         // dynamic modulo based on operator count

                // Accumulate into a separate modulation array (0..1 "how much FM" space —
                // see maxFmModulationIndex where this is actually consumed)
                matrixModOffsets[src][dst] += srcSignal * ProjectConfig::modRangeForMatrixCell;
            }
        };

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
            // We get different sources depending on the mod slot. This follows the list in Constants.h
            float rawSrc = 0.0f;
            if (srcIdx >= 1 && srcIdx <= 6)
            {
                rawSrc = lastOpOutputs[srcIdx - 1];
            }
            else if (srcIdx >= 7 && srcIdx <= 9)
            {
                rawSrc = fxLfoOutputs[srcIdx - 7].load(std::memory_order_relaxed);
            }
            else if (srcIdx == 10)
            {
                rawSrc = currentVelocity.load(std::memory_order_relaxed);
            }
            else if (srcIdx == 11)
            {
                rawSrc = currentModWheel.load(std::memory_order_relaxed);
            }

            applyToTarget (tgtIdx, rawSrc * amt);
	}

        // Macros — each has one bipolar value routed at up to numMacroTargets independent
        // targets, each scaled by its own amount knob.
        for (int m = 0; m < ProjectConfig::numMacros; ++m)
        {
            if (!macroVal[m])
                continue;

            float val = macroVal[m]->load (std::memory_order_relaxed);
            if (std::abs (val) < 0.0001f)
                continue;

            for (int t = 0; t < ProjectConfig::numMacroTargets; ++t)
            {
                if (!macroTgt[m][t])
                    continue;

                int   tgt = static_cast<int> (macroTgt[m][t]->load (std::memory_order_relaxed));
                float amt = macroAmt[m][t] ? macroAmt[m][t]->load (std::memory_order_relaxed) : 1.0f;

                applyToTarget (tgt, val * amt);
            }
        }

        // Process the operators!
        for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
        {
            float modulationSum = 0.0f;
            float audioInputSum = 0.0f;

            for (int src = 0; src < ProjectConfig::numOperators; ++src)
            {
                // matrixParams/matrixModOffsets live in a normalized 0..1 "how much FM"
                // space; maxFmModulationIndex converts that into the actual phase-modulation
                // index, so 1.0 always means "maximum DX7-style FM depth" regardless of source.
                float modDepth = (safeLoad(matrixParams[src][dest]) + matrixModOffsets[src][dest])
                                    * ProjectConfig::maxFmModulationIndex;
                if (modDepth > 0.0f)
                {
                    float modSignal = (src == dest) ? (lastOpOutputs[src] + previousOpOutputs[src]) * 0.5f : lastOpOutputs[src];
                    modulationSum += modSignal * modDepth;
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
                cachedEffectTypes[dest], 
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

    if (!isAnyEnvelopeActive)
    {
        clearCurrentNote();
        resetVoiceState();
    }
}
