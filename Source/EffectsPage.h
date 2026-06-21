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

        // Target selector
        targetSelector.addItemList (ModChoices::fxtargets(), 1);
        targetSelector.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (targetSelector);

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

        // Depth knob
        depthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        depthSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 0, 0);
        addAndMakeVisible (depthSlider);
        depthLabel.setText ("Depth", juce::dontSendNotification);
        depthLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (depthLabel);

        // APVTS attachments
        waveAttach   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, "FX_LFO_WAVE_"  + s, waveSelector);
        targetAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, "FX_LFO_TGT_"   + s, targetSelector);
        syncAttach   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, "FX_LFO_SYNC_"  + s, syncButton);
        rateAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, "FX_LFO_RATE_"  + s, rateSlider);
        rateSyncAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, "FX_LFO_RATE_SYNC_" + s, rateSyncSlider);
        depthAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, "FX_LFO_DEPTH_" + s, depthSlider);

        // Swap knob visibility
        syncButton.onClick = [this]() 
        { 
            bool isSynced = syncButton.getToggleState();
            rateSlider.setVisible (!isSynced);
            rateSyncSlider.setVisible (isSynced);
        };
        
        // 4. Force the initial state on boot
        syncButton.onClick();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colors.background);
        g.setColour (colors.text.withAlpha (0.1f));
        g.drawRect (getLocalBounds(), 1);
    }

    void lookAndFeelChanged() override
    {
        juce::Component::lookAndFeelChanged();

        label.setColour      (juce::Label::textColourId, colors.text);
        rateLabel.setColour  (juce::Label::textColourId, colors.text);
        depthLabel.setColour (juce::Label::textColourId, colors.text);

        waveSelector.setColour   (juce::ComboBox::backgroundColourId, colors.surface);
        waveSelector.setColour   (juce::ComboBox::textColourId,       colors.text);
        targetSelector.setColour (juce::ComboBox::backgroundColourId, colors.surface);
        targetSelector.setColour (juce::ComboBox::textColourId,       colors.text);

        rateSlider.setColour  (juce::Slider::textBoxBackgroundColourId, colors.surface);
        rateSlider.setColour  (juce::Slider::textBoxTextColourId,       colors.text);
        rateSyncSlider.setColour  (juce::Slider::textBoxBackgroundColourId, colors.surface);
        rateSyncSlider.setColour  (juce::Slider::textBoxTextColourId,       colors.text);
        depthSlider.setColour (juce::Slider::textBoxBackgroundColourId, colors.surface);
        depthSlider.setColour (juce::Slider::textBoxTextColourId,       colors.text);

        waveSelector.sendLookAndFeelChange();
        targetSelector.sendLookAndFeelChange();
        rateSlider.sendLookAndFeelChange();
        rateSyncSlider.sendLookAndFeelChange();
        depthSlider.sendLookAndFeelChange();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (juce::roundToInt (getWidth() * 0.02f),
                                               juce::roundToInt (getHeight() * 0.03f));

        // Top row: label | wave selector | sync button
        auto topRow = area.removeFromTop (juce::roundToInt (getHeight() * 0.24f));
        label.setBounds        (topRow.removeFromLeft  (juce::roundToInt (topRow.getWidth() * 0.20f)));
        syncButton.setBounds   (topRow.removeFromRight (juce::roundToInt (topRow.getWidth() * 0.28f)).reduced (2));
        waveSelector.setBounds (topRow.reduced (2));

        // Middle row: target selector (full width) — no separate gap row, just a small inset
        targetSelector.setBounds (area.removeFromTop (juce::roundToInt (getHeight() * 0.24f)).reduced (2, 1));

        // Bottom row: Rate slider | Depth slider, label-above, value-below each
        int textBoxW = juce::jmax (30, juce::roundToInt (getWidth()  * 0.4f));
        int textBoxH = juce::jmax (12, juce::roundToInt (getHeight() * 0.2f));
        int labelH   = juce::jmax (10, juce::roundToInt (getHeight() * 0.16f));
        int trackH   = juce::jlimit (4, 18, area.getHeight() - labelH - textBoxH);
        int knobW    = area.getWidth() / 2;

        auto rateArea = area.removeFromLeft (knobW);
        rateLabel.setBounds (rateArea.removeFromTop (labelH));
        rateSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        rateSlider.setBounds (rateArea.removeFromTop (trackH + textBoxH).reduced (2, 0));
        rateSyncSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        rateSyncSlider.setBounds (rateSlider.getBounds()); //put in the same place

        depthLabel.setBounds (area.removeFromTop (labelH));
        depthSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        depthSlider.setBounds (area.removeFromTop (trackH + textBoxH).reduced (2, 0));
    }

