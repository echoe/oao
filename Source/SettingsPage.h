#pragma once
#include <JuceHeader.h>
#include "OAOColors.h"
#include "OAOLookAndFeel.h"

struct ColorPreviewButton : public juce::Component
{
    std::function<void()> onClick;
    juce::Colour color;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (color);
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.drawRect (getLocalBounds(), 1);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (onClick) onClick();
    }
};

class SettingsPage : public juce::Component, public juce::ChangeListener
{
public:
    std::function<void()> onColorsChanged;
    
    // Added the callback for the main editor
    std::function<void(float)> onScaleChanged;

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
        setupPreset (mintBtn,     "Minty");
        setupPreset (peachBtn,    "Peach");
        setupPreset (lavenderBtn, "Lavender");
        setupPreset (nordicBtn,   "Nordic");

        synthwaveBtn.onClick  = [this] { colors.setSynthwave();  refreshAll(); };
        industrialBtn.onClick = [this] { colors.setIndustrial(); refreshAll(); };
        minimalBtn.onClick    = [this] { colors.setMinimal();    refreshAll(); };
        warmBtn.onClick       = [this] { colors.setWarmAnalog(); refreshAll(); };
        mintBtn.onClick     = [this] { colors.setMintyBreeze();  refreshAll(); };
        peachBtn.onClick    = [this] { colors.setPeachBlossom(); refreshAll(); };
        lavenderBtn.onClick = [this] { colors.setLavenderHaze(); refreshAll(); };
        nordicBtn.onClick   = [this] { colors.setNordicMorning(); refreshAll(); };

        // Scale selector (50% to 200% in 25% steps)
        scaleLabel.setText ("UI Scale:", juce::dontSendNotification);
        scaleLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (scaleLabel);
        
        int id = 1;
        for (int percent = 50; percent <= 200; percent += 25)
        {
            scaleSelector.addItem (juce::String (percent) + "%", id++);
            if (percent == 100)
                scaleSelector.setSelectedId (id - 1, juce::dontSendNotification);
        }
        addAndMakeVisible (scaleSelector);
        
        scaleSelector.onChange = [this]
        {
            int selectedId  = scaleSelector.getSelectedId();
            float percent   = 50.0f + (selectedId - 1) * 25.0f; 
            float scale     = percent / 100.0f;
            if (onScaleChanged)
                onScaleChanged (scale);
        };

        fontLabel.setText ("Font:", juce::dontSendNotification);
        fontLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (fontLabel);

        fontSelector.addItem ("Default", 1);
        juce::StringArray systemFonts = juce::Font::findAllTypefaceNames();
        
        int fontId = 2;
        for (const auto& fontName : systemFonts)
            fontSelector.addItem (fontName, fontId++);
            
        fontSelector.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (fontSelector);

        fontSelector.onChange = [this]
        {
            lookAndFeel.currentFontName = fontSelector.getText();
            refreshAll(); 
            // Note: Since this changes fonts globally, you might also need 
            // to trigger a repaint on your parent editor component if 
            // refreshAll() doesn't catch components outside the settings page.
        };

