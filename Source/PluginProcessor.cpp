// PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Constants.h"

FMPluginAudioProcessor::FMPluginAudioProcessor()
    : AudioProcessor (BusesProperties()
		    .withInput  ("Input",  juce::AudioChannelSet::stereo(), false) // optional input!
		    .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // This is the number of voices the synth has (8 voice polyphony).
    for (int i = 0; i < 8; ++i)
    {
        auto* voice = new FMVoice();   // 1. Instantiate the voice
        voice->initParameters (apvts); // 2. Initialize parameters while it's in scope
        synth.addVoice (voice);        // 3. Hand ownership over to the JUCE synth
    }

    synth.addSound (new FMSound());
    for (int i = 0; i < numFxSlots; ++i)
    {
        juce::String s = juce::String (i + 1);
        fxTypeParams[i]   = apvts.getRawParameterValue ("FX_TYPE_"   + s);
        fxSyncParams[i]   = apvts.getRawParameterValue ("FX_SYNC_"   + s);
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
    // Output must always be stereo
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Input can be stereo or disabled (no input)
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo() &&
        !layouts.getMainInputChannelSet().isDisabled())
        return false;

    return true;
}

juce::AudioProcessorValueTreeState::ParameterLayout FMPluginAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    juce::StringArray modeChoices { "Wave", "Additive", "Filter", "Ext. In" };
    juce::StringArray waveShapeChoices { "Sine", "Triangle", "Saw", "Square", "Pulse", "SquarePWM", "White Noise", "Pink Noise" };
    juce::StringArray freqModeChoices { "Standard", "Sync", "Hz", "LFO" };
    auto filterTypeChoices = ProjectConfig::getFilterTypeChoices();

    // Generate parameters for each operator dynamically
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        // ALL KNOB SETTINGS ARE HERE
        juce::String opNum = juce::String (i + 1);
        // Wave
        params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "MODE_" + opNum, 1 }, "Op " + opNum + " Mode", modeChoices, 0));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "WAVE_SHAPE_" + opNum, 1 }, "Op " + opNum + " Wave Shape", waveShapeChoices, 0));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "FILTER_TYPE_" + opNum, 1 }, "Op " + opNum + " Filter Type", filterTypeChoices, 0));
        // Tempo Sync/hz lock/LFO mode
        params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "FREQ_MODE_" + opNum, 1 }, "Op " + opNum + " Freq Mode", freqModeChoices, 0));
	// Osc Settings - Ratios, Detune, Phase, Fold. We repurpose these for filter
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"RATIO_" + opNum, 1}, "Op " + opNum + " Ratio", 0.01f, 16.0f, 1.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"DETUNE_" + opNum, 1}, "Op " + opNum + " Detune", -50.0f, 50.0f, 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "PHASE_" + opNum, 1 }, "Op " + opNum + " Phase", 0.0f, 360.0f, 0.0f));
	params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "FOLD_" + opNum, 1 }, "Op " + opNum + " Phase", 0.0f, 1.0f, 0.0f));
        // Envelopes
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"ATTACK_" + opNum, 1}, "Op " + opNum + " Attack", 0.001f, 5.0f, 0.1f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"DECAY_" + opNum, 1}, "Op " + opNum + " Decay", 0.01f, 5.0f, 0.2f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"SUSTAIN_" + opNum, 1}, "Op " + opNum + " Sustain", 0.0f, 1.0f, 0.8f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"RELEASE_" + opNum, 1}, "Op " + opNum + " Release", 0.01f, 5.0f, 0.5f));
        // Output Levels
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"OUT_" + opNum, 1}, "Op " + opNum + " Out Level", 0.0f, 1.0f, (i == 0) ? 1.0f : 0.0f));
    }
    // Effects page — 3 slots, each with type, sync, mix, and 4 knobs
    const int numFxSlots = 3;
    for (int i = 0; i < numFxSlots; ++i)
    {
        juce::String s = juce::String (i + 1);
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { "FX_TYPE_" + s, 1 }, "FX " + s + " Type", filterTypeChoices, 0));
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { "FX_SYNC_" + s, 1 }, "FX " + s + " Sync", false));
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
    // Sources: None + 6 Operators = 7 choices (index 0 = None, 1-6 = Op 1-6)
    // Targets: None + 5 params x 6 ops + 8 effects = 39 choices
    juce::StringArray modSourceChoices {
        "None",
        "Op 1", "Op 2", "Op 3", "Op 4", "Op 5", "Op 6"
    };
    juce::StringArray modTargetChoices {
        "None",
        // Per-operator targets (5 params x 6 ops = 30)
        "Op 1 Ratio",  "Op 1 Detune", "Op 1 Phase", "Op 1 Fold", "Op 1 Level",
        "Op 2 Ratio",  "Op 2 Detune", "Op 2 Phase", "Op 2 Fold", "Op 2 Level",
        "Op 3 Ratio",  "Op 3 Detune", "Op 3 Phase", "Op 3 Fold", "Op 3 Level",
        "Op 4 Ratio",  "Op 4 Detune", "Op 4 Phase", "Op 4 Fold", "Op 4 Level",
        "Op 5 Ratio",  "Op 5 Detune", "Op 5 Phase", "Op 5 Fold", "Op 5 Level",
        "Op 6 Ratio",  "Op 6 Detune", "Op 6 Phase", "Op 6 Fold", "Op 6 Level",
        // Effects targets (15)
        "Fx 1 Ratio",  "Fx 1 Detune", "Fx 1 Phase", "Fx 1 Fold", "Fx 1 Level",
        "Fx 2 Ratio",  "Fx 2 Detune", "Fx 2 Phase", "Fx 2 Fold", "Fx 2 Level",
        "Fx 3 Ratio",  "Fx 3 Detune", "Fx 3 Phase", "Fx 3 Fold", "Fx 3 Level",
    };
    
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

    // Tell all voices about the new oversampling factor so phase increments stay correct
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
        {
            voice->setOversamplingFactor (factor);
            voice->prepare (getSampleRate(), getBlockSize(), &waveTable);
        }
}