private:
    OAOColors& colors;

    juce::Label      label;
    juce::ComboBox   waveSelector;
    juce::ComboBox   targetSelector;
    juce::TextButton syncButton;
    juce::Slider     rateSlider,  depthSlider, rateSyncSlider;
    juce::Label      rateLabel,   depthLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   syncAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   rateAttach, depthAttach, rateSyncAttach;
};

// Effects slot - we make three of these to show the effects
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
        
        mixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 0, 0);
        addAndMakeVisible (mixSlider);
        mixLabel.setText ("Mix", juce::dontSendNotification);
        mixLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (mixLabel);
        
        for (int i = 0; i < 4; ++i)
        {
            knobs[i].setSliderStyle (juce::Slider::LinearHorizontal);
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
        g.fillAll (colors.background);
        g.setColour (colors.text.withAlpha (0.1f));
        g.drawRect (getLocalBounds(), 1);
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

        // Left block: tiny slot tag on top, effect selector underneath — sized to its own
        // content rather than the full slot height. Everything else sits to its right.
        int leftBlockW = juce::jmax (60, juce::roundToInt (getWidth() * 0.16f));
        int tagH       = juce::jmax (10, juce::roundToInt (getHeight() * 0.16f));
        int selectorH  = juce::jmax (16, juce::roundToInt (getHeight() * 0.26f));

        auto leftBlock = area.removeFromLeft (leftBlockW);
        slotLabel.setBounds      (leftBlock.removeFromTop (tagH));
        effectSelector.setBounds (leftBlock.removeFromTop (selectorH).reduced (2));

        area.removeFromLeft (juce::roundToInt (getWidth() * 0.01f)); // small gap before knobs

        int textBoxW  = juce::jmax (32, juce::roundToInt (getWidth()  * 0.16f));
        int textBoxH  = juce::jmax (12, juce::roundToInt (getHeight() * 0.18f));
        int labelH    = juce::jmax (10, juce::roundToInt (getHeight() * 0.16f));
        int trackH    = juce::jlimit (4, 18, area.getHeight() - labelH - textBoxH);

        auto mixArea = area.removeFromLeft (juce::roundToInt (area.getWidth() * 0.16f));
        mixLabel.setBounds (mixArea.removeFromTop (labelH));
        mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        mixSlider.setBounds (mixArea.removeFromTop (trackH + textBoxH).reduced (2, 0));

        int knobW = area.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto knobArea = area.removeFromLeft (knobW);
            knobLabels[i].setBounds (knobArea.removeFromTop (labelH));
            knobs[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
            knobs[i].setBounds (knobArea.removeFromTop (trackH + textBoxH).reduced (2, 0));
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
        float gapH    = juce::jmax (2.0f, h * 0.006f);
        float padding = h * 0.01f;

        auto full    = getLocalBounds().reduced (w * 0.005f, padding);
        auto fxArea  = full.removeFromLeft ((int)(full.getWidth() * splitRatio));
        auto lfoArea = full;

        // FX slots
        int slotH = (int)((fxArea.getHeight() - (ProjectConfig::numEffects - 1) * gapH) / ProjectConfig::numEffects);
        for (int i = 0; i < ProjectConfig::numEffects; ++i)
        {
            slots[i]->setBounds (fxArea.removeFromTop (slotH).reduced (0, (int)(h * 0.002f)));
            if (i < (ProjectConfig::numEffects - 1)) fxArea.removeFromTop ((int)gapH);
        }

        // LFO sidebar
        int lfoH = lfoArea.getHeight() / ProjectConfig::numEffects;
        for (int i = 0; i < ProjectConfig::numEffects; ++i)
            lfoSlots[i]->setBounds (lfoArea.removeFromTop (lfoH).reduced (2, (int)(h * 0.002f)));
    }

private:
    static constexpr float splitRatio = 0.75f; // controls split between effects and effect lfos
    OAOColors& colors;
    std::vector<std::unique_ptr<EffectsSlot>> slots;
    std::vector<std::unique_ptr<FXLFOSlot>>   lfoSlots;
};