        // HSB sliders for each color
        setupColorSection (backgroundSection, "Background",  colors.background);
        setupColorSection (primarySection,    "Primary",     colors.primary);
        setupColorSection (secondarySection,  "Secondary",   colors.secondary);
        setupColorSection (surfaceSection,    "Surface",     colors.surface);
        setupColorSection (textSection, "Text", colors.text);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colors.background);
        g.setColour (colors.text);
        g.setFont (lookAndFeel.getCustomFont (getHeight() * 0.05f));
        g.drawText ("Settings", getLocalBounds().removeFromTop (getHeight() * 0.05f),
                    juce::Justification::centred);

        g.setColour (colors.primary.withAlpha (0.3f));
        g.drawHorizontalLine (getHeight()*0.06f, 8.0f, static_cast<float> (getWidth()) - 8.0f); // line under settings title
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (getWidth() * 0.02f);
        area.removeFromTop (getHeight() * 0.08f); // title area
    
        // UI Scale and Font Selection
        auto scaleRow = area.removeFromTop (getHeight() * 0.05f);
        scaleLabel.setBounds    (scaleRow.removeFromLeft (scaleRow.getWidth() * 0.15f));
        scaleSelector.setBounds (scaleRow.removeFromLeft (scaleRow.getWidth() * 0.25f).reduced (2));
        fontLabel.setBounds    (scaleRow.removeFromLeft (scaleRow.getWidth() * 0.2f));
        fontSelector.setBounds (scaleRow.reduced (2));
        area.removeFromTop (getHeight() * 0.03f); // gap
    
        // Buttons
        auto presetRow1 = area.removeFromTop (getHeight() * 0.07f);
        int  btnW1      = presetRow1.getWidth() / 4;
        synthwaveBtn.setBounds  (presetRow1.removeFromLeft (btnW1).reduced (2));
        industrialBtn.setBounds (presetRow1.removeFromLeft (btnW1).reduced (2));
        minimalBtn.setBounds    (presetRow1.removeFromLeft (btnW1).reduced (2));
        warmBtn.setBounds       (presetRow1.reduced (2));
    
        area.removeFromTop (getHeight() * 0.01f); // small gap
    
        auto presetRow2 = area.removeFromTop (getHeight() * 0.07f);
        int  btnW2      = presetRow2.getWidth() / 4;
        mintBtn.setBounds     (presetRow2.removeFromLeft (btnW2).reduced (2));
        peachBtn.setBounds    (presetRow2.removeFromLeft (btnW2).reduced (2));
        lavenderBtn.setBounds (presetRow2.removeFromLeft (btnW2).reduced (2));
        nordicBtn.setBounds   (presetRow2.reduced (2));
    
        area.removeFromTop (getHeight() * 0.03f); // gap
    
        // Color pickers
        auto colorRow = area; // takes all remaining space
        int  sectionW = colorRow.getWidth() / 5;
    
        layoutSection (backgroundSection, colorRow.removeFromLeft (sectionW));
        layoutSection (primarySection,    colorRow.removeFromLeft (sectionW));
        layoutSection (secondarySection,  colorRow.removeFromLeft (sectionW));
        layoutSection (surfaceSection,    colorRow.removeFromLeft (sectionW));
        layoutSection (textSection,       colorRow);
    }

    void refreshAll() //refreshes colors
    {
        backgroundSection.previewBox.color = colors.background;
        primarySection.previewBox.color    = colors.primary;
        secondarySection.previewBox.color  = colors.secondary;
        surfaceSection.previewBox.color    = colors.surface;
        textSection.previewBox.color       = colors.text;

        backgroundSection.nameLabel.setColour (juce::Label::textColourId, colors.text);
        primarySection.nameLabel.setColour    (juce::Label::textColourId, colors.text);
        secondarySection.nameLabel.setColour  (juce::Label::textColourId, colors.text);
        surfaceSection.nameLabel.setColour    (juce::Label::textColourId, colors.text);
        textSection.nameLabel.setColour       (juce::Label::textColourId, colors.text);

        scaleLabel.setColour (juce::Label::textColourId, colors.text);
        scaleSelector.setColour (juce::ComboBox::backgroundColourId, colors.surface);
        scaleSelector.setColour (juce::ComboBox::textColourId, colors.text);
        scaleSelector.setColour (juce::ComboBox::arrowColourId, colors.text);
        scaleSelector.sendLookAndFeelChange();

        fontLabel.setColour (juce::Label::textColourId, colors.text);
        fontSelector.setColour (juce::ComboBox::backgroundColourId, colors.surface);
        fontSelector.setColour (juce::ComboBox::textColourId, colors.text);
        fontSelector.setColour (juce::ComboBox::arrowColourId, colors.text);
        fontSelector.sendLookAndFeelChange();

	// general refreshes
        lookAndFeel.applyColors();
        colors.saveToFile();
        repaint();
        if (onColorsChanged)
            onColorsChanged();
    }

private:
    struct ColorSection
    {
        juce::Label      nameLabel;
        ColorPreviewButton previewBox;
        juce::Colour* targetColor = nullptr;
    };

    void setupColorSection (ColorSection& section, const juce::String& name,
                            juce::Colour& targetColor)
    {
        section.targetColor = &targetColor;
    
        section.nameLabel.setText (name, juce::dontSendNotification);
        section.nameLabel.setJustificationType (juce::Justification::centred); // Center text
        addAndMakeVisible (section.nameLabel);
    
        section.previewBox.setColour (juce::TextButton::buttonColourId, targetColor);
        section.previewBox.onClick = [this, &section]()
        {
            auto* colorSelector = new juce::ColourSelector();
            colorSelector->setName (section.nameLabel.getText());
            colorSelector->setCurrentColour (*section.targetColor);
            colorSelector->setSize (300, 300);
    
            colorSelector->addChangeListener (this);
    
            juce::CallOutBox::launchAsynchronously (std::unique_ptr<juce::Component>(colorSelector),
                                                    section.previewBox.getScreenBounds(),
                                                    nullptr);
        };
        addAndMakeVisible (section.previewBox);
    }

    void layoutSection (ColorSection& section, juce::Rectangle<int> area) // Organizes the colors
    {
        area.reduce (4, 0);
        int labelH = area.getHeight() * 0.1f; // labels
        int previewH = area.getHeight() * 0.5f; //colors
    
        section.nameLabel.setBounds (area.removeFromTop (labelH));
        area.removeFromTop (4);
        section.previewBox.setBounds (area.removeFromTop (previewH));
    }

    void changeListenerCallback (juce::ChangeBroadcaster* source) override
    {
        if (auto* cs = dynamic_cast<juce::ColourSelector*> (source))
        {
            juce::Colour newColor = cs->getCurrentColour();
            juce::String pickerName = cs->getName();

            if (pickerName == "Background") colors.background = newColor;
            if (pickerName == "Primary")    colors.primary = newColor;
            if (pickerName == "Secondary")  colors.secondary = newColor;
            if (pickerName == "Surface")    colors.surface = newColor;
            if (pickerName == "Text")       colors.text = newColor;

            refreshAll(); 
        }
    }

    OAOColors&        colors;
    OAOLookAndFeel&   lookAndFeel;

    juce::ComboBox    scaleSelector, fontSelector, oversamplingSelector, polyphonySelector;
    juce::Label       scaleLabel, fontLabel, oversamplingLabel, polyphonyLabel;

    juce::TextButton  synthwaveBtn, industrialBtn, minimalBtn, warmBtn, mintBtn, peachBtn, lavenderBtn, nordicBtn;

    ColorSection backgroundSection;
    ColorSection primarySection;
    ColorSection secondarySection;
    ColorSection surfaceSection;
    ColorSection textSection;
};
