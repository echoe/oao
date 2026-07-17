//FMVoice.h
#pragma once
#include <JuceHeader.h>
#include "FMOperator.h"
#include "SynthEffect.h"
#include "Constants.h"
#include "WaveTable.h"
#include <array>
#include <memory>

struct OperatorParameterCache 
{
    std::atomic<float>* mode { nullptr };
    std::atomic<float>* wave { nullptr };
    std::atomic<float>* effectType { nullptr };
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
    void setBaseFrequency (float freq) noexcept { baseFrequency = freq; }
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int controllerNumber, int newControllerValue) override;
    void setOversamplingFactor (int factor);
    void prepare (double sampleRate, int samplesPerBlock, WaveTable* wt);
    void setDAWTempo (float newBPM) noexcept;
    void resetVoiceState();
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;
    // Distribute a loaded audio buffer to a specific operator index for sample mode.
    void setSampleData (int opIndex, std::shared_ptr<juce::AudioBuffer<float>> buffer) noexcept
    {
        if (opIndex >= 0 && opIndex < ProjectConfig::numOperators)
            operators[opIndex].setSampleData (buffer);
    }
    //effects
    std::atomic<float> fxRatioMods[ProjectConfig::numEffects]  { 0.0f, 0.0f, 0.0f };
    std::atomic<float> fxDetuneMods[ProjectConfig::numEffects] { 0.0f, 0.0f, 0.0f };
    std::atomic<float> fxPhaseMods[ProjectConfig::numEffects]  { 0.0f, 0.0f, 0.0f };
    std::atomic<float> fxFoldMods[ProjectConfig::numEffects]   { 0.0f, 0.0f, 0.0f };
    std::atomic<float> fxLevelMods[ProjectConfig::numEffects]  { 0.0f, 0.0f, 0.0f };
    //mod sources
    std::atomic<float> currentVelocity  { 0.0f };
    std::atomic<float> currentModWheel  { 0.0f };
    std::atomic<float> fxLfoOutputs[ProjectConfig::numEffects] { 0.0f, 0.0f, 0.0f };

private:
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
    // Macros — one bipolar value each, routed straight to two independent targets
    std::atomic<float>* macroVal[ProjectConfig::numMacros]  { nullptr };
    std::atomic<float>* macroTgtA[ProjectConfig::numMacros] { nullptr };
    std::atomic<float>* macroTgtB[ProjectConfig::numMacros] { nullptr };
    // FM and Audio routing grids
    std::atomic<float>* matrixParams[ProjectConfig::numOperators][ProjectConfig::numOperators]      { nullptr };
    std::atomic<float>* audioMatrixParams[ProjectConfig::numOperators][ProjectConfig::numOperators] { nullptr };
};
