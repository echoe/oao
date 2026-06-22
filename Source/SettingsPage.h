//SettingsPage.h
#pragma once
#include <JuceHeader.h>
#include "Constants.h"
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
        setupColorSection (panelGapSection, "Panel Gap", colors.panelGap);

        // Theme preset save/load — shareable .oaotheme XML files
        setupPreset (saveThemeBtn, "Save Theme");
        setupPreset (loadThemeBtn, "Load Theme");
        saveThemeBtn.onClick = [this] { saveThemePreset(); };
        loadThemeBtn.onClick = [this] { loadThemePreset(); };
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colors.panelGap);

        // Card behind all the actual controls — same uniform outer margin on all
        // four sides as every other page, no extra top-only deduction
        auto cardArea = getLocalBounds().reduced (
            juce::roundToInt (getWidth()  * ProjectConfig::outerMargin),
            juce::roundToInt (getHeight() * ProjectConfig::outerMargin));
        auto cardBounds = cardArea.toFloat();

        g.setColour (colors.background);
        g.fillRoundedRectangle (cardBounds, 4.0f);
        g.setColour (colors.text.withAlpha (0.15f));
        g.drawRoundedRectangle (cardBounds.reduced (1.0f), 4.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (
            juce::roundToInt (getWidth()  * ProjectConfig::outerMargin),
            juce::roundToInt (getHeight() * ProjectConfig::outerMargin));
    
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
    
        area.removeFromTop (getHeight() * 0.01f); // small gap

        // Save/Load theme preset buttons
        auto themeRow = area.removeFromTop (getHeight() * 0.07f);
        int  themeBtnW = themeRow.getWidth() / 2;
        saveThemeBtn.setBounds (themeRow.removeFromLeft (themeBtnW).reduced (2));
        loadThemeBtn.setBounds (themeRow.reduced (2));

        area.removeFromTop (getHeight() * 0.03f); // gap
    
        // Color pickers
        auto colorRow = area; // takes all remaining space
        int  sectionW = colorRow.getWidth() / 6;
    
        layoutSection (backgroundSection, colorRow.removeFromLeft (sectionW));
        layoutSection (primarySection,    colorRow.removeFromLeft (sectionW));
        layoutSection (secondarySection,  colorRow.removeFromLeft (sectionW));
        layoutSection (surfaceSection,    colorRow.removeFromLeft (sectionW));
        layoutSection (textSection,       colorRow.removeFromLeft (sectionW));
        layoutSection (panelGapSection,   colorRow);
    }

    void refreshAll() //refreshes colors
    {
        backgroundSection.previewBox.color = colors.background;
        primarySection.previewBox.color    = colors.primary;
        secondarySection.previewBox.color  = colors.secondary;
        surfaceSection.previewBox.color    = colors.surface;
        textSection.previewBox.color       = colors.text;
        panelGapSection.previewBox.color   = colors.panelGap;

        backgroundSection.nameLabel.setColour (juce::Label::textColourId, colors.text);
        primarySection.nameLabel.setColour    (juce::Label::textColourId, colors.text);
        secondarySection.nameLabel.setColour  (juce::Label::textColourId, colors.text);
        surfaceSection.nameLabel.setColour    (juce::Label::textColourId, colors.text);
        textSection.nameLabel.setColour       (juce::Label::textColourId, colors.text);
        panelGapSection.nameLabel.setColour   (juce::Label::textColourId, colors.text);

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

    void saveThemePreset()
    {
        juce::File startDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

        fileChooser = std::make_unique<juce::FileChooser> (
            "Save Theme Preset", startDir.getChildFile ("My Theme"), "*.oaotheme");

        fileChooser->launchAsync (
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& chooser)
            {
                auto file = chooser.getResult();
                if (file == juce::File())
                    return;

                if (file.getFileExtension().toLowerCase() != ".oaotheme")
                    file = file.withFileExtension ("oaotheme");

                juce::XmlElement xml ("OAOThemePreset");
                xml.setAttribute ("background", colors.background.toString());
                xml.setAttribute ("primary",     colors.primary.toString());
                xml.setAttribute ("secondary",   colors.secondary.toString());
                xml.setAttribute ("surface",     colors.surface.toString());
                xml.setAttribute ("text",        colors.text.toString());
                xml.setAttribute ("textDim",     colors.textDim.toString());
                xml.setAttribute ("panelGap",    colors.panelGap.toString());
                xml.setAttribute ("scale",       colors.scale);
                xml.setAttribute ("fontName",    lookAndFeel.currentFontName);

                xml.writeTo (file);
            });
    }

    void loadThemePreset()
    {
        juce::File startDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

        fileChooser = std::make_unique<juce::FileChooser> (
            "Load Theme Preset", startDir, "*.oaotheme");

        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& chooser)
            {
                auto file = chooser.getResult();
                if (! file.existsAsFile())
                    return;

                std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (file));
                if (xml == nullptr || ! xml->hasTagName ("OAOThemePreset"))
                    return;

                // Colors fall back to their current value if a field is missing,
                // so a hand-edited or partial file degrades gracefully rather than zeroing out.
                auto loadColour = [&] (const char* attr, juce::Colour fallback)
                {
                    return xml->hasAttribute (attr)
                               ? juce::Colour::fromString (xml->getStringAttribute (attr))
                               : fallback;
                };

                colors.background = loadColour ("background", colors.background);
                colors.primary    = loadColour ("primary",    colors.primary);
                colors.secondary  = loadColour ("secondary",  colors.secondary);
                colors.surface    = loadColour ("surface",    colors.surface);
                colors.text       = loadColour ("text",       colors.text);
                colors.textDim    = loadColour ("textDim",    colors.textDim);
                colors.panelGap   = loadColour ("panelGap",   colors.panelGap);

                float loadedScale = (float) xml->getDoubleAttribute ("scale", colors.scale);
                juce::String loadedFont = xml->getStringAttribute ("fontName", lookAndFeel.currentFontName);

                lookAndFeel.currentFontName = loadedFont;

                // Reflect the loaded font in the selector (falls back to "Default" if not found)
                int fontItemId = 1; // Default
                for (int i = 0; i < fontSelector.getNumItems(); ++i)
                {
                    if (fontSelector.getItemText (i) == loadedFont)
                    {
                        fontItemId = fontSelector.getItemId (i);
                        break;
                    }
                }
                fontSelector.setSelectedId (fontItemId, juce::dontSendNotification);

                // Snap the scale selector to the nearest valid 25%-step option and notify
                // the editor so the whole window actually resizes to match.
                int nearestPercent = juce::jlimit (50, 200, juce::roundToInt (loadedScale * 100.0f / 25.0f) * 25);
                int scaleItemId    = ((nearestPercent - 50) / 25) + 1;
                scaleSelector.setSelectedId (scaleItemId, juce::dontSendNotification);
                colors.scale = nearestPercent / 100.0f;
                if (onScaleChanged)
                    onScaleChanged (colors.scale);

                refreshAll();
            });
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
            if (pickerName == "Panel Gap")  colors.panelGap = newColor;

            refreshAll(); 
        }
    }

    OAOColors&        colors;
    OAOLookAndFeel&   lookAndFeel;

    juce::ComboBox    scaleSelector, fontSelector, oversamplingSelector, polyphonySelector;
    juce::Label       scaleLabel, fontLabel, oversamplingLabel, polyphonyLabel;

    juce::TextButton  synthwaveBtn, industrialBtn, minimalBtn, warmBtn, mintBtn, peachBtn, lavenderBtn, nordicBtn;
    juce::TextButton  saveThemeBtn, loadThemeBtn;
    std::unique_ptr<juce::FileChooser> fileChooser;

    ColorSection backgroundSection;
    ColorSection primarySection;
    ColorSection secondarySection;
    ColorSection surfaceSection;
    ColorSection textSection;
    ColorSection panelGapSection;
};
