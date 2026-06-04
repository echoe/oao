#pragma once
#include <JuceHeader.h>
#include "OAOColors.h"
#include "OAOLookAndFeel.h"

class SettingsPage : public juce::Component, public juce::ChangeListener
{
public:
    // Called when colors change so the editor can refresh
    std::function<void()> onColorsChanged;

    SettingsPage (OAOColors& c, OAOLookAndFeel& laf) 
        : colors (c), lookAndFeel (laf)
    {
        // Preset buttons
        auto setupPreset = [this] (juce::TextButton& btn, const juce::String& name)
        {
            btn.setButtonText (name);
            addAndMakeVisible (btn);
        };
        setupPreset (synthwaveBtn,  "Synthwave");
        setupPreset (industrialBtn, "Industrial");
        setupPreset (minimalBtn,    "Minimal");
        setupPreset (warmBtn,       "Warm Analog");

        synthwaveBtn.onClick  = [this] { colors.setSynthwave();  refreshAll(); };
        industrialBtn.onClick = [this] { colors.setIndustrial(); refreshAll(); };
        minimalBtn.onClick    = [this] { colors.setMinimal();    refreshAll(); };
        warmBtn.onClick       = [this] { colors.setWarmAnalog(); refreshAll(); };

        // HSB sliders for each color
        setupColorSection (backgroundSection, "Background",  colors.background);
        setupColorSection (primarySection,    "Primary",     colors.primary);
        setupColorSection (secondarySection,  "Secondary",   colors.secondary);
        setupColorSection (surfaceSection,    "Surface",     colors.surface);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colors.background);

        g.setColour (colors.text);
        g.setFont (juce::Font (juce::FontOptions (18.0f)));
        g.drawText ("Settings", getLocalBounds().removeFromTop (40),
                    juce::Justification::centred);

        g.setColour (colors.primary.withAlpha (0.3f));
        g.drawHorizontalLine (40, 8.0f, static_cast<float> (getWidth()) - 8.0f);

        g.setColour (colors.textDim);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText ("Presets", 8, 48, 80, 20, juce::Justification::centredLeft);
        g.drawText ("Colors",  8, 100, 80, 20, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (8);
        area.removeFromTop (48);

        // Preset buttons row
        auto presetRow  = area.removeFromTop (30);
        int  btnW       = presetRow.getWidth() / 4;
        synthwaveBtn.setBounds  (presetRow.removeFromLeft (btnW).reduced (2));
        industrialBtn.setBounds (presetRow.removeFromLeft (btnW).reduced (2));
        minimalBtn.setBounds    (presetRow.removeFromLeft (btnW).reduced (2));
        warmBtn.setBounds       (presetRow.reduced (2));

        area.removeFromTop (16);

        // Color sections
        int sectionH = (area.getHeight()) / 4;
        layoutSection (backgroundSection, area.removeFromTop (sectionH));
        layoutSection (primarySection,    area.removeFromTop (sectionH));
        layoutSection (secondarySection,  area.removeFromTop (sectionH));
        layoutSection (surfaceSection,    area.removeFromTop (sectionH));
    }

private:
    struct ColorSection
    {
        juce::Label      nameLabel;
        juce::Label      previewBox;
        juce::TextButton editButton; // <-- Replaces the 3 sliders and labels
        juce::Colour* targetColor = nullptr;
    };
    void setupColorSection (ColorSection& section, const juce::String& name,
                            juce::Colour& targetColor)
    {
        section.targetColor = &targetColor;
    
        section.nameLabel.setText (name, juce::dontSendNotification);
        addAndMakeVisible (section.nameLabel);
    
        addAndMakeVisible (section.previewBox);
        section.previewBox.setColour (juce::Label::backgroundColourId, targetColor);
    
        // Setup the new Edit Button
        section.editButton.setButtonText ("Edit");
        addAndMakeVisible (section.editButton);
    
        section.editButton.onClick = [this, &section]()
        {
            // Create the color selector
            auto* colorSelector = new juce::ColourSelector();
            colorSelector->setName (section.nameLabel.getText());
            colorSelector->setCurrentColour (*section.targetColor);
            colorSelector->setSize (300, 300);
    
            // Tell the color picker to send updates to SettingsPage
            colorSelector->addChangeListener (this);
    
            // Launch it in a popup box attached to the edit button!
            juce::CallOutBox::launchAsynchronously (std::unique_ptr<juce::Component>(colorSelector),
                                                    section.editButton.getScreenBounds(),
                                                    nullptr);
        };
    }

    void layoutSection (ColorSection& section, juce::Rectangle<int> area)
    {
        area.reduce (4, 2);
        section.nameLabel.setBounds (area.removeFromLeft (60));
        section.previewBox.setBounds (area.removeFromLeft (60).reduced (2));
        area.removeFromLeft (8);

        section.editButton.setBounds (area.removeFromLeft (60).reduced(2));
    }

    void refreshAll()
    {
        // Update preview boxes to reflect current slider values
        backgroundSection.previewBox.setColour (juce::Label::backgroundColourId, 
                                                 colors.background);
        primarySection.previewBox.setColour    (juce::Label::backgroundColourId, 
                                                 colors.primary);
        secondarySection.previewBox.setColour  (juce::Label::backgroundColourId, 
                                                 colors.secondary);
        surfaceSection.previewBox.setColour    (juce::Label::backgroundColourId, 
                                                 colors.surface);

        lookAndFeel.applyColors();
        colors.saveToFile();

        if (onColorsChanged)
            onColorsChanged();
    }

    void changeListenerCallback (juce::ChangeBroadcaster* source) override
    {
        // If the change came from a ColourSelector...
        if (auto* cs = dynamic_cast<juce::ColourSelector*> (source))
        {
            juce::Colour newColor = cs->getCurrentColour();
            juce::String pickerName = cs->getName();

            // Figure out which color we are editing based on the name we gave it
            if (pickerName == "Background") colors.background = newColor;
            if (pickerName == "Primary")    colors.primary = newColor;
            if (pickerName == "Secondary")  colors.secondary = newColor;
            if (pickerName == "Surface")    colors.surface = newColor;

            refreshAll(); // Saves to file, triggers LookAndFeel, repaints
        }
    }

    OAOColors&        colors;
    OAOLookAndFeel&   lookAndFeel;

    juce::TextButton  synthwaveBtn, industrialBtn, minimalBtn, warmBtn;

    ColorSection backgroundSection;
    ColorSection primarySection;
    ColorSection secondarySection;
    ColorSection surfaceSection;
};
