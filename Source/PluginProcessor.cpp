// PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Constants.h"

FMPluginAudioProcessor::FMPluginAudioProcessor()
    : AudioProcessor (BusesProperties()
#ifdef OAO_FX_ONLY
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)  // required in FX mode
#else
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), false) // optional in synth mode
#endif
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
#ifndef OAO_FX_ONLY
    for (int i = 0; i < 8; ++i)
    {
        auto* voice = new FMVoice();
        voice->initParameters (apvts);
        for (int opIdx = 0; opIdx < ProjectConfig::numOperators; ++opIdx)
            if (loadedSamples[opIdx] != nullptr)
                voice->setSampleData (opIdx, loadedSamples[opIdx]);
        synth.addVoice (voice);
    }
    synth.addSound (new FMSound());
#endif
    for (int i = 0; i < ProjectConfig::numEffects; ++i)
    {
        juce::String s = juce::String (i + 1);
        fxTypeParams[i]   = apvts.getRawParameterValue ("FX_TYPE_"   + s);
        fxMixParams[i]    = apvts.getRawParameterValue ("FX_MIX_"    + s);
        fxRatioParams[i]  = apvts.getRawParameterValue ("FX_RATIO_"  + s);
        fxDetuneParams[i] = apvts.getRawParameterValue ("FX_DETUNE_" + s);
        fxPhaseParams[i]  = apvts.getRawParameterValue ("FX_PHASE_"  + s);
        fxFoldParams[i]   = apvts.getRawParameterValue ("FX_FOLD_"   + s);
        //debug checks 
        jassert (fxTypeParams[i]   != nullptr);
        jassert (fxMixParams[i]    != nullptr);
        jassert (fxRatioParams[i]  != nullptr);
    }
}

FMPluginAudioProcessor::~FMPluginAudioProcessor() {}

bool FMPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#ifdef OAO_FX_ONLY
    // FX mode requires stereo input
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
#else
    // Synth mode: input can be stereo or disabled
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo() &&
        !layouts.getMainInputChannelSet().isDisabled())
        return false;
#endif

    return true;
}

