//EffectsPage.h
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Constants.h"
#include "OAOColors.h"

//  FX LFO SLOT  — one row in the LFO sidebar
class FXLFOSlot : public juce::Component
{
public:
    FXLFOSlot (juce::AudioProcessorValueTreeState& apvts, int lfoIndex, OAOColors& c)
        : colors (c)
    {
        juce::String s = juce::String (lfoIndex + 1);

        // Label
        label.setText ("LFO " + s, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);

        // Waveform selector
        waveSelector.addItem ("Sine",     1);
        waveSelector.addItem ("Triangle", 2);
        waveSelector.addItem ("Saw",      3);
        waveSelector.addItem ("Square",   4);
        waveSelector.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (waveSelector);

        // Target selector. Effects if effects-only, otherwise full list
	addAndMakeVisible(targetSelector);
        addAndMakeVisible(targetSelector2);
        #ifndef OAO_FX_ONLY
        buildTargetMenu();
        #else
	targetSelector.addItemList (ModChoices::fxtargets(), 1);
        targetSelector.setSelectedId (1, juce::dontSendNotification);
        targetSelector2.addItemList (ModChoices::fxtargets(), 1);
        targetSelector2.setSelectedId (1, juce::dontSendNotification);
        #endif

        // Sync button
        syncButton.setButtonText ("Sync");
        syncButton.setClickingTogglesState (true);
        addAndMakeVisible (syncButton);

        // Rate knob
        rateSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        rateSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 0, 0);
        addAndMakeVisible (rateSlider);
        // Second Rate knob for if it's synced
        rateSyncSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        rateSyncSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 0, 0);
        addAndMakeVisible (rateSyncSlider);
        
        rateLabel.setText ("Rate", juce::dontSendNotification);
        rateLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (rateLabel);

        // Depth knob (destination A)
        depthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        depthSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 0, 0);
        addAndMakeVisible (depthSlider);
        depthLabel.setText ("Depth A", juce::dontSendNotification);
        depthLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (depthLabel);

        // Depth knob (destination B) — second, independent target
        depthSlider2.setSliderStyle (juce::Slider::LinearHorizontal);
        depthSlider2.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 0, 0);
        addAndMakeVisible (depthSlider2);
        depthLabel2.setText ("Depth B", juce::dontSendNotification);
        depthLabel2.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (depthLabel2);

        // APVTS attachments
        waveAttach   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, "FX_LFO_WAVE_"  + s, waveSelector);
        targetAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, "FX_LFO_TGT_"   + s, targetSelector);
        targetAttach2 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, "FX_LFO_TGT2_"  + s, targetSelector2);
        syncAttach   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, "FX_LFO_SYNC_"  + s, syncButton);
        rateAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, "FX_LFO_RATE_"  + s, rateSlider);
        rateSyncAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, "FX_LFO_RATE_SYNC_" + s, rateSyncSlider);
        depthAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, "FX_LFO_DEPTH_" + s, depthSlider);
        depthAttach2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, "FX_LFO_DEPTH2_" + s, depthSlider2);

        // Swap knob visibility
        syncButton.onClick = [this]() 
        { 
            bool isSynced = syncButton.getToggleState();
            rateSlider.setVisible (!isSynced);
            rateSyncSlider.setVisible (isSynced);
        };
        // Force initial state on startup
        syncButton.onClick();
    }

    void buildTargetMenu()
    {
        ModChoices::buildTargetMenu (targetSelector);
        ModChoices::buildTargetMenu (targetSelector2);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (colors.background);
        g.fillRoundedRectangle (bounds, 4.0f);

        g.setColour (colors.text.withAlpha (0.15f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 4.0f, 1.0f);
    }

    void lookAndFeelChanged() override
    {
        juce::Component::lookAndFeelChanged();

        label.setColour      (juce::Label::textColourId, colors.text);
        rateLabel.setColour  (juce::Label::textColourId, colors.text);
        depthLabel.setColour (juce::Label::textColourId, colors.text);
        depthLabel2.setColour (juce::Label::textColourId, colors.text);

        waveSelector.setColour   (juce::ComboBox::backgroundColourId, colors.surface);
        waveSelector.setColour   (juce::ComboBox::textColourId,       colors.text);
        targetSelector.setColour (juce::ComboBox::backgroundColourId, colors.surface);
        targetSelector.setColour (juce::ComboBox::textColourId,       colors.text);
        targetSelector2.setColour (juce::ComboBox::backgroundColourId, colors.surface);
        targetSelector2.setColour (juce::ComboBox::textColourId,       colors.text);

        rateSlider.setColour  (juce::Slider::textBoxBackgroundColourId, colors.surface);
        rateSlider.setColour  (juce::Slider::textBoxTextColourId,       colors.text);
        rateSyncSlider.setColour  (juce::Slider::textBoxBackgroundColourId, colors.surface);
        rateSyncSlider.setColour  (juce::Slider::textBoxTextColourId,       colors.text);
        depthSlider.setColour (juce::Slider::textBoxBackgroundColourId, colors.surface);
        depthSlider.setColour (juce::Slider::textBoxTextColourId,       colors.text);
        depthSlider2.setColour (juce::Slider::textBoxBackgroundColourId, colors.surface);
        depthSlider2.setColour (juce::Slider::textBoxTextColourId,       colors.text);

        waveSelector.sendLookAndFeelChange();
        targetSelector.sendLookAndFeelChange();
        targetSelector2.sendLookAndFeelChange();
        rateSlider.sendLookAndFeelChange();
        rateSyncSlider.sendLookAndFeelChange();
        depthSlider.sendLookAndFeelChange();
        depthSlider2.sendLookAndFeelChange();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (juce::roundToInt (getWidth() * 0.02f),
                                               juce::roundToInt (getHeight() * 0.03f));

        // Top row: label | wave selector | sync button
        auto topRow = area.removeFromTop (juce::roundToInt (getHeight() * 0.20f));
        label.setBounds        (topRow.removeFromLeft  (juce::roundToInt (topRow.getWidth() * 0.20f)));
        syncButton.setBounds   (topRow.removeFromRight (juce::roundToInt (topRow.getWidth() * 0.28f)).reduced (2));
        waveSelector.setBounds (topRow.reduced (2));

        // Target A / Target B selectors (full width, stacked) — two independent destinations
        targetSelector.setBounds  (area.removeFromTop (juce::roundToInt (getHeight() * 0.18f)).reduced (2, 1));
        targetSelector2.setBounds (area.removeFromTop (juce::roundToInt (getHeight() * 0.18f)).reduced (2, 1));

        // Bottom row: Rate | Depth A | Depth B, label-above, value-below each
        int textBoxW = juce::jmax (26, juce::roundToInt (getWidth()  * 0.28f));
        int textBoxH = juce::jmax (12, juce::roundToInt (getHeight() * 0.2f));
        int labelH   = juce::jmax (10, juce::roundToInt (getHeight() * 0.16f));
        int trackH   = juce::jlimit (4, 18, area.getHeight() - labelH - textBoxH);
        int knobW    = area.getWidth() / 3;

        auto rateArea = area.removeFromLeft (knobW);
        rateLabel.setBounds (rateArea.removeFromTop (labelH));
        rateSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        rateSlider.setBounds (rateArea.removeFromTop (trackH + textBoxH).reduced (2, 0));
        rateSyncSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        rateSyncSlider.setBounds (rateSlider.getBounds()); //put in the same place for swapping

        auto depthAArea = area.removeFromLeft (knobW);
        depthLabel.setBounds (depthAArea.removeFromTop (labelH));
        depthSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        depthSlider.setBounds (depthAArea.removeFromTop (trackH + textBoxH).reduced (2, 0));

        auto depthBArea = area;
        depthLabel2.setBounds (depthBArea.removeFromTop (labelH));
        depthSlider2.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        depthSlider2.setBounds (depthBArea.removeFromTop (trackH + textBoxH).reduced (2, 0));
    }

private:
    OAOColors& colors;

    juce::Label      label;
    juce::ComboBox   waveSelector;
    juce::ComboBox   targetSelector, targetSelector2;
    juce::TextButton syncButton;
    juce::Slider     rateSlider,  depthSlider, depthSlider2, rateSyncSlider;
    juce::Label      rateLabel,   depthLabel,  depthLabel2;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAttach, targetAttach2;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   syncAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   rateAttach, depthAttach, depthAttach2, rateSyncAttach;
};

