// PluginProcessor.h
#pragma once
#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include "FMVoice.h"
#include "WaveTable.h"
#include "SynthEffect.h"

// Simple dummy sound struct needed by juce::Synthesiser
struct FMSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class FMPluginAudioProcessor  : public juce::AudioProcessor
{
public:
    FMPluginAudioProcessor();
    ~FMPluginAudioProcessor() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void setOversamplingFactor (int factor);
    void setPolyphony (int numVoices);
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    juce::AudioProcessorValueTreeState apvts;
    // Load a WAV/AIFF/etc. file and distribute it to the given operator index on all voices.
    void loadSampleForOperator (int opIndex, const juce::File& file);
    // Called on the message thread after setStateInformation restores any sample data.
    // The editor uses this to update Load button text.
    std::function<void()> onSamplesRestored;
    // Filename of the loaded sample for each operator (empty if none). Used to update UI after preset load.
    std::array<juce::String, ProjectConfig::numOperators> loadedSampleNames;
    static constexpr int scopeBufferSize = 512;
    std::array<float, scopeBufferSize> scopeBuffer { 0.0f };
    std::atomic<int> scopeWritePos { 0 };
private:
    juce::Synthesiser synth;
    WaveTable waveTable; //single shared instance for all operators
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateVoices();
    //effects
    static constexpr int numFxSlots = 3;
    std::array<SynthEffect, numFxSlots> fxEffects; 
    // Parameter pointers for each effects slot
    std::atomic<float>* fxTypeParams[numFxSlots]   { nullptr };
    std::atomic<float>* fxSyncParams[numFxSlots]   { nullptr };
    std::atomic<float>* fxMixParams[numFxSlots]    { nullptr };
    std::atomic<float>* fxRatioParams[numFxSlots]  { nullptr };
    std::atomic<float>* fxDetuneParams[numFxSlots] { nullptr };
    std::atomic<float>* fxPhaseParams[numFxSlots]  { nullptr };
    std::atomic<float>* fxFoldParams[numFxSlots]   { nullptr };
    // One shared sample buffer per operator slot (shared across all voices, set once)
    std::array<std::shared_ptr<juce::AudioBuffer<float>>, ProjectConfig::numOperators> loadedSamples;
    // also for everything we need to flush on every run to not echo forever
    std::array<int, numFxSlots> lastFxEffectType { 0, 0, 0 };
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    int currentOversamplingFactor = 1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FMPluginAudioProcessor)
};