juce::AudioProcessorValueTreeState::ParameterLayout FMPluginAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    juce::StringArray modeChoices { "Wave", "Additive", "Effect", "Sample" };
    juce::StringArray waveShapeChoices { "Sine", "Triangle", "Saw", "Square", "Pulse", "SquarePWM", "White Noise", "Pink Noise" };
    juce::StringArray freqModeChoices { "Standard", "Sync", "Hz", "LFO" };
    juce::StringArray lfosyncRates = { "8/1", "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" }; // for Sync LFOs
    auto effectTypeChoices = ProjectConfig::getEffectTypeChoices();

    // Setting up an option to use when/if you see too many decimal attributes ...
    auto twoDecimalAttributes = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([](float value, int) 
        { 
            return juce::String (value, 2); 
        });

    // Generate parameters for each operator dynamically
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        // ALL KNOB SETTINGS ARE HERE
        juce::String opNum = juce::String (i + 1);
        // Wave
        params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "MODE_" + opNum, 1 }, "Op " + opNum + " Mode", modeChoices, 0));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "WAVE_SHAPE_" + opNum, 1 }, "Op " + opNum + " Wave Shape", waveShapeChoices, 0));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "FILTER_TYPE_" + opNum, 1 }, "Op " + opNum + " Effect Type", effectTypeChoices, 0));
        // Tempo Sync/hz lock/LFO mode
        params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "FREQ_MODE_" + opNum, 1 }, "Op " + opNum + " Freq Mode", freqModeChoices, 0));
	// Osc Settings - Ratios, Detune, Phase, Fold. We repurpose these for everything
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"RATIO_" + opNum, 1}, "Op " + opNum + " Ratio", 0.01f, 16.0f, 1.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"DETUNE_" + opNum, 1}, "Op " + opNum + " Detune", -50.0f, 50.0f, 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "PHASE_" + opNum, 1 }, "Op " + opNum + " Phase", 0.0f, 360.0f, 0.0f));
	params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "FOLD_" + opNum, 1 }, "Op " + opNum + " Fold", 0.0f, 1.0f, 0.0f));
        // Envelopes
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"ATTACK_" + opNum, 1}, "Op " + opNum + " Attack", 0.001f, 5.0f, 0.1f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"DECAY_" + opNum, 1}, "Op " + opNum + " Decay", 0.01f, 5.0f, 0.2f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"SUSTAIN_" + opNum, 1}, "Op " + opNum + " Sustain", 0.0f, 1.0f, 0.8f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"RELEASE_" + opNum, 1}, "Op " + opNum + " Release", 0.01f, 5.0f, 0.5f));
        // Output Levels
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"OUT_" + opNum, 1}, "Op " + opNum + " Out Level", 0.0f, 1.0f, (i == 0) ? 1.0f : 0.0f));
    }
    // Effects page — slots, each with type, sync, mix, and 4 knobs
    for (int i = 0; i < ProjectConfig::numEffects; ++i)
    {
        juce::String s = juce::String (i + 1);
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { "FX_TYPE_" + s, 1 }, "FX " + s + " Type", effectTypeChoices, 0));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "FX_MIX_" + s, 1 }, "FX " + s + " Mix", 0.0f, 1.0f, 1.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "FX_RATIO_" + s, 1 }, "FX " + s + " Ratio", 0.01f, 16.0f, 1.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "FX_DETUNE_" + s, 1 }, "FX " + s + " Detune", -50.0f, 50.0f, 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "FX_PHASE_" + s, 1 }, "FX " + s + " Phase", 0.0f, 360.0f, 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "FX_FOLD_" + s, 1 }, "FX " + s + " Fold", 0.0f, 1.0f, 0.0f));
    }

    // Effects LFO Parameters
    juce::StringArray lfoWaveChoices   { "Sine", "Triangle", "Saw", "Square" };
    auto              lfoTargetChoices = ModChoices::fxtargets();
    for (int i = 0; i < ProjectConfig::numEffects; ++i)
    {
        juce::String s = juce::String (i + 1);
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { "FX_LFO_WAVE_" + s, 1 }, "FX LFO " + s + " Wave", lfoWaveChoices, 0));
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { "FX_LFO_SYNC_" + s, 1 }, "FX LFO " + s + " Sync", false));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "FX_LFO_RATE_" + s, 1 }, "FX LFO " + s + " Rate",
            juce::NormalisableRange<float> (0.0f, 20.0f, 0.0f, 1.0f), 1.0f, twoDecimalAttributes)); // don't show full float
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID ( "FX_LFO_RATE_SYNC_" + s, 1), "Rate Sync", lfosyncRates, 5));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "FX_LFO_DEPTH_" + s, 1 }, "FX LFO " + s + " Depth", -1.0f, 1.0f, 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { "FX_LFO_TGT_" + s, 1 }, "FX LFO " + s + " Target", lfoTargetChoices, 0));
    }

    // Generate Modulation Matrix Nodes (NxN Grid)
    for (int src = 0; src < ProjectConfig::numOperators; ++src)
    {
        for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
        {
            juce::String paramID = "MOD_" + juce::String (src) + "_" + juce::String (dest);
            juce::String name = "Mod Op " + juce::String (src + 1) + " -> Op " + juce::String (dest + 1);
            params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {paramID, 1}, name, 0.0f, 10.0f, 0.0f));
        }
    }

    // Audio Routing Matrix Nodes (NxN Grid)
    for (int src = 0; src < ProjectConfig::numOperators; ++src)
    {
        for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
        {
            juce::String paramID = "AUDIO_ROUTE_" + juce::String (src) + "_" + juce::String (dest);
            juce::String name = "Audio Matrix: Op " + juce::String (src + 1) + " -> Op " + juce::String (dest + 1);
            params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {paramID, 1}, name, 0.0f, 1.0f, 0.0f));
        }
    }

    // --- MODULATION MATRIX: 6 SLOTS ---
    auto modSourceChoices = ModChoices::sources();
    auto modTargetChoices = ModChoices::targets();

    for (int slot = 1; slot <= 6; ++slot)
    {
        juce::String s = juce::String (slot);
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { "MOD_SRC_" + s, 1 },
            "Mod Source " + s, modSourceChoices, 0));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { "MOD_TGT_" + s, 1 },
            "Mod Target " + s, modTargetChoices, 0));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "MOD_AMT_" + s, 1 },
            "Mod Amount " + s, -1.0f, 1.0f, 0.0f));
    }
    
    // Gain
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
    juce::ParameterID { "GAIN_CEIL", 1 }, "Gain Ceiling", juce::NormalisableRange<float> (-24.0f, 0.0f, 0.1f),-0.2f)); // Ranges from -24dB to 0dB, default at -0.2dB

    // This line has to be the end of this function, otherwise stuff won't process
    return { params.begin(), params.end() };
}

void FMPluginAudioProcessor::setOversamplingFactor (int factor)
{
    int order = 0;
    if (factor == 2) order = 1;
    if (factor == 4) order = 2;
    if (factor == 8) order = 3;
    currentOversamplingFactor = factor;
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        getTotalNumOutputChannels(), order,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
    oversampling->initProcessing (getBlockSize());

#ifndef OAO_FX_ONLY
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
        {
            voice->setOversamplingFactor (factor);
            voice->prepare (getSampleRate(), getBlockSize(), &waveTable);
        }
#endif
}

void FMPluginAudioProcessor::setPolyphony (int numVoices)
{
#ifndef OAO_FX_ONLY
    synth.clearVoices();

    for (int i = 0; i < numVoices; ++i)
    {
        auto* voice = new FMVoice();
        voice->initParameters (apvts);
        synth.addVoice (voice);
    }

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
        {
            voice->setCurrentPlaybackSampleRate (getSampleRate());
            voice->prepare (getSampleRate(), getBlockSize(), &waveTable);
            voice->setOversamplingFactor (currentOversamplingFactor);
        }
    }
#endif
}

void FMPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
#ifndef OAO_FX_ONLY
    waveTable.prepare();
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
        {
            voice->setCurrentPlaybackSampleRate (sampleRate);
            voice->prepare (sampleRate, samplesPerBlock, &waveTable);
        }