// Effects slot - we make these to show the effects
class EffectsSlot : public juce::Component
{
public:
    EffectsSlot (juce::AudioProcessorValueTreeState& apvts, int slotIndex, OAOColors& c)
        : colors (c)
    {
        juce::String s = juce::String (slotIndex + 1);
        effectSelector.addItemList (ProjectConfig::getEffectTypeChoices(), 1);
        effectSelector.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (effectSelector);
        
        mixSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 0, 0);
        addAndMakeVisible (mixSlider);
        mixLabel.setText ("Mix", juce::dontSendNotification);
        mixLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (mixLabel);
        
        for (int i = 0; i < 4; ++i)
        {
            knobs[i].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            knobs[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 0, 0);
            addAndMakeVisible (knobs[i]);
            addAndMakeVisible (knobLabels[i]);
            knobLabels[i].setJustificationType (juce::Justification::centred);
        }
        slotLabel.setText ("FX " + s, juce::dontSendNotification);
        slotLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (slotLabel);
        
        effectAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, "FX_TYPE_" + s, effectSelector);
        mixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, "FX_MIX_" + s, mixSlider);
        const juce::StringArray knobIDs = { "FX_RATIO_", "FX_DETUNE_", "FX_PHASE_", "FX_FOLD_" };
        for (int i = 0; i < 4; ++i)
            knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, knobIDs[i] + s, knobs[i]);
        effectSelector.onChange = [this] { updateLabels(); };
        updateLabels();
    }

    void updateLabels()
    {
        int typeIndex = effectSelector.getSelectedId() - 1;
        auto labels   = ProjectConfig::getEffectKnobLabels (typeIndex);
        for (int i = 0; i < 4; ++i)
            knobLabels[i].setText (labels[i], juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (colors.background);
        g.fillRoundedRectangle (bounds, 4.0f);

        g.setColour (colors.text.withAlpha (0.15f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 4.0f, 1.0f);
    }

    void lookAndFeelChanged() override
    {
        juce::Component::lookAndFeelChanged();

        slotLabel.setColour (juce::Label::textColourId, colors.text);
        mixLabel.setColour  (juce::Label::textColourId, colors.text);
        for (int i = 0; i < 4; ++i)
            knobLabels[i].setColour (juce::Label::textColourId, colors.text);

        effectSelector.setColour (juce::ComboBox::backgroundColourId, colors.surface);
        effectSelector.setColour (juce::ComboBox::textColourId,       colors.text);

        mixSlider.setColour (juce::Slider::textBoxBackgroundColourId, colors.surface);
        mixSlider.setColour (juce::Slider::textBoxTextColourId,       colors.text);

        for (int i = 0; i < 4; ++i)
        {
            knobs[i].setColour (juce::Slider::textBoxBackgroundColourId, colors.surface);
            knobs[i].setColour (juce::Slider::textBoxTextColourId,       colors.text);
            knobs[i].sendLookAndFeelChange();
        }
        effectSelector.sendLookAndFeelChange();
        mixSlider.sendLookAndFeelChange();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (juce::roundToInt (getWidth() * 0.015f),
                                               juce::roundToInt (getHeight() * 0.03f));

        // Left block: select optoins for the operator
        int leftBlockW = juce::jmax (60, juce::roundToInt (getWidth() * 0.16f));
        int tagH       = juce::jmax (10, juce::roundToInt (getHeight() * 0.16f));
        int selectorH  = juce::jmax (16, juce::roundToInt (getHeight() * 0.26f));

        auto leftBlock = area.removeFromLeft (leftBlockW);
        slotLabel.setBounds      (leftBlock.removeFromTop (tagH));
        effectSelector.setBounds (leftBlock.removeFromTop (selectorH).reduced (2));

        area.removeFromLeft (juce::roundToInt (getWidth() * 0.01f)); // small gap before knobs

        // Shared text box / label / knob sizing derived from OAOColors and constants.h values
        int textBoxW = juce::roundToInt (sharedKnobTarget * ProjectConfig::textBoxWidthFraction);
        int textBoxH = juce::jlimit (12, 70, juce::roundToInt (sharedKnobTarget * colors.textBoxHeightFraction));
        int labelH   = juce::jmax (10, juce::roundToInt (sharedKnobTarget * colors.textBoxHeightFraction));
        int targetBoxSize = sharedKnobTarget + 8;
        auto clampKnob = [targetBoxSize, textBoxH] (juce::Rectangle<int> box)
        {
            int w = juce::jmin (box.getWidth(),  targetBoxSize);
            int h = juce::jmin (box.getHeight(), targetBoxSize + textBoxH);
            return box.withSizeKeepingCentre (w, h);
        };

        auto mixArea = area.removeFromLeft (juce::roundToInt (area.getWidth() * 0.16f));
        mixLabel.setBounds (mixArea.removeFromTop (labelH));
        mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        mixSlider.setBounds (clampKnob (mixArea));

        int knobW = area.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto knobArea = area.removeFromLeft (knobW);
            knobLabels[i].setBounds (knobArea.removeFromTop (labelH));
            knobs[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
            knobs[i].setBounds (clampKnob (knobArea));
        }
    }

private:
    OAOColors& colors;
    juce::Label      slotLabel;
    juce::ComboBox   effectSelector;
    juce::Slider     mixSlider;
    juce::Label      mixLabel;
    juce::Slider     knobs[4];
    juce::Label      knobLabels[4];

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> effectAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   mixAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   knobAttachments[4];

public:
    // Set from EffectsPage::resized()
    void setSharedKnobTarget (int targetDiameter)
    {   
	if (sharedKnobTarget != targetDiameter)
        {
            sharedKnobTarget = targetDiameter;
            resized();
        }
    }
private:
    int sharedKnobTarget = 90; //fallback default, not used normally
};


//  EFFECTS PAGE  — show the FX and sidebar
class EffectsPage : public juce::Component
{
public:
    EffectsPage (juce::AudioProcessorValueTreeState& apvts, OAOColors& c)
        : colors (c)
    {
        for (int i = 0; i < ProjectConfig::numEffects; ++i)
        {
            slots.push_back (std::make_unique<EffectsSlot> (apvts, i, colors));
            addAndMakeVisible (*slots.back());

            lfoSlots.push_back (std::make_unique<FXLFOSlot> (apvts, i, colors));
            addAndMakeVisible (*lfoSlots.back());
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colors.panelGap);

        float splitX = getWidth() * splitRatio;

        g.setColour (colors.text.withAlpha (0.15f));
        g.drawVerticalLine ((int)splitX, getHeight() * 0.01f, getHeight() * 0.99f);
    }

    void lookAndFeelChanged() override
    {
        juce::Component::lookAndFeelChanged();

        for (auto& slot : slots)
            if (slot != nullptr) slot->lookAndFeelChanged();

        for (auto& lfo : lfoSlots)
            if (lfo != nullptr) lfo->lookAndFeelChanged();
    }

    void resized() override
    {
        float h = getHeight();
        float w = getWidth();
        int gapH = juce::roundToInt (juce::jmax (2.0f, h * 0.006f));

        auto full    = getLocalBounds().reduced (
            juce::roundToInt (w * ProjectConfig::outerMargin),
            juce::roundToInt (h * ProjectConfig::outerMargin));
        auto fxArea  = full.removeFromLeft (juce::roundToInt (full.getWidth() * splitRatio));
	auto lfoArea = full;

        // Computed from the true page dimensions (this component, not a per-slot card)
        int sharedKnobTarget = juce::roundToInt (juce::jmin (w, h) * colors.knobDiameterFraction);

        // FX slots 
        int slotH = juce::roundToInt ((fxArea.getHeight() - (ProjectConfig::numEffects - 1) * gapH) / (float)ProjectConfig::numEffects);
	for (int i = 0; i < ProjectConfig::numEffects; ++i)
        {
            bool isLast = (i == ProjectConfig::numEffects - 1);
            // SharedKnob must be set before Bounds
            slots[i]->setSharedKnobTarget (sharedKnobTarget);
            slots[i]->setBounds (fxArea.removeFromTop (isLast ? fxArea.getHeight() : slotH));
            if (! isLast) fxArea.removeFromTop (gapH);
	}

        // LFO sidebar
        int lfoH = juce::roundToInt ((lfoArea.getHeight() - (ProjectConfig::numEffects - 1) * gapH) / (float)ProjectConfig::numEffects);
	for (int i = 0; i < ProjectConfig::numEffects; ++i)
        {
            bool isLast = (i == ProjectConfig::numEffects - 1);
            lfoSlots[i]->setBounds (lfoArea.removeFromTop (isLast ? lfoArea.getHeight() : lfoH));
            if (! isLast) lfoArea.removeFromTop (gapH);
        }
    }

private:
    static constexpr float splitRatio = 0.75f; // controls split between effects and effect lfos
    OAOColors& colors;
    std::vector<std::unique_ptr<EffectsSlot>> slots;
    std::vector<std::unique_ptr<FXLFOSlot>>   lfoSlots;
};