void FMPluginAudioProcessor::setPolyphony (int numVoices)
{
    // Remove all existing voices
    synth.clearVoices();

    // Add the new number of voices
    for (int i = 0; i < numVoices; ++i)
    {
        auto* voice = new FMVoice();
        voice->initParameters (apvts);
        synth.addVoice (voice);
    }

    // Prepare all the new voices
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
        {
            voice->setCurrentPlaybackSampleRate (getSampleRate());
            voice->prepare (getSampleRate(), getBlockSize(), &waveTable);
            voice->setOversamplingFactor (currentOversamplingFactor);
        }
    }
}

void FMPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    waveTable.prepare(); // Build tables before use
    // prepare synth voices
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
        {
            voice->setCurrentPlaybackSampleRate (sampleRate);
            voice->prepare (sampleRate, samplesPerBlock, &waveTable);
        }
    }
    setOversamplingFactor(1); //prime this setting
    for (int i = 0; i < numFxSlots; ++i) // prep effects lanes
    {
        fxFilters[i].prepare (sampleRate);
        fxFilters[i].reset();
    }
}

void FMPluginAudioProcessor::releaseResources() {}

void FMPluginAudioProcessor::updateVoices()
{
    // Read current atomic parameters from APVTS and safely pass them to our synthesis engine
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
        {
            // In a production build, add a thread-safe method inside `FMVoice` 
            // (e.g., voice->updateParameters(...)) to read these parameters atomically.
        }
    }
}

void FMPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    // Tempo Sync
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
    // clear unused channels, render synth
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    // Capture input audio before clearing so external audio operators can use it
    juce::AudioBuffer<float> inputCapture (totalNumInputChannels, buffer.getNumSamples());
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
        inputCapture.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());
    
    // Clear entire buffer so input audio doesn't bleed through to output
    buffer.clear();
    DBG ("after clear - inputCh: " + juce::String (totalNumInputChannels) +
     " outputCh: " + juce::String (totalNumOutputChannels) +
     " sample0: " + juce::String (buffer.getSample (0, 0)));
    
    auto* inputL = totalNumInputChannels > 0 ? inputCapture.getReadPointer (0) : nullptr;
    auto* inputR = totalNumInputChannels > 1 ? inputCapture.getReadPointer (1) : nullptr;

    updateVoices();
    // Render synth with optional oversampling and external audio possibly being passed in  
    if (oversampling != nullptr && currentOversamplingFactor > 1)
    {
        juce::dsp::AudioBlock<float> inputBlock (buffer);
        auto oversampledBlock = oversampling->processSamplesUp (inputBlock);
    
        int oversampledSize = static_cast<int> (oversampledBlock.getNumSamples());
        juce::AudioBuffer<float> oversampledBuffer (totalNumOutputChannels, oversampledSize);
        oversampledBuffer.clear();
    
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
    
        // Feed external audio per sample into voices before render
        for (int i = 0; i < oversampledSize; ++i)
        {
            float extL = (inputL != nullptr && i < buffer.getNumSamples()) ? inputL[i] : 0.0f;
            float extR = (inputR != nullptr && i < buffer.getNumSamples()) ? inputR[i] : 0.0f;
            for (int v = 0; v < synth.getNumVoices(); ++v)
                if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (v)))
                    voice->setExternalAudioSample (extL, extR);
        }
    
        synth.renderNextBlock (oversampledWrap, scaledMidi, 0, oversampledSize);
        oversampling->processSamplesDown (inputBlock);
    }
    else
    {
        // Feed external audio per sample into voices before render
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float extL = inputL != nullptr ? inputL[i] : 0.0f;
            float extR = inputR != nullptr ? inputR[i] : 0.0f;
            for (int v = 0; v < synth.getNumVoices(); ++v)
                if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (v)))
                    voice->setExternalAudioSample (extL, extR);
        }
        synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
    }

    // EFFECTS PAGE — series processing 1 → 2 → 3
    // First we get any modulation
    std::array<float, 3> fxRatioModSum  { 0.0f, 0.0f, 0.0f };
    std::array<float, 3> fxDetuneModSum { 0.0f, 0.0f, 0.0f };
    std::array<float, 3> fxPhaseModSum  { 0.0f, 0.0f, 0.0f };
    std::array<float, 3> fxFoldModSum   { 0.0f, 0.0f, 0.0f };
    std::array<float, 3> fxLevelModSum  { 0.0f, 0.0f, 0.0f };
    
    for (int v = 0; v < synth.getNumVoices(); ++v)
    {
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (v)))
        {
            if (voice->isVoiceActive())
            {
                for (int i = 0; i < 3; ++i)
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
    // Now we start the effects loop
    {
        auto* leftData  = buffer.getWritePointer (0);
        auto* rightData = buffer.getWritePointer (1);
        int   numSamples = buffer.getNumSamples();
    
        for (int slot = 0; slot < numFxSlots; ++slot)
        {
            int   filterType = static_cast<int> (fxTypeParams[slot]->load  (std::memory_order_relaxed));
            float ratio  = fxRatioParams[slot]->load  (std::memory_order_relaxed) + fxRatioModSum[slot];
            float detune = fxDetuneParams[slot]->load (std::memory_order_relaxed) + fxDetuneModSum[slot];
            float phase  = fxPhaseParams[slot]->load  (std::memory_order_relaxed) + fxPhaseModSum[slot];
            float fold   = fxFoldParams[slot]->load   (std::memory_order_relaxed) + fxFoldModSum[slot];
            float mix    = juce::jlimit (0.0f, 1.0f,
                           fxMixParams[slot]->load (std::memory_order_relaxed) + fxLevelModSum[slot]);
	    bool  isSynced   = fxSyncParams[slot]->load   (std::memory_order_relaxed) > 0.5f;

	    if (filterType != lastFxFilterType[slot])
            {
                fxFilters[slot].reset();
                lastFxFilterType[slot] = filterType;
            }

            if (filterType == 0) continue; // None — skip entirely

            // Compute cutoff from ratio knob (same mapping as operators)
            float normalizedRatio = (ratio - 0.01f) / (16.0f - 0.01f);
            float baseCutoff      = 20.0f * std::pow (1000.0f, normalizedRatio);

            // Tempo sync — snap cutoff to rhythmic subdivision
            if (isSynced && activeBPM > 0.0f)
            {
                // Map phase knob to subdivision (0=1/16, 0.25=1/8, 0.5=1/4, 0.75=1/2, 1.0=1/1)
                float normalizedPhase = phase / 360.0f;
                float subdivisions[]  = { 16.0f, 8.0f, 4.0f, 2.0f, 1.0f };
                int   subIdx          = juce::jlimit (0, 4, static_cast<int> (normalizedPhase * 5.0f));
                float beatsPerCycle   = 1.0f / subdivisions[subIdx];
                baseCutoff            = (activeBPM / 60.0f) / beatsPerCycle;
                baseCutoff            = juce::jlimit (20.0f, static_cast<float> (getSampleRate()) * 0.49f, baseCutoff);
            }
    
            float dampingAmt      = juce::jlimit (0.001f, 0.95f, (detune + 50.0f) / 100.0f);
            float feedbackAmt     = juce::jlimit (-0.95f, 0.95f, (fold * 2.0f) - 1.0f);
            float coupledRes      = dampingAmt * dampingAmt;

            for (int i = 0; i < numSamples; ++i) // process every effect ...
            {
                // Mix L+R to mono for processing, then spread back to stereo
                float inputSample = (leftData[i] + rightData[i]) * 0.5f;
                float processed   = 0.0f;
                switch (filterType)
                {
                    case 1: // Lowpass
                    case 2: // Highpass
                    case 3: // Bandpass
                    {
                        fxFilters[slot].setResonance (coupledRes);
                        float k = fxFilters[slot].getPrecalculatedK();
                        processed = fxFilters[slot].processSampleAudioRate (inputSample, baseCutoff, k);
                        break;
                    }
                    case 4: // Comb
                    {
                        processed = fxFilters[slot].processSampleComb (inputSample, baseCutoff, feedbackAmt, dampingAmt);
                        processed = std::isfinite (processed) ? std::tanh (processed) : 0.0f;
                        break;
                    }
                    case 5: // Granular
                    {
                        float grainDurationMs = juce::jmap (normalizedRatio, 0.0f, 1.0f, 10.0f, 1000.0f);
                        float scatterAmt      = juce::jlimit (0.0f, 1.0f, phase / 360.0f);
                        processed = fxFilters[slot].processSampleGranular (inputSample, baseCutoff, scatterAmt, grainDurationMs, feedbackAmt, dampingAmt);
                        processed = std::isfinite (processed) ? std::tanh (processed) : 0.0f;
                        break;
                    }
                    case 6: // Formant
                    {
                        float normalizedRatio  = (ratio - 0.01f) / (16.0f - 0.01f);
                        float baseVowel        = normalizedRatio * 4.0f;
                        float modDepth         = phase / 360.0f;
                        float dynamicVowel     = juce::jlimit (0.0f, 4.0f,
                                                     baseVowel + (fxPhaseModSum[slot] * modDepth * 4.0f));
                        float normalizedDetune = (detune + 50.0f) / 100.0f;
                        float qFactor          = juce::jmap (normalizedDetune, 0.0f, 1.0f, 2.0f, 15.0f);
                        float drive            = 1.0f + (fold * 4.0f);
                        float drivenInput      = std::tanh (inputSample * drive);
                        float output           = fxFilters[slot].processSampleFormant (drivenInput, dynamicVowel, qFactor);
                        processed              = std::isfinite (output) ? std::tanh (output) : 0.0f;
                        break;
                    }
                    case 7: // Tape
                    {
                        float wobbleRate = normalizedRatio;
                        float age        = dampingAmt;
                        float saturation = phase / 360.0f;
                        float bias       = juce::jlimit (0.0f, 1.0f, fold);
                        processed = fxFilters[slot].processSampleTape (inputSample, wobbleRate, age,
                                                                        saturation, bias, getSampleRate());
                        processed = std::isfinite (processed) ? processed : 0.0f;
                        break;
                    }
                    case 8: // Bitcrush
                    {
                        float bits   = normalizedRatio;
                        float rate   = dampingAmt;
                        float jitter = phase / 360.0f;
                        float noise  = juce::jlimit (0.0f, 1.0f, fold);
                        processed = fxFilters[slot].processSampleBitcrush (inputSample, bits, rate,
                                                                           jitter, noise, getSampleRate());
                        processed = std::isfinite (processed) ? processed : 0.0f;
                        break;
                    }
                    case 9: // Allpass Delay
                    {
                        float time      = normalizedRatio;
                        float feedback  = dampingAmt;
                        float diffusion = phase / 360.0f;
                        float damping   = juce::jlimit (0.0f, 1.0f, fold);
                        processed = fxFilters[slot].processSampleAllpassDelay (inputSample, time, feedback,
                                                                               diffusion, damping, getSampleRate());
                        processed = std::isfinite (processed) ? std::tanh (processed) : 0.0f;
                        break;
                    }
                    case 10: // Allpass Reverb
                    {
                        float size      = normalizedRatio;
                        float decay     = dampingAmt;
                        float diffusion = phase / 360.0f;
                        float damping   = juce::jlimit (0.0f, 1.0f, fold);
                        processed = fxFilters[slot].processSampleAllpassReverb (inputSample, size, decay,
                                                                                 diffusion, damping, getSampleRate());
                        processed = std::isfinite (processed) ? std::tanh (processed) : 0.0f;
                        break;
                    }
                    case 11: // Compressor
                    {
                        float threshold = normalizedRatio;
                        float compRatio = dampingAmt;
                        float attack    = phase / 360.0f;
                        float release   = juce::jlimit (0.0f, 1.0f, fold);
                        processed = fxFilters[slot].processSampleCompressor (inputSample, threshold,
                                                                              compRatio, attack, release,
                                                                              getSampleRate());
                        processed = std::isfinite (processed) ? processed : 0.0f;
                        break;
                    }
                    case 12: // Varispeed
                    {
                        float speed        = normalizedRatio;
                        float acceleration = dampingAmt;
                        float depth        = phase / 360.0f;
                        float mode         = juce::jlimit (0.0f, 1.0f, fold);
                        processed = fxFilters[slot].processSampleVarispeed (inputSample, speed, acceleration,
                                                                             depth, mode, getSampleRate());
                        processed = std::isfinite (processed) ? processed : 0.0f;
                        break;
                    }
                    case 13: // Scatter
                    {
                        float pattern = normalizedRatio;
                        float size    = dampingAmt;
                        float speed   = phase / 360.0f;
                        float depth   = juce::jlimit (0.0f, 1.0f, fold);
                        processed = fxFilters[slot].processSampleScatter (inputSample, pattern, size,
                                                                           speed, depth, getSampleRate());
                        processed = std::isfinite (processed) ? processed : 0.0f;
                        break;
                    }
                    case 14: // Ring Modulator
                    {
                        float frequency = 0.1f * std::pow (50000.0f, normalizedRatio);
                        float shape     = dampingAmt;
                        float depth     = phase / 360.0f;
                        float feedback  = juce::jlimit (0.0f, 0.95f, fold);
                        processed = fxFilters[slot].processSampleRingMod (inputSample, frequency, shape,
                                                                           depth, feedback, getSampleRate());
                        processed = std::isfinite (processed) ? processed : 0.0f;
                        break;
                    }
                    case 15: // Chorus
                    {
                        float rate   = normalizedRatio;
                        float depth  = dampingAmt;
                        float spread = phase / 360.0f;
                        float voices = juce::jlimit (0.0f, 1.0f, fold);
                        processed = fxFilters[slot].processSampleChorus (inputSample, rate, depth,
                                                                          spread, voices, getSampleRate());
                        processed = std::isfinite (processed) ? processed : 0.0f;
                        break;
                    }
                    case 16: // Distortion
                    {
                        float drive       = normalizedRatio;
                        float flavor      = dampingAmt;
                        float toneKnob    = phase / 360.0f;
                        float degradation = juce::jlimit (0.0f, 1.0f, fold);
                        processed = fxFilters[slot].processSampleDistortion (inputSample, drive, flavor,
                                                                              toneKnob, degradation,
                                                                              getSampleRate());
                        processed = std::isfinite (processed) ? processed : 0.0f;
                        break;
                    }
                    case 17: // DJFX Delay
                    {
                        float bufferAmt = normalizedRatio;
                        float speed     = dampingAmt;
                        float on        = phase / 360.0f;
                        float drift     = juce::jlimit (0.0f, 1.0f, fold);
                        processed = fxFilters[slot].processSampleDJFXDelay (inputSample, bufferAmt, speed,
                                                                             on, drift, getSampleRate());
                        processed = std::isfinite (processed) ? processed : 0.0f;
                        break;
                    }
                    case 18: // Harmonic Resonator
                    {
                        float root       = normalizedRatio;
                        float scaleKnob  = dampingAmt;
                        float brightness = phase / 360.0f;
                        float resonDepth = juce::jlimit (0.0f, 1.0f, fold);
                        processed = fxFilters[slot].processSampleHarmonicResonator (inputSample, root,
                                                                                      scaleKnob, brightness,
                                                                                      resonDepth, getSampleRate());
                        processed = std::isfinite (processed) ? processed : 0.0f;
                        break;
                    }
                    case 19: // Ambient Delay
                    {
                        float time       = normalizedRatio;
                        float feedbk     = dampingAmt;
                        float shimmerAmt = phase / 360.0f;
                        float diffusion  = juce::jlimit (0.0f, 1.0f, fold);
                        processed = fxFilters[slot].processSampleAmbientDelay (inputSample, time, feedbk,
                                                                                shimmerAmt, diffusion,
                                                                                getSampleRate());
                        processed = std::isfinite (processed) ? processed : 0.0f;
                        break;
                    }
                    case 20: // Old Chorus
                    {
                        float rate     = normalizedRatio;
                        float depth    = dampingAmt;
                        float modeKnob = phase / 360.0f;
                        float warmth   = juce::jlimit (0.0f, 1.0f, fold);
                        processed = fxFilters[slot].processSampleOldChorus (inputSample, rate, depth,
                                                                             modeKnob, warmth,
                                                                             getSampleRate());
                        processed = std::isfinite (processed) ? processed : 0.0f;
                        break;
                    }
                    case 21: // OTT
                    {
                        float depth    = normalizedRatio;
                        float timeKnob = dampingAmt;
                        float upward   = phase / 360.0f;
                        float tone     = juce::jlimit (0.0f, 1.0f, fold);
                        processed = fxFilters[slot].processSampleOTT (inputSample, depth, timeKnob,
                                                                       upward, tone, getSampleRate());
                        processed = std::isfinite (processed) ? processed : 0.0f;
                        break;
                    }
                    case 22: // Spectral Freeze
                    {
                        float freeze = normalizedRatio;
                        float blend  = dampingAmt;
                        float pitch  = phase / 360.0f;
                        float blur   = juce::jlimit (0.0f, 1.0f, fold);
                        processed = fxFilters[slot].processSampleSpectralFreeze (inputSample, freeze,
                                                                                  blend, pitch, blur,
                                                                                  getSampleRate());
                        processed = std::isfinite (processed) ? processed : 0.0f;
                        break;
                    }
                    default:
                        processed = inputSample;
                        break;
                }
    
                // Wet/dry mix and write back to stereo
                float mixed      = processed * mix + inputSample * (1.0f - mix);
                leftData[i]      = mixed;
                rightData[i]     = mixed;
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

juce::AudioProcessorEditor* FMPluginAudioProcessor::createEditor()
{
    return new FMPluginAudioProcessorEditor (*this);
}

void FMPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void FMPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

void FMPluginAudioProcessor::reset() // This function solves issues if you're looping a bar in a VST.
{
    // Tell the synth engine to kill hanging voice notes
    synth.allNotesOff (0, false);
    // Also clear filters, which aren't cleared by the above.
    for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
            {
                voice->resetVoiceState();
            }
        }
    // and clear effect filters
    for (int i = 0; i < numFxSlots; ++i)
    {
        fxFilters[i].reset();
    }
}

// Necessary to avoid createPluginFilter() error and actually create the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FMPluginAudioProcessor();
}
