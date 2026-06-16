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

// FX LFO — global, voice-independent modulator for the effects chain.
// Caches APVTS raw parameter pointers at prepare() time for lock-free audio-thread reads.
// tick() advances phase by one sample and returns output scaled by depth — call per sample.
struct FXModLFO
{
    // Call once in prepareToPlay — caches all param pointers and sample rate
    void prepare (double sampleRate, juce::AudioProcessorValueTreeState& apvts, int lfoIndex)
    {
        currentSampleRate = sampleRate;
        juce::String s = juce::String (lfoIndex + 1);
        rateParam  = apvts.getRawParameterValue ("FX_LFO_RATE_"  + s);
        rateSyncParam = apvts.getRawParameterValue ("FX_LFO_RATE_SYNC_" + s);
        depthParam = apvts.getRawParameterValue ("FX_LFO_DEPTH_" + s);
        waveParam  = apvts.getRawParameterValue ("FX_LFO_WAVE_"  + s);
        syncParam  = apvts.getRawParameterValue ("FX_LFO_SYNC_"  + s);
        tgtParam   = apvts.getRawParameterValue ("FX_LFO_TGT_"   + s);
        jassert (rateParam != nullptr && depthParam != nullptr &&
                 waveParam != nullptr && syncParam  != nullptr && tgtParam != nullptr);
    }

    // Advance phase by one sample and return LFO output scaled by depth.
    // Call once per sample inside the effects loop.
    float tick (float activeBPM)
    {
        float rate   = rateParam  != nullptr ? rateParam->load  (std::memory_order_relaxed) : 1.0f;
        float depth  = depthParam != nullptr ? depthParam->load (std::memory_order_relaxed) : 1.0f;
        bool  synced = syncParam  != nullptr && syncParam->load (std::memory_order_relaxed) > 0.5f;
        int   wave   = waveParam  != nullptr ? static_cast<int> (waveParam->load (std::memory_order_relaxed)) : 0;

        if (synced && activeBPM > 0.0f)
        {
            // Simply grab the index (0-8) from the choice parameter
            int subIdx = static_cast<int> (rateSyncParam->load (std::memory_order_relaxed));
            
            // Use an array that matches your Constants.h syncRates order
            const float multipliers[] = { 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f };
            subIdx = juce::jlimit (0, 8, subIdx);

            // Calculate rate based on BPM
            rate = (activeBPM / 60.0f) * multipliers[subIdx];
        }
        else
        {
            rate = rateParam->load (std::memory_order_relaxed);
        }

        // Advance phase by one sample (negative rate runs LFO in reverse)
        phase += (juce::MathConstants<double>::twoPi * (double)rate) / currentSampleRate;
        while (phase >= juce::MathConstants<double>::twoPi)
            phase -= juce::MathConstants<double>::twoPi;
        while (phase < 0.0)
            phase += juce::MathConstants<double>::twoPi;

        float raw    = 0.0f;
        float fPhase = static_cast<float> (phase);
        float twoPi  = static_cast<float> (juce::MathConstants<double>::twoPi);
        switch (wave)
        {
            case 0: // Sine
                raw = std::sin (fPhase);
                break;
            case 1: // Triangle
                raw = (2.0f / twoPi) * (fPhase < twoPi * 0.5f
                      ? fPhase
                      : twoPi - fPhase) * 2.0f - 1.0f;
                break;
            case 2: // Saw (descending)
                raw = 1.0f - (2.0f * fPhase / twoPi);
                break;
            case 3: // Square
                raw = (fPhase < static_cast<float> (juce::MathConstants<double>::pi)) ? 1.0f : -1.0f;
                break;
            default:
                raw = std::sin (fPhase);
                break;
        }

        return raw * depth;
    }

    // Returns encoded target: -1 for None, otherwise (fxSlot * 5 + fxParam)
    // fxSlot = result/5, fxParam = result%5  (0=Ratio, 1=Detune, 2=Phase, 3=Fold, 4=Level)
    int getTarget() const
    {
        if (tgtParam == nullptr) return -1;
        int tgtIdx = static_cast<int> (tgtParam->load (std::memory_order_relaxed));
        int fxBase = tgtIdx - 1; // index 0 = None, 1 = "FX 1 Ratio", etc.
        if (fxBase < 0 || fxBase >= 15) return -1;
        return fxBase;
    }

    bool isPrepared() const { return rateParam != nullptr; }

private:
    double phase = 0.0;
    double currentSampleRate = 44100.0;
    std::atomic<float>* rateParam  = nullptr;
    std::atomic<float>* rateSyncParam = nullptr;
    std::atomic<float>* depthParam = nullptr;
    std::atomic<float>* waveParam  = nullptr;
    std::atomic<float>* syncParam  = nullptr;
    std::atomic<float>* tgtParam   = nullptr;
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
    juce::AudioBuffer<float> inputCapture;
    juce::Synthesiser synth;
    WaveTable waveTable; //single shared instance for all operators
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateVoices();
    //effects
    static constexpr int numFxSlots = 3;
    std::array<std::array<SynthEffect, 2>, numFxSlots> fxEffects;
    // Parameter pointers for each effects slot
    std::atomic<float>* fxTypeParams[numFxSlots]   { nullptr };
    std::atomic<float>* fxMixParams[numFxSlots]    { nullptr };
    std::atomic<float>* fxRatioParams[numFxSlots]  { nullptr };
    std::atomic<float>* fxDetuneParams[numFxSlots] { nullptr };
    std::atomic<float>* fxPhaseParams[numFxSlots]  { nullptr };
    std::atomic<float>* fxFoldParams[numFxSlots]   { nullptr };
    // FX LFOs — three global, voice-independent modulators
    FXModLFO fxLfo[3];
    // One shared sample buffer per operator slot (shared across all voices, set once)
    std::array<std::shared_ptr<juce::AudioBuffer<float>>, ProjectConfig::numOperators> loadedSamples;
    // also for everything we need to flush on every run to not echo forever
    std::array<int, numFxSlots> lastFxEffectType { 0, 0, 0 };
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    int currentOversamplingFactor = 1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FMPluginAudioProcessor)
};