#endif
    setOversamplingFactor (1);
    for (int i = 0; i < ProjectConfig::numEffects; ++i)
    {
	for (int ch = 0; ch < 2; ++ch)
	{
        	fxEffects[i][ch].prepare (sampleRate);
        	fxEffects[i][ch].reset();
	}
    }
    // Start LFOs — cache APVTS param pointers at prepare time
    for (int i = 0; i < ProjectConfig::numEffects; ++i)
        fxLfo[i].prepare (sampleRate, apvts, i);
    // Pre-allocate the input capture buffer using the max block size
    inputCapture.setSize (getTotalNumInputChannels(), samplesPerBlock);
    inputCapture.clear(); // Good practice to zero it out initially
}

void FMPluginAudioProcessor::releaseResources() {}

void FMPluginAudioProcessor::updateVoices()
{
    // Read current atomic parameters from APVTS and safely pass them to our synthesis engine
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
        {
            // In a production build, add a thread-safe method inside `FMVoice` (e.g., voice->updateParameters(...)) to read these parameters atomically.
        }
    }
}

void FMPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    //defaults, inputs, outputs
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
#ifdef OAO_FX_ONLY
    // FX mode: input audio passes straight through to the effects chain.
    for (int ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
#else
    // Synth mode: read tempo, update voices, render synth.
    float activeBPM = 120.0f;
    if (auto* playHead = getPlayHead())
    {
        auto positionInfo = playHead->getPosition();
        if (positionInfo.hasValue())
            activeBPM = static_cast<float> (positionInfo->getBpm().orFallback (120.0));
    }
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
            voice->setDAWTempo (activeBPM);

    // Capture input before clearing (external audio ops, if ever re-added, would use this)
    if (inputCapture.getNumSamples() >= buffer.getNumSamples())
    {
        for (int ch = 0; ch < totalNumInputChannels; ++ch)
            inputCapture.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());
    }
    DBG ("after clear - inputCh: " + juce::String (totalNumInputChannels) +
         " outputCh: " + juce::String (totalNumOutputChannels) +
         " sample0: " + juce::String (buffer.getSample (0, 0)));

    updateVoices();

    if (oversampling != nullptr && currentOversamplingFactor > 1)
    {
        juce::dsp::AudioBlock<float> inputBlock (buffer);
        auto oversampledBlock = oversampling->processSamplesUp (inputBlock);

        int oversampledSize = static_cast<int> (oversampledBlock.getNumSamples());

        float* channels[2];
        channels[0] = oversampledBlock.getChannelPointer (0);
        channels[1] = oversampledBlock.getNumChannels() > 1
                          ? oversampledBlock.getChannelPointer (1)
                          : oversampledBlock.getChannelPointer (0);

        juce::AudioBuffer<float> oversampledWrap (channels, 2, oversampledSize);

        juce::MidiBuffer scaledMidi;
        for (auto meta : midiMessages)
        {
            auto newPos = juce::jlimit (0, oversampledSize - 1,
                meta.samplePosition * currentOversamplingFactor);
            scaledMidi.addEvent (meta.getMessage(), newPos);
        }

        synth.renderNextBlock (oversampledWrap, scaledMidi, 0, oversampledSize);
        oversampling->processSamplesDown (inputBlock);
    }
    else
    {
        synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
    }
#endif // OAO_FX_ONLY

#ifndef OAO_FX_ONLY
    // Collect per-voice FX modulation (synth mode only — no voices in FX mode)
    std::array<float, ProjectConfig::numEffects> fxRatioModSum  { 0.0f, 0.0f, 0.0f };
    std::array<float, ProjectConfig::numEffects> fxDetuneModSum { 0.0f, 0.0f, 0.0f };
    std::array<float, ProjectConfig::numEffects> fxPhaseModSum  { 0.0f, 0.0f, 0.0f };
    std::array<float, ProjectConfig::numEffects> fxFoldModSum   { 0.0f, 0.0f, 0.0f };
    std::array<float, ProjectConfig::numEffects> fxLevelModSum  { 0.0f, 0.0f, 0.0f };

    for (int v = 0; v < synth.getNumVoices(); ++v)
    {
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (v)))
        {
            if (voice->isVoiceActive())
            {
                for (int i = 0; i < ProjectConfig::numEffects; ++i)
                {
                    fxRatioModSum[i]  += voice->fxRatioMods[i].load  (std::memory_order_relaxed);
                    fxDetuneModSum[i] += voice->fxDetuneMods[i].load (std::memory_order_relaxed);
                    fxPhaseModSum[i]  += voice->fxPhaseMods[i].load  (std::memory_order_relaxed);
                    fxFoldModSum[i]   += voice->fxFoldMods[i].load   (std::memory_order_relaxed);
                    fxLevelModSum[i]  += voice->fxLevelMods[i].load  (std::memory_order_relaxed);

                    voice->fxRatioMods[i].store  (0.0f, std::memory_order_relaxed);
                    voice->fxDetuneMods[i].store (0.0f, std::memory_order_relaxed);
                    voice->fxPhaseMods[i].store  (0.0f, std::memory_order_relaxed);
                    voice->fxFoldMods[i].store   (0.0f, std::memory_order_relaxed);
                    voice->fxLevelMods[i].store  (0.0f, std::memory_order_relaxed);
                }
            }
        }
    }
