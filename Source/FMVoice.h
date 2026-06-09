//FMVoice.h
#pragma once
#include <JuceHeader.h>
#include "FMOperator.h"
#include "SynthFilter.h"
#include "Constants.h"
#include "WaveTable.h"
#include <array>
#include <memory>

struct OperatorParameterCache 
{
    std::atomic<float>* mode { nullptr };
    std::atomic<float>* wave { nullptr };
    std::atomic<float>* filterType { nullptr };
    std::atomic<float>* ratio { nullptr };
    std::atomic<float>* detune { nullptr };
    std::atomic<float>* phase { nullptr };
    std::atomic<float>* fold { nullptr };
    std::atomic<float>* out { nullptr };
    std::atomic<float>* attack { nullptr };
    std::atomic<float>* decay { nullptr };
    std::atomic<float>* sustain { nullptr };
    std::atomic<float>* release { nullptr };
    std::atomic<float>* freqMode { nullptr };
};

class FMVoice : public juce::SynthesiserVoice
{
public:
    FMVoice();
    ~FMVoice() override = default;
    bool canPlaySound (juce::SynthesiserSound* sound) override;
    void initParameters (juce::AudioProcessorValueTreeState& apvts);
    void setCurrentPlaybackSampleRate (double newRate) override;
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int controllerNumber, int newControllerValue) override;
    void setOversamplingFactor (int factor);
    void prepare (double sampleRate, int samplesPerBlock, WaveTable* wt);
    void setAlwaysActive (bool shouldBeActive) noexcept { alwaysActive = shouldBeActive; }
    void setDAWTempo (float newBPM) noexcept;
    void resetVoiceState();
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;
    void setExternalAudioSample (float left, float right) noexcept
    {
        externalAudioL = left;
        externalAudioR = right;
    }
    bool isVoiceActive() const override
    {
        return alwaysActive || juce::SynthesiserVoice::isVoiceActive();
    }
    //effects
    std::atomic<float> fxRatioMods[3]  { 0.0f, 0.0f, 0.0f };
    std::atomic<float> fxDetuneMods[3] { 0.0f, 0.0f, 0.0f };
    std::atomic<float> fxPhaseMods[3]  { 0.0f, 0.0f, 0.0f };
    std::atomic<float> fxFoldMods[3]   { 0.0f, 0.0f, 0.0f };
    std::atomic<float> fxLevelMods[3]  { 0.0f, 0.0f, 0.0f };
    
private:
    float externalAudioL = 0.0f;
    float externalAudioR = 0.0f;
    double baseFrequency { 440.0 };
    bool alwaysActive = false;
    float level { 0.0f };
    int lastPlayedNote = 60; 
    std::atomic<float> currentBPM { 120.0f };
    WaveTable* waveTable = nullptr;
    std::array<FMOperator, ProjectConfig::numOperators> operators;
    std::array<float, ProjectConfig::numOperators> lastOpOutputs { 0.0f };
    std::array<float, ProjectConfig::numOperators> previousOpOutputs { 0.0f };
    std::array<float, ProjectConfig::numOperators> processedOpOutputs { 0.0f };
    std::array<OperatorParameterCache, ProjectConfig::numOperators> opParams;
    std::array<std::atomic<float>*, 6> extraModParams { nullptr };
    std::atomic<float>* modSlotSrc[ProjectConfig::numModSlots] { nullptr };
    std::atomic<float>* modSlotTgt[ProjectConfig::numModSlots] { nullptr };
    std::atomic<float>* modSlotAmt[ProjectConfig::numModSlots] { nullptr };
    // FM and Audio routing grids
    std::atomic<float>* matrixParams[ProjectConfig::numOperators][ProjectConfig::numOperators]      { nullptr };
    std::atomic<float>* audioMatrixParams[ProjectConfig::numOperators][ProjectConfig::numOperators] { nullptr };
};
