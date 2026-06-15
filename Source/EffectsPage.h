#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Constants.h"
#include "OAOColors.h"

// ============================================================
//  FX LFO SLOT  — one row in the LFO sidebar
// ============================================================
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
        targetSelector.addItemList (ProjectConfig::getFXLFOTargetChoices(), 1);
        targetSelector.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (targetSelector);

        // Sync button
        syncButton.setButtonText ("Sync");
        syncButton.setClickingTogglesState (true);
        addAndMakeVisible (syncButton);

        // Rate knob
        rateSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        rateSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 0, 0);
        addAndMakeVisible (rateSlider);
        rateLabel.setText ("Rate", juce::dontSendNotification);
        rateLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (rateLabel);

        // Depth knob
        depthSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
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
        depthAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, "FX_LFO_DEPTH_" + s, depthSlider);
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
        depthSlider.setColour (juce::Slider::textBoxBackgroundColourId, colors.surface);
        depthSlider.setColour (juce::Slider::textBoxTextColourId,       colors.text);

        waveSelector.sendLookAndFeelChange();
        targetSelector.sendLookAndFeelChange();
        rateSlider.sendLookAndFeelChange();
        depthSlider.sendLookAndFeelChange();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (getWidth() * 0.02f);

        // Top row: label | wave selector | sync button
        auto topRow = area.removeFromTop (getHeight() * 0.17f);
        label.setBounds        (topRow.removeFromLeft  (topRow.getWidth() * 0.20f));
        syncButton.setBounds   (topRow.removeFromRight (topRow.getWidth() * 0.22f).reduced (2));
        waveSelector.setBounds (topRow.reduced (2));

        area.removeFromTop (getHeight() * 0.02f);

        // Middle row: target selector (full width)
        targetSelector.setBounds (area.removeFromTop (getHeight() * 0.17f).reduced (2));

        area.removeFromTop (getHeight() * 0.02f);

        // Bottom row: Rate knob | Depth knob
        int textBoxH = (int)(getHeight() * 0.12f);
        int textBoxW = (int)(getWidth()  * 0.18f);
        int knobW    = area.getWidth() / 2;

        auto rateArea = area.removeFromLeft (knobW);
        rateLabel.setBounds (rateArea.removeFromBottom (getHeight() * 0.15f));
        rateSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        rateSlider.setBounds (rateArea);

        depthLabel.setBounds (area.removeFromBottom (getHeight() * 0.15f));
        depthSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        depthSlider.setBounds (area);
    }

private:
    OAOColors& colors;

    juce::Label      label;
    juce::ComboBox   waveSelector;
    juce::ComboBox   targetSelector;
    juce::TextButton syncButton;
    juce::Slider     rateSlider,  depthSlider;
    juce::Label      rateLabel,   depthLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   syncAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   rateAttach, depthAttach;
};


// ============================================================
//  EFFECTS SLOT  — unchanged from original
// ============================================================
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
        // Mix knob
        mixSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 0, 0);
        addAndMakeVisible (mixSlider);
        mixLabel.setText ("Mix", juce::dontSendNotification);
        mixLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (mixLabel);
        // Four knobs
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
        // Attachments
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
        auto area = getLocalBounds().reduced (getWidth() * 0.02f);

        auto topRow = area.removeFromTop (getHeight() * 0.18f);
        slotLabel.setBounds      (topRow.removeFromLeft (topRow.getWidth() * 0.10f));
        effectSelector.setBounds (topRow.reduced (2));
        area.removeFromTop (getHeight() * 0.005f);

        int textBoxH = (int)(getHeight() * 0.12f);
        int textBoxW = (int)(getWidth()  * 0.1f);

        auto mixArea = area.removeFromLeft (area.getWidth() * 0.12f);
        mixLabel.setBounds (mixArea.removeFromBottom (getHeight() * 0.22f));
        mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        mixSlider.setBounds (mixArea);
        area.removeFromLeft (area.getWidth() * 0.01f);

        int knobW = area.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto knobArea = area.removeFromLeft (knobW);
            knobLabels[i].setBounds (knobArea.removeFromBottom (getHeight() * 0.15f));
            knobs[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
            knobs[i].setBounds (knobArea);
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


// ============================================================
//  EFFECTS PAGE  — 70% FX slots | 30% LFO sidebar
// ============================================================
class EffectsPage : public juce::Component
{
public:
    EffectsPage (juce::AudioProcessorValueTreeState& apvts, OAOColors& c)
        : colors (c)
    {
        for (int i = 0; i < 3; ++i)
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

        // Flow arrows between FX slots
        g.setColour (colors.text.withAlpha (0.3f));
        g.setFont (getHeight() * 0.03f);
        int slotH = (int)(getHeight() / 3);
        for (int i = 0; i < 2; ++i)
        {
            int arrowY = slotH * (i + 1);
            g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x93")),
                        (int)(splitX * 0.5f) - 10, arrowY - 12, 20, 20,
                        juce::Justification::centred);
        }

        // Divider
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
        float arrowH  = h * 0.03f;
        float padding = h * 0.01f;

        auto full    = getLocalBounds().reduced (w * 0.005f, padding);
        auto fxArea  = full.removeFromLeft ((int)(full.getWidth() * splitRatio));
        auto lfoArea = full;

        // FX slots
        int slotH = (int)((fxArea.getHeight() - 2 * arrowH) / 3);
        for (int i = 0; i < 3; ++i)
        {
            slots[i]->setBounds (fxArea.removeFromTop (slotH).reduced (0, (int)(h * 0.005f)));
            if (i < 2) fxArea.removeFromTop ((int)arrowH);
        }

        // LFO sidebar
        int lfoH = lfoArea.getHeight() / 3;
        for (int i = 0; i < 3; ++i)
            lfoSlots[i]->setBounds (lfoArea.removeFromTop (lfoH).reduced (2, (int)(h * 0.005f)));
    }

private:
    static constexpr float splitRatio = 0.70f;

    OAOColors& colors;
    std::vector<std::unique_ptr<EffectsSlot>> slots;
    std::vector<std::unique_ptr<FXLFOSlot>>   lfoSlots;
};