#else
    // No voice modulation in FX mode — zero everything
    std::array<float, ProjectConfig::numEffects> fxRatioModSum  { 0.0f, 0.0f, 0.0f };
    std::array<float, ProjectConfig::numEffects> fxDetuneModSum { 0.0f, 0.0f, 0.0f };
    std::array<float, ProjectConfig::numEffects> fxPhaseModSum  { 0.0f, 0.0f, 0.0f };
    std::array<float, ProjectConfig::numEffects> fxFoldModSum   { 0.0f, 0.0f, 0.0f };
    std::array<float, ProjectConfig::numEffects> fxLevelModSum  { 0.0f, 0.0f, 0.0f };
    // Also need activeBPM for tempo-sync in the FX loop
    float activeBPM = 120.0f;
    if (auto* playHead = getPlayHead())
    {
        auto positionInfo = playHead->getPosition();
        if (positionInfo.hasValue())
            activeBPM = static_cast<float> (positionInfo->getBpm().orFallback (120.0));
    }
#endif
    // Now we start the effects loop
    {
        auto* leftData   = buffer.getWritePointer (0);
        auto* rightData  = buffer.getWritePointer (1);
        int   numSamples = buffer.getNumSamples();

        for (int slot = 0; slot < ProjectConfig::numEffects; ++slot)
        {
            int effectType = static_cast<int> (fxTypeParams[slot]->load (std::memory_order_relaxed));

            for (int ch = 0; ch < 2; ++ch)
            {
                if (effectType != lastFxEffectType[slot])
                {
                    fxEffects[slot][ch].reset();
                    lastFxEffectType[slot] = effectType;
                }
            }

            if (effectType == 0) continue; // None — skip entirely

            // Base knob values + per-buffer voice mod sums (voice mods are per-buffer)
            float baseRatio  = fxRatioParams[slot]->load  (std::memory_order_relaxed) + fxRatioModSum[slot];
            float baseDetune = fxDetuneParams[slot]->load (std::memory_order_relaxed) + fxDetuneModSum[slot];
            float basePhase  = fxPhaseParams[slot]->load  (std::memory_order_relaxed) + fxPhaseModSum[slot];
            float baseFold   = fxFoldParams[slot]->load   (std::memory_order_relaxed) + fxFoldModSum[slot];
            float baseMix    = juce::jlimit (0.0f, 1.0f,
                                             fxMixParams[slot]->load (std::memory_order_relaxed) + fxLevelModSum[slot]);

            // Determine which LFO (if any) targets this slot, and which param
            int lfoTargetParam = -1;
            int lfoIndex       = -1;
            for (int lfo = 0; lfo < ProjectConfig::numEffects; ++lfo)
            {
                int t = fxLfo[lfo].getTarget();
                if (t >= 0 && t / 5 == slot)
                {
                    lfoTargetParam = t % 5;
                    lfoIndex       = lfo;
                    break; // one LFO per slot for now
                }
            }

            for (int i = 0; i < numSamples; ++i)
            {
                // Tick the LFO once per sample and apply to the right parameter
                float ratio  = baseRatio;
                float detune = baseDetune;
                float phase  = basePhase;
                float fold   = baseFold;
                float mix    = baseMix;

                if (lfoIndex >= 0)
                {
                    float lfoVal = fxLfo[lfoIndex].tick (activeBPM);
                    switch (lfoTargetParam)
                    {
                        case 0: ratio  += lfoVal * 16.0f;  break; // ratio range ~0-16
                        case 1: detune += lfoVal * 50.0f;  break; // detune range -50 to +50
                        case 2: phase  += lfoVal * 360.0f; break; // phase range 0-360
                        case 3: fold   += lfoVal;          break; // fold range 0-1
                        case 4: mix     = juce::jlimit (0.0f, 1.0f, mix + lfoVal); break; // mix 0-1
                        default: break;
                    }
                }

                float inL = leftData[i];
                float inR = rightData[i];
                float outL = inL;
                float outR = inR;

                // Shared pre-computations for filter-family effects
                float normalizedRatio = juce::jlimit (0.0f, 1.0f, (ratio - 0.01f) / (16.0f - 0.01f));
                float baseCutoff      = 20.0f * std::pow (1000.0f, normalizedRatio);
                float filterRes       = juce::jlimit (0.001f, 0.95f, (detune + 50.0f) / 100.0f);
                float coupledRes      = filterRes * filterRes;

                // TRUE STEREO EFFECTS
                // Each passes raw modulated values directly — no shared intermediary squashing
                if (effectType == 12) // Chorus: Rate | Depth | Spread | Voices
                {
                    float rate   = normalizedRatio;
                    float depth  = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                    float spread = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                    float voices = juce::jlimit (0.0f, 1.0f, fold);
                    auto out = fxEffects[slot][0].processSampleStereoChorus (inL, inR, fxEffects[slot][1], rate, depth, spread, voices, getSampleRate());
                    outL = std::isfinite(out.L) ? out.L : 0.0f;
                    outR = std::isfinite(out.R) ? out.R : 0.0f;
                }
                else if (effectType == 13) // Old Chorus: Rate | Depth | Mode | Warmth
                {
                    float rate   = normalizedRatio;
                    float depth  = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                    float mode   = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                    float warmth = juce::jlimit (0.0f, 1.0f, fold);
                    auto out = fxEffects[slot][0].processSampleStereoOldChorus (inL, inR, fxEffects[slot][1], rate, depth, mode, warmth, getSampleRate());
                    outL = std::isfinite(out.L) ? out.L : 0.0f;
                    outR = std::isfinite(out.R) ? out.R : 0.0f;
                }
                else if (effectType == 17) // AP Reverb: Size | Decay | Diffusion | Damping
                {
                    float size      = normalizedRatio;
                    float decay     = juce::jlimit (0.0f, 0.99f, (detune + 50.0f) / 100.0f);
                    float diffusion = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                    float damping   = juce::jlimit (0.0f, 1.0f, fold);
                    auto out = fxEffects[slot][0].processSampleStereoAllpassReverb (inL, inR, fxEffects[slot][1], size, decay, diffusion, damping, getSampleRate());
                    outL = std::isfinite(out.L) ? std::tanh(out.L) : 0.0f;
                    outR = std::isfinite(out.R) ? std::tanh(out.R) : 0.0f;
                }
                else if (effectType == 18) // AP Delay: Time | Feedback | Diffusion | Damping
                {
                    float time      = normalizedRatio;
                    float feedback  = juce::jlimit (0.0f, 0.95f, (detune + 50.0f) / 100.0f);
                    float diffusion = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                    float damping   = juce::jlimit (0.0f, 1.0f, fold);
                    auto out = fxEffects[slot][0].processSampleStereoAllpassDelay (inL, inR, fxEffects[slot][1], time, feedback, diffusion, damping, getSampleRate());
                    outL = std::isfinite(out.L) ? std::tanh(out.L) : 0.0f;
                    outR = std::isfinite(out.R) ? std::tanh(out.R) : 0.0f;
                }
                else if (effectType == 20) // Ambient Delay: Time | Feedback | Shimmer | Diffusion
                {
                    float time      = normalizedRatio;
                    float feedback  = juce::jlimit (0.0f, 0.95f, (detune + 50.0f) / 100.0f);
                    float shimmer   = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                    float diffusion = juce::jlimit (0.0f, 1.0f, fold);
                    auto out = fxEffects[slot][0].processSampleStereoAmbientDelay (inL, inR, fxEffects[slot][1], time, feedback, shimmer, diffusion, getSampleRate());
                    outL = std::isfinite(out.L) ? out.L : 0.0f;
                    outR = std::isfinite(out.R) ? out.R : 0.0f;
                }
                else
                {
                    // DUAL MONO EFFECTS
                    float inArr[2]  = { inL, inR };
                    float outArr[2] = { 0.0f, 0.0f };

                    for (int ch = 0; ch < 2; ++ch)
                    {
                        switch (effectType)
                        {
                            case 1: // Lowpass
                            case 2: // Highpass
                            case 3: // Bandpass — filters use baseCutoff/coupledRes (frequency mapping needed)
                            {
                                fxEffects[slot][ch].setResonance (coupledRes);
                                float k = fxEffects[slot][ch].getPrecalculatedK();
                                outArr[ch] = fxEffects[slot][ch].processSampleAudioRate (inArr[ch], baseCutoff, k);
                                break;
                            }
                            case 4: // Filter Drive: Cutoff | Resonance | Overdrive | Mode
                            {
                                float cutoff   = normalizedRatio;
                                float res      = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                                float drive    = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                                float LP_or_HP = juce::jlimit (0.0f, 1.0f, fold);
                                outArr[ch] = fxEffects[slot][ch].processSampleFilterDrive (inArr[ch], cutoff, res, drive, LP_or_HP, getSampleRate());
                                outArr[ch] = std::isfinite (outArr[ch]) ? outArr[ch] : 0.0f;
                                break;
                            }
                            case 5: // Comb: Cutoff | Damping | Keytrack | Feedback
                            {
                                float feedback = juce::jlimit (-0.95f, 0.95f, fold * 2.0f - 1.0f);
                                float damping  = juce::jlimit (0.001f, 0.95f, (detune + 50.0f) / 100.0f);
                                outArr[ch] = fxEffects[slot][ch].processSampleComb (inArr[ch], baseCutoff, feedback, damping);
                                outArr[ch] = std::isfinite (outArr[ch]) ? std::tanh (outArr[ch]) : 0.0f;
                                break;
                            }
                            case 6: // Formant: Vowel | Nasal | Vowel Mod | Drive
                            {
                                float baseVowel    = normalizedRatio * 4.0f;
                                float modDepth     = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                                float dynamicVowel = juce::jlimit (0.0f, 4.0f, baseVowel + (fxPhaseModSum[slot] * modDepth * 4.0f));
                                float qFactor      = juce::jmap (juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f), 0.0f, 1.0f, 2.0f, 15.0f);
                                float drive        = 1.0f + (juce::jlimit (0.0f, 1.0f, fold) * 4.0f);
                                float drivenInput  = std::tanh (inArr[ch] * drive);
                                float output       = fxEffects[slot][ch].processSampleFormant (drivenInput, dynamicVowel, qFactor);
                                outArr[ch]         = std::isfinite (output) ? std::tanh (output) : 0.0f;
                                break;
                            }
                            case 7: // Compressor: Threshold | Ratio | Attack | Release
                            {
                                float threshold = normalizedRatio;
                                float compRatio = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                                float attack    = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                                float release   = juce::jlimit (0.0f, 1.0f, fold);
                                outArr[ch] = fxEffects[slot][ch].processSampleCompressor (inArr[ch], threshold, compRatio, attack, release, getSampleRate());
                                outArr[ch] = std::isfinite (outArr[ch]) ? outArr[ch] : 0.0f;
                                break;
                            }
                            case 8: // 3-Band EQ: Low Gain | Mid Gain | High Gain | Master
                            {
                                float lowGain  = normalizedRatio;
                                float midGain  = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                                float highGain = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                                float master   = juce::jlimit (0.0f, 1.0f, fold);
                                outArr[ch] = fxEffects[slot][ch].processSampleThreeBandEQ (inArr[ch], lowGain, midGain, highGain, master, getSampleRate());
                                outArr[ch] = std::isfinite (outArr[ch]) ? outArr[ch] : 0.0f;
                                break;
                            }
                            case 9: // OTT: Depth | Time | Upward | Tone
                            {
                                float depth    = normalizedRatio;
                                float timeKnob = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                                float upward   = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                                float tone     = juce::jlimit (0.0f, 1.0f, fold);
                                outArr[ch] = fxEffects[slot][ch].processSampleOTT (inArr[ch], depth, timeKnob, upward, tone, getSampleRate());
                                outArr[ch] = std::isfinite (outArr[ch]) ? outArr[ch] : 0.0f;
                                break;
                            }
                            case 10: // Lo-Fi: Decimate | Bits | Wear | Tone
                            {
                                float decimate = normalizedRatio;
                                float bits     = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                                float wear     = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                                float tone     = juce::jlimit (0.0f, 1.0f, fold);
                                outArr[ch] = fxEffects[slot][ch].processSampleLoFi (inArr[ch], decimate, bits, wear, tone, getSampleRate());
                                outArr[ch] = std::isfinite (outArr[ch]) ? outArr[ch] : 0.0f;
                                break;
                            }
                            case 11: // Tape: Wobble | Age | Saturation | Bias
                            {
                                float wobbleRate = normalizedRatio;
                                float age        = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                                float saturation = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                                float bias       = juce::jlimit (0.0f, 1.0f, fold);
                                outArr[ch] = fxEffects[slot][ch].processSampleTape (inArr[ch], wobbleRate, age, saturation, bias, getSampleRate());
                                outArr[ch] = std::isfinite (outArr[ch]) ? outArr[ch] : 0.0f;
                                break;
                            }
                            case 14: // Distortion: Drive | Flavor | Tone | Degrade
                            {
                                float drive       = normalizedRatio;
                                float flavor      = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                                float toneKnob    = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                                float degradation = juce::jlimit (0.0f, 1.0f, fold);
                                outArr[ch] = fxEffects[slot][ch].processSampleDistortion (inArr[ch], drive, flavor, toneKnob, degradation, getSampleRate());
                                outArr[ch] = std::isfinite (outArr[ch]) ? outArr[ch] : 0.0f;
                                break;
                            }
                            case 15: // Bitcrush: Bits | Rate | Jitter | Noise
                            {
                                float bits   = normalizedRatio;
                                float rate   = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                                float jitter = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                                float noise  = juce::jlimit (0.0f, 1.0f, fold);
                                outArr[ch] = fxEffects[slot][ch].processSampleBitcrush (inArr[ch], bits, rate, jitter, noise, getSampleRate());
                                outArr[ch] = std::isfinite (outArr[ch]) ? outArr[ch] : 0.0f;
                                break;
                            }
                            case 16: // Ring Mod: Frequency | Shape | Depth | Feedback
                            {
                                float frequency       = 20.0f * std::pow (250.0f, normalizedRatio);
				float shape     = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                                float depth     = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                                float feedback  = juce::jlimit (0.0f, 0.95f, fold);
                                outArr[ch] = fxEffects[slot][ch].processSampleRingMod (inArr[ch], frequency, shape, depth, feedback, getSampleRate());
                                outArr[ch] = std::isfinite (outArr[ch]) ? outArr[ch] : 0.0f;
                                break;
                            }
                            case 19: // Time Control Delay: Time | Feedback | Damping | Drive
                            {
                                float delayTime = normalizedRatio;
                                float feedback  = juce::jlimit (0.0f, 0.95f, (detune + 50.0f) / 100.0f);
                                float damping   = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                                float drive     = juce::jlimit (0.0f, 1.0f, fold);
                                outArr[ch] = fxEffects[slot][ch].processSampleTimeCtrlDelay (inArr[ch], delayTime, feedback, damping, drive, getSampleRate());
                                outArr[ch] = std::isfinite (outArr[ch]) ? outArr[ch] : 0.0f;
                                break;
                            }
                            case 21: // DJFX Delay: Buffer | Speed | Loop On | Drift
                            {
                                float bufferAmt = normalizedRatio;
                                float speed     = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                                float on        = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                                float drift     = juce::jlimit (0.0f, 1.0f, fold);
                                outArr[ch] = fxEffects[slot][ch].processSampleDJFXDelay (inArr[ch], bufferAmt, speed, on, drift, getSampleRate());
                                outArr[ch] = std::isfinite (outArr[ch]) ? outArr[ch] : 0.0f;
                                break;
                            }
                            case 22: // Scatter: Type | Size | Speed | Depth
                            {
                                float pattern = normalizedRatio;
                                float size    = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                                float speed   = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                                float depth   = juce::jlimit (0.0f, 1.0f, fold);
                                outArr[ch] = fxEffects[slot][ch].processSampleScatter (inArr[ch], pattern, size, speed, depth, getSampleRate());
                                outArr[ch] = std::isfinite (outArr[ch]) ? outArr[ch] : 0.0f;
                                break;
                            }
                            case 23: // Granular: Grain Size | Damping | Scatter | Feedback
                            {
                                float grainDurationMs = juce::jmap (normalizedRatio, 0.0f, 1.0f, 10.0f, 1000.0f);
                                float damping         = juce::jlimit (0.0f, 0.95f, (detune + 50.0f) / 100.0f);
                                float scatterAmt      = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                                float feedback        = juce::jlimit (-0.95f, 0.95f, fold * 2.0f - 1.0f);
                                outArr[ch] = fxEffects[slot][ch].processSampleGranular (inArr[ch], baseCutoff, scatterAmt, grainDurationMs, feedback, damping);
                                outArr[ch] = std::isfinite (outArr[ch]) ? std::tanh (outArr[ch]) : 0.0f;
                                break;
                            }
                            case 24: // Color Bass: Drive | Shimmer | Tone | Decay
                            {
                                float drive   = juce::jlimit (0.0f, 1.0f, normalizedRatio);
                                float shimmer = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
				float tone    = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                                float decay   = juce::jlimit (0.0f, 1.0f, fold);
                                outArr[ch]    = fxEffects[slot][ch].processSampleColorBass (inArr[ch], drive, shimmer, tone, decay, getSampleRate());
                                outArr[ch] = std::isfinite (outArr[ch]) ? outArr[ch] : 0.0f;
                                break;
                            }
                            case 25: // Spectral Freeze: Freeze On | Blend | Pitch | Blur
                            {
                                float freeze     = normalizedRatio;
                                float blend      = juce::jlimit (0.0f, 1.0f, (detune + 50.0f) / 100.0f);
                                float rawPitch   = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                                float pitchRatio = std::pow (2.0f, (rawPitch - 0.5f) * 48.0f / 12.0f);
                                float blur       = juce::jlimit (0.0f, 1.0f, fold);
                                outArr[ch] = fxEffects[slot][ch].processSampleSpectralFreeze (inArr[ch], freeze, blend, pitchRatio, blur, getSampleRate());
                                outArr[ch] = std::isfinite (outArr[ch]) ? outArr[ch] : 0.0f;
                                break;
                            }
                            default:
                                outArr[ch] = inArr[ch];
                                break;
                        }
                    }

                    // Assign mono outputs back to main stereo channels
                    outL = outArr[0];
                    outR = outArr[1];
                }
                
                // Wet/dry mix applied discretely to L and R using the correct scope variables
                leftData[i]  = outL * mix + inL * (1.0f - mix);
                rightData[i] = outR * mix + inR * (1.0f - mix);
            }
        }
    }

    // Master Gain
    juce::dsp::AudioBlock<float> block (buffer);
    if (auto* gainParam = apvts.getRawParameterValue ("GAIN_CEIL"))
        block.multiplyBy (juce::Decibels::decibelsToGain (gainParam->load()));

    // mix L+R to mono to feed oscilloscope
    {
        auto* leftData  = buffer.getReadPointer (0);
        auto* rightData = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : leftData;
        int writePos    = scopeWritePos.load (std::memory_order_relaxed);
    
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            scopeBuffer[writePos] = (leftData[i] + rightData[i]) * 0.5f;
            writePos = (writePos + 1) % scopeBufferSize;
        }
        scopeWritePos.store (writePos, std::memory_order_relaxed);
    }

    // Output (soft clipping)
    for (int channel = 0; channel < totalNumOutputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            channelData[sample] = std::tanh(channelData[sample] * 0.8f);
    }
    DBG ("end of processBlock - sample0: " + juce::String (buffer.getSample (0, 0)));
}

