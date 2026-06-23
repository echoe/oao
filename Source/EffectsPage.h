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

        // Left block: tiny slot tag on top, effect selector underneath — sized to its own
        // content rather than the full slot height. Everything else sits to its right.
        int leftBlockW = juce::jmax (60, juce::roundToInt (getWidth() * 0.16f));
        int tagH       = juce::jmax (10, juce::roundToInt (getHeight() * 0.16f));
        int selectorH  = juce::jmax (16, juce::roundToInt (getHeight() * 0.26f));

        auto leftBlock = area.removeFromLeft (leftBlockW);
        slotLabel.setBounds      (leftBlock.removeFromTop (tagH));
        effectSelector.setBounds (leftBlock.removeFromTop (selectorH).reduced (2));

        area.removeFromLeft (juce::roundToInt (getWidth() * 0.01f)); // small gap before knobs

        // Shared text box / label sizing, derived from sharedKnobTarget (not this slot's
        // own getHeight(), which is only 1/6th of the page) so every page's knob row
        // — label position, label height, text box — matches exactly.
        int textBoxW = juce::roundToInt (sharedKnobTarget * ProjectConfig::textBoxWidthFraction);
        int textBoxH = juce::jlimit (12, 70, juce::roundToInt (sharedKnobTarget * colors.textBoxHeightFraction));
        int labelH   = juce::jmax (10, juce::roundToInt (sharedKnobTarget * colors.textBoxHeightFraction));

        // Shared knob diameter target, matching OperatorsPage/MatrixPage. The slider's
        // box must be diameter+8 wide (LookAndFeel reserves 4px each side) and that same
        // amount plus textBoxH tall (TextBoxBelow eats into the bottom of the box).
        // sharedKnobTarget comes from EffectsPage (the true page dimensions), not
        // computed from this slot's own getWidth()/getHeight(), which is only 1/6th
        // of the page height.
        int targetBoxSize = sharedKnobTarget + 8;
        auto clampKnob = [targetBoxSize, textBoxH] (juce::Rectangle<int> box)
        {
            int w = juce::jmin (box.getWidth(),  targetBoxSize);
            int h = juce::jmin (box.getHeight(), targetBoxSize + textBoxH);
            return box.withSizeKeepingCentre (w, h);
        };

        // Label sits ABOVE the knob, matching OperatorsPage's layout exactly.
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
    // Set from EffectsPage::resized(), which knows the true page dimensions — this
    // slot's own getWidth()/getHeight() is only 1/6th of the page height, far too
    // small to derive a sensible shared knob target from directly.
    void setSharedKnobTarget (int targetDiameter) { sharedKnobTarget = targetDiameter; }
private:
    int sharedKnobTarget = 90;
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
            // Must be set before setBounds, since setBounds synchronously triggers
            // the slot's own resized() which reads this value.
            slots[i]->setSharedKnobTarget (sharedKnobTarget);
            slots[i]->setBounds (fxArea.removeFromTop (isLast ? fxArea.getHeight() : slotH));
            if (! isLast) fxArea.removeFromTop (gapH);
	}

        // LFO sidebar — same fix applied identically, so both columns' bottom edges
        // land flush with the page's outer margin, matching the top edge exactly.
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
