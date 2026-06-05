#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Constants.h"
#include "OAOColors.h"

class EffectsSlot : public juce::Component
{
public:
    EffectsSlot (juce::AudioProcessorValueTreeState& apvts, int slotIndex, OAOColors& c)
	    : colors (c)
    {
        juce::String s = juce::String (slotIndex + 1);

        // Filter type selector. We grab the list from Constants.h
        filterSelector.addItemList (ProjectConfig::getFilterTypeChoices(), 1);
	filterSelector.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (filterSelector);
        // Sync button
        syncButton.setButtonText ("Sync");
        syncButton.setClickingTogglesState (true);
        addAndMakeVisible (syncButton);

        // Mix knob
        mixSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 15);
        addAndMakeVisible (mixSlider);
        mixLabel.setText ("Mix", juce::dontSendNotification);
        mixLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (mixLabel);

        // Four knobs
        for (int i = 0; i < 4; ++i)
        {
            knobs[i].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            knobs[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 15);
            addAndMakeVisible (knobs[i]);
            addAndMakeVisible (knobLabels[i]);
            knobLabels[i].setJustificationType (juce::Justification::centred);
        }

        // Slot label
        slotLabel.setText ("FX " + s, juce::dontSendNotification);
        slotLabel.setFont (juce::Font (juce::FontOptions (14.0f)));
        slotLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (slotLabel);

        // Attachments
	filterAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, "FX_TYPE_" + s, filterSelector);
        syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, "FX_SYNC_" + s, syncButton);
        mixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, "FX_MIX_" + s, mixSlider);
        
        const juce::StringArray knobIDs = { "FX_RATIO_", "FX_DETUNE_", "FX_PHASE_", "FX_FOLD_" };
        for (int i = 0; i < 4; ++i)
            knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, knobIDs[i] + s, knobs[i]);

        // Update labels when filter type changes
        filterSelector.onChange = [this] { updateLabels(); };
        updateLabels();
    }

    void updateLabels()
    {
        int typeIndex = filterSelector.getSelectedId() - 1; // ComboBox is 1-indexed
	auto labels = ProjectConfig::getFilterKnobLabels (typeIndex);
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

        // Update Labels
        slotLabel.setColour (juce::Label::textColourId, colors.text);
        mixLabel.setColour (juce::Label::textColourId, colors.text);
        
        for (int i = 0; i < 4; ++i)
            knobLabels[i].setColour (juce::Label::textColourId, colors.text);

        // Update ComboBox
        filterSelector.setColour (juce::ComboBox::backgroundColourId, colors.surface);
        filterSelector.setColour (juce::ComboBox::textColourId, colors.text);

        // Update Mix Slider
        mixSlider.setColour (juce::Slider::textBoxBackgroundColourId, colors.surface);
        mixSlider.setColour (juce::Slider::textBoxTextColourId, colors.text);

        // Update the 4 Parameter Knobs
        for (int i = 0; i < 4; ++i)
        {
            knobs[i].setColour (juce::Slider::textBoxBackgroundColourId, colors.surface);
            knobs[i].setColour (juce::Slider::textBoxTextColourId, colors.text);
            knobs[i].sendLookAndFeelChange();
        }

        // Nudge the others
        filterSelector.sendLookAndFeelChange();
        mixSlider.sendLookAndFeelChange();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (getWidth() * 0.02f);
    
        // Top row: slot label, filter selector, sync button
        auto topRow = area.removeFromTop (getHeight() * 0.18f);
        slotLabel.setBounds      (topRow.removeFromLeft (topRow.getWidth() * 0.08f));
        syncButton.setBounds     (topRow.removeFromRight (topRow.getWidth() * 0.12f).reduced (2));
        filterSelector.setBounds (topRow.reduced (2));
    
        area.removeFromTop (getHeight() * 0.02f);
    
        // Mix knob on the left
        auto mixArea = area.removeFromLeft (area.getWidth() * 0.12f);
        mixLabel.setBounds  (mixArea.removeFromBottom (getHeight() * 0.1f));
        mixSlider.setBounds (mixArea);
    
        area.removeFromLeft (area.getWidth() * 0.01f);
    
        // Four knobs share remaining space
        int knobW = area.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto knobArea = area.removeFromLeft (knobW);
            knobLabels[i].setBounds (knobArea.removeFromBottom (getHeight() * 0.1f));
            knobs[i].setBounds (knobArea);
        }
    }

private:
    void setKnobLabels (const juce::String& a, const juce::String& b,
                        const juce::String& c, const juce::String& d)
    {
        knobLabels[0].setText (a, juce::dontSendNotification);
        knobLabels[1].setText (b, juce::dontSendNotification);
        knobLabels[2].setText (c, juce::dontSendNotification);
        knobLabels[3].setText (d, juce::dontSendNotification);
    }
    OAOColors& colors;
    juce::Label    slotLabel;
    juce::ComboBox filterSelector;
    juce::TextButton syncButton;
    juce::Slider   mixSlider;
    juce::Label    mixLabel;
    juce::Slider   knobs[4];
    juce::Label    knobLabels[4];

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   syncAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   mixAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   knobAttachments[4];
};


class EffectsPage : public juce::Component
{
public:
    EffectsPage (juce::AudioProcessorValueTreeState& apvts, OAOColors& c)
	    : colors(c)
    {
        for (int i = 0; i < 3; ++i)
        {
            slots.push_back (std::make_unique<EffectsSlot> (apvts, i, colors));
            addAndMakeVisible (*slots.back());
        }
    }

    void paint (juce::Graphics& g) override
    {
        // Draw flow arrows between slots
        g.setColour (colors.text.withAlpha (0.3f));
        g.setFont (18.0f);
        int slotH = getHeight() / 3;
        for (int i = 0; i < 2; ++i)
        {
            int arrowY = slotH * (i + 1);
            g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x93")),
                        getWidth() / 2 - 10, arrowY - 12, 20, 20,
                        juce::Justification::centred);
        }
    }

    void lookAndFeelChanged() override
    {
        juce::Component::lookAndFeelChanged();

        for (auto& slot : slots)
        {
            if (slot != nullptr)
                slot->lookAndFeelChanged();
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (8);
        int slotH = (area.getHeight() - 20) / 3; // 20px for arrows

        for (int i = 0; i < 3; ++i)
        {
            slots[i]->setBounds (area.removeFromTop (slotH).reduced (0, 4));
            if (i < 2) area.removeFromTop (10); // space for arrow
        }
    }

private:
    OAOColors& colors;
    std::vector<std::unique_ptr<EffectsSlot>> slots;
};