void FMPluginAudioProcessor::loadSampleForOperator (int opIndex, const juce::File& file)
{
    if (opIndex < 0 || opIndex >= ProjectConfig::numOperators)
        return;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
    {
        DBG ("loadSampleForOperator: could not read file " + file.getFullPathName());
        return;
    }

    auto newBuffer = std::make_shared<juce::AudioBuffer<float>> (
        static_cast<int> (reader->numChannels),
        static_cast<int> (reader->lengthInSamples));

    reader->read (newBuffer.get(), 0, newBuffer->getNumSamples(), 0, true, true);

    float peak = newBuffer->getMagnitude (0, newBuffer->getNumSamples());
    if (peak > 0.0001f)
        newBuffer->applyGain (1.0f / peak);

    loadedSamples[opIndex]      = newBuffer;
    loadedSampleNames[opIndex]  = file.getFileNameWithoutExtension();

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
            voice->setSampleData (opIndex, newBuffer);

    DBG ("loadSampleForOperator: loaded " + file.getFileName()
         + " for op " + juce::String (opIndex + 1)
         + " (" + juce::String (newBuffer->getNumSamples()) + " samples)");
}

juce::AudioProcessorEditor* FMPluginAudioProcessor::createEditor()
{
    return new FMPluginAudioProcessorEditor (*this);
}

void FMPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    // Embed any loaded samples as base64-encoded WAV data so presets are self-contained.
    auto* samplesNode = xml->createNewChildElement ("SampleData");
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    for (int opIdx = 0; opIdx < ProjectConfig::numOperators; ++opIdx)
    {
        auto& buf = loadedSamples[opIdx];
        if (buf == nullptr || buf->getNumSamples() == 0)
            continue;

        // Encode to WAV in memory
        juce::MemoryBlock wavBlock;
        {
            // 1. Create a unique_ptr to an OutputStream (the new API requires this specific base type)
            std::unique_ptr<juce::OutputStream> mos (new juce::MemoryOutputStream (wavBlock, false));
        
            std::unique_ptr<juce::AudioFormat> wavFormat (new juce::WavAudioFormat());
        
            // 2. Bundle the arguments using the builder pattern
            auto options = juce::AudioFormatWriterOptions()
                               .withSampleRate (44100.0)
                               .withNumChannels (static_cast<int> (buf->getNumChannels()))
                               .withBitsPerSample (16);
        
            // 3. Pass the stream reference and the options struct
            std::unique_ptr<juce::AudioFormatWriter> writer = wavFormat->createWriterFor (mos, options);
        
            if (writer != nullptr)
            {
                writer->writeFromAudioSampleBuffer (*buf, 0, buf->getNumSamples());
            }
        
            // No manual delete needed! If createWriterFor succeeds, it adopts 'mos' (setting it to nullptr).
            // If it fails, 'mos' simply goes out of scope and deletes itself safely.
        }

        // Store as base64 under an <Op> element
        auto* opNode = samplesNode->createNewChildElement ("Op");
        opNode->setAttribute ("index",    opIdx);
        opNode->setAttribute ("filename", loadedSampleNames[opIdx]);
        opNode->setAttribute ("data",     wavBlock.toBase64Encoding());
    }

    copyXmlToBinary (*xml, destData);
}

void FMPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState == nullptr || ! xmlState->hasTagName (apvts.state.getType()))
        return;

    // Restore any embedded samples before replacing state so voices are ready immediately
    if (auto* samplesNode = xmlState->getChildByName ("SampleData"))
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        for (auto* opNode : samplesNode->getChildIterator())
        {
            int opIdx = opNode->getIntAttribute ("index", -1);
            if (opIdx < 0 || opIdx >= ProjectConfig::numOperators)
                continue;

            juce::MemoryBlock wavBlock;
            wavBlock.fromBase64Encoding (opNode->getStringAttribute ("data"));
            if (wavBlock.isEmpty())
                continue;

            juce::MemoryInputStream mis (wavBlock.getData(), wavBlock.getSize(), false);
            std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (
                std::make_unique<juce::MemoryInputStream> (wavBlock.getData(), wavBlock.getSize(), false)));

            if (reader == nullptr)
                continue;

            auto newBuffer = std::make_shared<juce::AudioBuffer<float>> (
                static_cast<int> (reader->numChannels),
                static_cast<int> (reader->lengthInSamples));
            reader->read (newBuffer.get(), 0, newBuffer->getNumSamples(), 0, true, true);

            float peak = newBuffer->getMagnitude (0, newBuffer->getNumSamples());
            if (peak > 0.0001f)
                newBuffer->applyGain (1.0f / peak);

            loadedSamples[opIdx]     = newBuffer;
            loadedSampleNames[opIdx] = opNode->getStringAttribute ("filename", "Sample");
            for (int i = 0; i < synth.getNumVoices(); ++i)
                if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
                    voice->setSampleData (opIdx, newBuffer);
        }
    }

    // Remove the SampleData node safely before restoring APVTS state
    if (auto* samplesNode = xmlState->getChildByName ("SampleData"))
    {
        xmlState->removeChildElement (samplesNode, true);
    }
    
    apvts.replaceState (juce::ValueTree::fromXml (*xmlState));

    // Notify the editor so it can update Load button labels
    juce::MessageManager::callAsync ([this] {
        if (onSamplesRestored)
            onSamplesRestored();
    });
}

void FMPluginAudioProcessor::reset() // This function solves issues if you're looping a bar in a VST.
{
    // Tell the synth engine to kill hanging voice notes
    synth.allNotesOff (0, false);
    // Also clear effects, which aren't cleared by the above.
    for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
            {
                voice->resetVoiceState();
            }
        }
    // and clear effects
    for (int i = 0; i < ProjectConfig::numEffects; ++i)
    {
	for (int ch = 0; ch < 2; ++ch)
	{
        	fxEffects[i][ch].reset();
	}
    }
}

// Necessary to avoid createPluginFilter() error and actually create the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FMPluginAudioProcessor();
}
