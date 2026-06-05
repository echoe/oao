// OperatorsPage.h
#pragma once
#include <JuceHeader.h>
#include "Constants.h"
#include "OAOColors.h"

struct CompactOperatorGroup : public juce::Component
{
    CompactOperatorGroup (juce::AudioProcessorValueTreeState& valueTreeState, int opIndex, OAOColors& c)
        : apvts (valueTreeState), opNum (juce::String (opIndex + 1)), colors (c)
    {
        auto setupSlider = [this] (juce::Slider& slider, juce::Label& label, const juce::String& text, bool rotary)
        {
            slider.setSliderStyle (rotary ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearVertical);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 12);
            addAndMakeVisible (slider);
            
            label.setText (text, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (label);
        };
        setupSlider (ratioSlider, ratioLabel, "Ratio", true);
        setupSlider (detuneSlider, detuneLabel, "Detune", true);
        setupSlider (phaseSlider, phaseLabel, "Phase", true);
        setupSlider (foldSlider, foldLabel, "Fold", true);
        setupSlider (attackSlider, attackLabel, "Attack", false);
        setupSlider (decaySlider, decayLabel, "Decay", false);
        setupSlider (sustainSlider, sustainLabel, "Suspend", false);
        setupSlider (releaseSlider, releaseLabel, "Release", false);
        opHeaderLabel.setText ("OPERATOR " + opNum, juce::dontSendNotification);
        opHeaderLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (opHeaderLabel);
        syncButton.setButtonText ("Sync");
        syncButton.setClickingTogglesState (true);
        addAndMakeVisible (syncButton);
        modeSelector.addItemList ({ "Wave", "Additive", "Filter", "Ext. In" }, 1);
        addAndMakeVisible (modeSelector);
        waveShapeSelector.addItemList ({ "Sine", "Triangle", "Saw", "Square", "White Noise", "Pink Noise" }, 1);
        addAndMakeVisible (waveShapeSelector);
        filterTypeSelector.addItemList (ProjectConfig::getFilterTypeChoices(), 1);
addAndMakeVisible (filterTypeSelector);

        // APVTS Links
        syncAttach    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "TEMPO_SYNC_" + opNum, syncButton);
        ratioAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "RATIO_" + opNum, ratioSlider);
        detuneAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "DETUNE_" + opNum, detuneSlider);
        phaseAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "PHASE_" + opNum, phaseSlider);
        foldAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "FOLD_" + opNum, foldSlider);
        attackAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "ATTACK_" + opNum, attackSlider);
        decayAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "DECAY_" + opNum, decaySlider);
        sustainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "SUSTAIN_" + opNum, sustainSlider);
        releaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "RELEASE_" + opNum, releaseSlider);
        modeAttach      = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "MODE_" + opNum, modeSelector);
        waveShapeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "WAVE_SHAPE_" + opNum, waveShapeSelector);
        filterTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "FILTER_TYPE_" + opNum, filterTypeSelector);

        // Safe UI state triggers using the stored class member reference
        modeSelector.onChange = [this]() { updateUIState(); };
        syncButton.onClick    = [this]() { updateUIState(); };
        filterTypeSelector.onChange = [this]() {updateUIState(); };
        updateUIState();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (colors.text.withAlpha (0.15f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 4.0f, 1.0f);

        g.setColour (colors.text.withAlpha (0.02f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 4.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (6);
        auto topStrip = area.removeFromTop (22);
int topStripSize = topStrip.getWidth();
        // 1. Position the Header and Sync Button
        opHeaderLabel.setBounds (topStrip.removeFromLeft (topStripSize*0.25f));
        syncButton.setBounds (topStrip.removeFromLeft (topStripSize*0.2f).reduced (1));
        // 2. FIX: Carve out 75 pixels for the Mode Selector dropdown!
        modeSelector.setBounds (topStrip.removeFromLeft (topStripSize*0.25f).reduced (1));
        // 3. The remaining space goes to the conditional dropdowns. if/else.
        if (filterTypeSelector.isVisible())
        {
            filterTypeSelector.setBounds (topStrip.reduced (1));
        }
        else
        {
            waveShapeSelector.setBounds (topStrip.reduced (1));
        }
    
        auto knobZone = area.removeFromTop (area.getHeight() / 2 + 5);
        int knobWidth = knobZone.getWidth() / 4;
    
        auto rArea = knobZone.removeFromLeft (knobWidth); ratioLabel.setBounds (rArea.removeFromTop (15)); ratioSlider.setBounds (rArea);
        auto dArea = knobZone.removeFromLeft (knobWidth); detuneLabel.setBounds (dArea.removeFromTop (15)); detuneSlider.setBounds (dArea);
        auto pArea = knobZone.removeFromLeft (knobWidth); phaseLabel.setBounds (pArea.removeFromTop (15));  phaseSlider.setBounds (pArea);
        auto lArea = knobZone; foldLabel.setBounds (lArea.removeFromTop (15)); foldSlider.setBounds (lArea);
    
        int sliderWidth = area.getWidth() / 4;
        auto aArea = area.removeFromLeft (sliderWidth);   attackLabel.setBounds (aArea.removeFromTop (20));   attackSlider.setBounds (aArea);
        auto decArea = area.removeFromLeft (sliderWidth);  decayLabel.setBounds (decArea.removeFromTop (20)); decaySlider.setBounds (decArea);
        auto sArea = area.removeFromLeft (sliderWidth);    sustainLabel.setBounds (sArea.removeFromTop (20)); sustainSlider.setBounds (sArea);
        auto relArea = area;                              releaseLabel.setBounds (relArea.removeFromTop (20)); releaseSlider.setBounds (relArea);
    }

    void lookAndFeelChanged() override
    {
        juce::Component::lookAndFeelChanged();

        // 1. Update Labels
        opHeaderLabel.setColour (juce::Label::textColourId, colors.text);
        ratioLabel.setColour    (juce::Label::textColourId, colors.text);
        detuneLabel.setColour   (juce::Label::textColourId, colors.text);
        phaseLabel.setColour    (juce::Label::textColourId, colors.text);
        foldLabel.setColour     (juce::Label::textColourId, colors.text);
        attackLabel.setColour   (juce::Label::textColourId, colors.text);
        decayLabel.setColour    (juce::Label::textColourId, colors.text);
        sustainLabel.setColour  (juce::Label::textColourId, colors.text);
        releaseLabel.setColour  (juce::Label::textColourId, colors.text);

        syncButton.setColour (juce::ToggleButton::textColourId, colors.text);

        // 2. Helper lambda to update ComboBoxes cleanly
        auto updateComboBox = [this](juce::ComboBox& cb) {
            cb.setColour (juce::ComboBox::backgroundColourId, colors.surface);
            cb.setColour (juce::ComboBox::textColourId, colors.text);
            cb.sendLookAndFeelChange();
        };

        updateComboBox (modeSelector);
        updateComboBox (waveShapeSelector);
        updateComboBox (filterTypeSelector);

        // 3. Helper lambda to update Sliders cleanly
        auto updateSlider = [this](juce::Slider& s) {
            s.setColour (juce::Slider::textBoxBackgroundColourId, colors.surface);
            s.setColour (juce::Slider::textBoxTextColourId, colors.text);
            s.sendLookAndFeelChange();
        };

        updateSlider (ratioSlider);
        updateSlider (detuneSlider);
        updateSlider (phaseSlider);
        updateSlider (foldSlider);
        updateSlider (attackSlider);
        updateSlider (decaySlider);
        updateSlider (sustainSlider);
        updateSlider (releaseSlider);
    }

private:
    // Combined logic method avoids layout text fighting
    OAOColors& colors;
    void updateUIState()
    {
        int selectedMode   = modeSelector.getSelectedId();
        bool isWaveMode    = (selectedMode == 1);
        bool isAdditiveMode = (selectedMode == 2);
        bool isFilterMode  = (selectedMode == 3);
        bool isExtAudioMode = (selectedMode == 4);
        bool isSynced      = syncButton.getToggleState();
    
        int selectedFilter = filterTypeSelector.getSelectedId();
    
        waveShapeSelector.setVisible (isWaveMode);
        filterTypeSelector.setVisible (isFilterMode);
    
        if (isExtAudioMode)
        {
            ratioLabel.setText  ("Gain", juce::dontSendNotification);
            detuneLabel.setText ("Tone", juce::dontSendNotification);
            phaseLabel.setText  ("Mod",  juce::dontSendNotification);
            foldLabel.setText   ("Fold", juce::dontSendNotification);
        }
        else if (isAdditiveMode)
        {
            ratioLabel.setText  (isSynced ? "Sync Rate" : "Ratio", juce::dontSendNotification);
            detuneLabel.setText ("Tilt",     juce::dontSendNotification);
            phaseLabel.setText  ("Stretch",  juce::dontSendNotification);
            foldLabel.setText   ("Odd/Even", juce::dontSendNotification);
        }
        else if (isFilterMode)
        {
            // Use shared label lookup — filterTypeSelector is 1-indexed, getFilterKnobLabels is 0-indexed
            auto labels = ProjectConfig::getFilterKnobLabels (selectedFilter - 1);
            ratioLabel.setText  (labels[0], juce::dontSendNotification);
            detuneLabel.setText (labels[1], juce::dontSendNotification);
            phaseLabel.setText  (labels[2], juce::dontSendNotification);
            foldLabel.setText   (labels[3], juce::dontSendNotification);
        }
        else // Wave mode
        {
            ratioLabel.setText  (isSynced ? "Sync Rate" : "Ratio", juce::dontSendNotification);
            detuneLabel.setText ("Detune", juce::dontSendNotification);
            phaseLabel.setText  ("Phase",  juce::dontSendNotification);
            foldLabel.setText   ("Fold",   juce::dontSendNotification);
        }
    
        ratioSlider.setTextValueSuffix (isSynced ? "x" : "");
        if (getWidth() > 0 && getHeight() > 0)
            resized();
    }

    // Keep state reference completely safe inside class lifecycle
    juce::AudioProcessorValueTreeState& apvts;
    juce::String opNum;

    juce::Slider ratioSlider, detuneSlider, phaseSlider, foldSlider;
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    
    juce::Label opHeaderLabel;
    juce::Label ratioLabel, detuneLabel, phaseLabel, foldLabel;
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel;
    
    juce::ComboBox modeSelector;
    juce::ComboBox waveShapeSelector;
    juce::ComboBox filterTypeSelector;
    juce::ToggleButton syncButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveShapeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ratioAttach, detuneAttach, phaseAttach, foldAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach, decayAttach, sustainAttach, releaseAttach;
};

// --- THE PARENT VIEW MANAGER CLASS ---
class OperatorsPage : public juce::Component
{
public:
    OperatorsPage (juce::AudioProcessorValueTreeState& apvts, OAOColors& c) : colors (c)
    {
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            // FIX: Pass 'colors' as the 3rd argument here!
            opModules.push_back (std::make_unique<CompactOperatorGroup> (apvts, i, colors));
            addAndMakeVisible (*opModules.back());
        }
    }

    // NEW: Trigger updates in the children
    void lookAndFeelChanged() override
    {
        juce::Component::lookAndFeelChanged();

        for (auto& module : opModules)
        {
            if (module != nullptr)
                module->lookAndFeelChanged();
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();

        int rows = 2;
        int cols = 3;
        int cellWidth = area.getWidth() / cols;
        int cellHeight = area.getHeight() / rows;

        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < cols; ++c)
            {
                int index = (r * cols) + c;
                if (index < static_cast<int>(opModules.size()))
                {
                    opModules[index]->setBounds (c * cellWidth, r * cellHeight, cellWidth, cellHeight);
                }
            }
        }
    }

private:
    OAOColors& colors; // FIX: This was missing, causing a compiler error!
    std::vector<std::unique_ptr<CompactOperatorGroup>> opModules;
};
