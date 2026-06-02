#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class EffectsSlot : public juce::Component
{
public:
    EffectsSlot (juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    {
        juce::String s = juce::String (slotIndex + 1);

        // Filter type selector
        filterSelector.addItem ("None",               1);
        filterSelector.addItem ("Lowpass",            2);
        filterSelector.addItem ("Highpass",           3);
        filterSelector.addItem ("Bandpass",           4);
        filterSelector.addItem ("Comb",               5);
        filterSelector.addItem ("Granular",           6);
        filterSelector.addItem ("Tape",               7);
        filterSelector.addItem ("Distortion",         8);
        filterSelector.addItem ("Spectral Resonator", 9);
        filterSelector.addItem ("Chorus",             10);
        filterSelector.addItem ("Allpass Delay",      11);
        filterSelector.addItem ("Allpass Reverb",     12);
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

        const juce::StringArray knobIDs = { "FX_A_", "FX_B_", "FX_C_", "FX_D_" };
        for (int i = 0; i < 4; ++i)
            knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, knobIDs[i] + s, knobs[i]);

        // Update labels when filter type changes
        filterSelector.onChange = [this] { updateLabels(); };
        updateLabels();
    }

    void updateLabels()
    {
        int selectedId = filterSelector.getSelectedId();
        switch (selectedId)
        {
            case 1:  setKnobLabels ("--",      "--",       "--",      "--");      break; // None
            case 2:  setKnobLabels ("Cutoff",  "Resonance","Keytrack","Drive");   break; // LP
            case 3:  setKnobLabels ("Cutoff",  "Resonance","Keytrack","Drive");   break; // HP
            case 4:  setKnobLabels ("Cutoff",  "Resonance","Keytrack","Drive");   break; // BP
            case 5:  setKnobLabels ("Cutoff",  "Feedback", "Keytrack","Damping"); break; // Comb
            case 6:  setKnobLabels ("Size",    "Damping",  "Scatter", "Feedback");break; // Granular
            case 7:  setKnobLabels ("Wobble",  "Age",      "Saturation","Speed"); break; // Tape
            case 8:  setKnobLabels ("Drive",   "Tone",     "Mix",     "Flavor");  break; // Distortion
            case 9:  setKnobLabels ("Freq",    "Resonance","Spread",  "Decay");   break; // Spectral
            case 10: setKnobLabels ("Rate",    "Depth",    "Spread",  "Feedback");break; // Chorus
            case 11: setKnobLabels ("Time",    "Feedback", "Spread",  "Damping"); break; // AP Delay
            case 12: setKnobLabels ("Size",    "Diffusion","Decay",   "Damping"); break; // AP Reverb
            default: setKnobLabels ("A",       "B",        "C",       "D");       break;
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::darkgrey.darker (0.2f));
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.drawRect (getLocalBounds(), 1);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (8);

        // Top row: slot label, filter selector, sync button
        auto topRow = area.removeFromTop (30);
        slotLabel.setBounds       (topRow.removeFromLeft (40));
        syncButton.setBounds      (topRow.removeFromRight (50).reduced (2));
        filterSelector.setBounds  (topRow.reduced (2));

        area.removeFromTop (8);

        // Mix knob on the left
        auto mixArea = area.removeFromLeft (70);
        mixLabel.setBounds (mixArea.removeFromBottom (18));
        mixSlider.setBounds (mixArea);

        area.removeFromLeft (8);

        // Four knobs share remaining space
        int knobW = area.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto knobArea = area.removeFromLeft (knobW);
            knobLabels[i].setBounds (knobArea.removeFromBottom (18));
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
    EffectsPage (juce::AudioProcessorValueTreeState& apvts)
    {
        for (int i = 0; i < 3; ++i)
        {
            slots.push_back (std::make_unique<EffectsSlot> (apvts, i));
            addAndMakeVisible (*slots.back());
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::darkgrey.darker (0.3f));

        // Draw flow arrows between slots
        g.setColour (juce::Colours::white.withAlpha (0.3f));
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
    std::vector<std::unique_ptr<EffectsSlot>> slots;
};
