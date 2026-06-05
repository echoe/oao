#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PresetBar : public juce::Component
{
public:
    enum class RandomTarget
    {
        OperatorsAndEnvelopes,
        ModulationMatrix,
        RoutingMatrix
    };

    PresetBar (FMPluginAudioProcessor& processorToLink)
        : audioProcessor (processorToLink)
    {
        auto setupButton = [this] (juce::TextButton& btn, const juce::String& text) {
            btn.setButtonText (text);
            addAndMakeVisible (btn);
        };

        // Random and preset button labels and click functions
        setupButton (initButton,    "Init");
        setupButton (saveButton,    "Save");
        setupButton (loadButton,    "Load");
        setupButton (randOpsButton,   "R-Synth");
        setupButton (randModButton,   "R-FM");
        setupButton (randAudioButton, "R-Audio");

        initButton.onClick      = [this] { triggerInit(); };
        saveButton.onClick      = [this] { triggerSave(); };
        loadButton.onClick      = [this] { triggerLoad(); };
        randOpsButton.onClick   = [this] { triggerRandomizer (RandomTarget::OperatorsAndEnvelopes); };
        randModButton.onClick   = [this] { triggerRandomizer (RandomTarget::ModulationMatrix); };
        randAudioButton.onClick = [this] { triggerRandomizer (RandomTarget::RoutingMatrix); };

        // Preset navigation
        setupButton (prevButton, "<");
        setupButton (nextButton, ">");
        prevButton.onClick = [this] { navigatePreset (-1); };
        nextButton.onClick = [this] { navigatePreset (+1); };

        // Preset name label (centred, clickable to pick folder)
        presetNameLabel.setText ("(no preset)", juce::dontSendNotification);
        presetNameLabel.setJustificationType (juce::Justification::centred);
        presetNameLabel.setTooltip ("Current preset  —  click to choose preset folder");
        addAndMakeVisible (presetNameLabel);

        // Folder button
        setupButton (folderButton, "...");
        folderButton.setTooltip ("Choose preset folder");
        folderButton.onClick = [this] { choosePresetFolder(); };

        // Oversampling selector
        oversamplingSelector.addItem ("1x",  1);
        oversamplingSelector.addItem ("2x",  2);
        oversamplingSelector.addItem ("4x",  3);
        oversamplingSelector.addItem ("8x",  4);
        oversamplingSelector.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (oversamplingSelector);
        oversamplingSelector.onChange = [this]
        {
            int selectedId = oversamplingSelector.getSelectedId();
            int factor = 1 << (selectedId - 1); // 1, 2, 4, 8
            audioProcessor.setOversamplingFactor (factor);
        };
        oversamplingLabel.setText ("OS:", juce::dontSendNotification);
        oversamplingLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (oversamplingLabel);

        // Polyphony selector. 1 is, of course, mono.
        polyphonyLabel.setText ("Poly:", juce::dontSendNotification);
        polyphonyLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (polyphonyLabel);
  
        polyphonySelector.addItem ("1",  1);
        polyphonySelector.addItem ("2",  2);
        polyphonySelector.addItem ("4",  3);
        polyphonySelector.addItem ("8",  4);
        polyphonySelector.addItem ("16", 5);
        polyphonySelector.addItem ("32", 6);
        polyphonySelector.setSelectedId (4, juce::dontSendNotification); // default 8 voices
        addAndMakeVisible (polyphonySelector);
        polyphonySelector.onChange = [this]
        {
            int selectedId = polyphonySelector.getSelectedId();
            int numVoices  = polyphonySelector.getItemText (selectedId - 1).getIntValue();
            audioProcessor.setPolyphony (numVoices);
        };
    }

    void paint (juce::Graphics& g) override
    {
        g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (2);
        const int totalWidth = area.getWidth();
	const int dropdownWidth = totalWidth * 0.10f;
        const int smallBtnWidth   = totalWidth * 0.04f;
	const int smallLblWidth = totalWidth * 0.06f;
        const int medBtnWidth = totalWidth * 0.08f;  // Init, Save, Load, R-* buttons

        // Preset buttons
        initButton.setBounds      (area.removeFromLeft (medBtnWidth).reduced (1));
        saveButton.setBounds      (area.removeFromLeft (medBtnWidth).reduced (1));
        loadButton.setBounds      (area.removeFromLeft (medBtnWidth).reduced (1));
        // Centre area: < [... presetName] >
        prevButton.setBounds   (area.removeFromLeft  (smallBtnWidth).reduced (1));
        folderButton.setBounds (area.removeFromLeft  (smallBtnWidth).reduced (1));
        presetNameLabel.setBounds (area.removeFromLeft (dropdownWidth).reduced (1));
        nextButton.setBounds   (area.removeFromLeft (smallBtnWidth).reduced (1));
	// random buttons
        randOpsButton.setBounds   (area.removeFromLeft (medBtnWidth).reduced (1));
        randModButton.setBounds   (area.removeFromLeft (medBtnWidth).reduced (1));
        randAudioButton.setBounds (area.removeFromLeft (medBtnWidth).reduced (1));
        // oversampling and poly
        oversamplingLabel.setBounds    (area.removeFromLeft (smallLblWidth).reduced (1));
        oversamplingSelector.setBounds (area.removeFromLeft (dropdownWidth).reduced (1));
        polyphonyLabel.setBounds       (area.removeFromLeft (smallLblWidth).reduced (1));
        polyphonySelector.setBounds    (area.removeFromLeft (dropdownWidth).reduced (1));
    }

    void refreshScale()
    {
        sendLookAndFeelChange();
        repaint();
    }

private:
    // -------------------------------------------------------------------------
    // Preset folder / navigation helpers
    // -------------------------------------------------------------------------

    /** Rescans the current preset folder and sorts results alphabetically. */
    void refreshPresetList()
    {
        presetFiles.clear();
        if (presetFolder == juce::File() || ! presetFolder.isDirectory())
            return;

        presetFolder.findChildFiles (presetFiles, juce::File::findFiles, false, "*.xml");
        presetFiles.sort(); // juce::Array<File> sorts lexicographically
    }

    /** Updates the name label to reflect the current index (or a fallback). */
    void updateNameLabel()
    {
        if (presetFiles.isEmpty() || currentPresetIndex < 0)
        {
            presetNameLabel.setText ("(no preset)", juce::dontSendNotification);
            return;
        }
        // Show just the filename without the extension
        juce::String name = presetFiles[currentPresetIndex]
                                .getFileNameWithoutExtension();
        presetNameLabel.setText (name, juce::dontSendNotification);
    }

    /** Loads the preset at presetFiles[currentPresetIndex] into the APVTS. */
    void loadCurrentPreset()
    {
        if (currentPresetIndex < 0 || currentPresetIndex >= presetFiles.size())
            return;

        auto file = presetFiles[currentPresetIndex];
        if (file.existsAsFile())
        {
            std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (file));
            if (xml != nullptr && xml->hasTagName (audioProcessor.apvts.state.getType()))
                audioProcessor.apvts.replaceState (juce::ValueTree::fromXml (*xml));
        }
        updateNameLabel();
    }

    /** Steps through the preset list by +1 or -1, wrapping around. */
    void navigatePreset (int delta)
    {
        if (presetFiles.isEmpty())
            return;

        currentPresetIndex = (currentPresetIndex + delta + presetFiles.size())
                             % presetFiles.size();
        loadCurrentPreset();
    }

    /** Opens a folder chooser so the user can pick their preset directory. */
    void choosePresetFolder()
    {
        // Start in the folder we already have (or Documents as a fallback)
        juce::File startDir = (presetFolder != juce::File() && presetFolder.isDirectory())
                              ? presetFolder
                              : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

        fileChooser = std::make_unique<juce::FileChooser> (
            "Select Preset Folder", startDir, "*");

        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& chooser)
            {
                auto result = chooser.getResult();
                if (result != juce::File() && result.isDirectory())
                {
                    presetFolder = result;
                    refreshPresetList();
                    currentPresetIndex = presetFiles.isEmpty() ? -1 : 0;
                    updateNameLabel();
                }
            });
    }

    // -------------------------------------------------------------------------
    // Existing helpers
    // -------------------------------------------------------------------------

    void triggerInit()
    {
        auto& parameters = audioProcessor.apvts.processor.getParameters();
        for (auto* param : parameters)
            if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
                rangedParam->setValueNotifyingHost (rangedParam->getDefaultValue());

        currentPresetIndex = -1;
        updateNameLabel();
    }

    void triggerRandomizer (RandomTarget target)
    {
        auto& prng = juce::Random::getSystemRandom();
        auto& parameters = audioProcessor.apvts.processor.getParameters();
        for (auto* param : parameters)
        {
            if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
            {
                juce::String paramID = rangedParam->getParameterID();
                bool shouldRandomize = false;
                switch (target)
                {
                    case RandomTarget::OperatorsAndEnvelopes: 
                        if (paramID.startsWith ("MODE_") || paramID.startsWith ("WAVE_SHAPE") || paramID.startsWith ("FILTER_TYPE") || paramID.startsWith ("TEMPO_SYNC") || paramID.startsWith ("RATIO") || paramID.startsWith ("DETUNE") || paramID.startsWith ("PHASE") || paramID.startsWith ("FOLD") || paramID.startsWith ("ATTACK") || paramID.startsWith ("DECAY") || paramID.startsWith ("SUSTAIN") || paramID.startsWith ("RELEASE") || paramID.startsWith ("OUT"))
                            shouldRandomize = true;
                        break;
                    case RandomTarget::ModulationMatrix: 
                        if (paramID.startsWith ("MOD_"))
                            shouldRandomize = true;
                        break;
                    case RandomTarget::RoutingMatrix: 
                        if (paramID.startsWith ("AUDIO_ROUTE_"))
                            shouldRandomize = true;
                        break;
                }
                if (shouldRandomize)
                    rangedParam->setValueNotifyingHost (prng.nextFloat());
            }
        }
        // Randomised state is no longer tied to a named preset
        currentPresetIndex = -1;
        updateNameLabel();
    }

    void triggerSave()
    {
        // Default filename: the current preset name (if any), else "NewPreset"
        juce::String defaultName = (currentPresetIndex >= 0 && ! presetFiles.isEmpty())
                                   ? presetFiles[currentPresetIndex].getFileNameWithoutExtension()
                                   : "NewPreset";

        juce::File startDir = (presetFolder != juce::File() && presetFolder.isDirectory())
                              ? presetFolder
                              : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

        fileChooser = std::make_unique<juce::FileChooser> (
            "Save Synth Preset", startDir.getChildFile (defaultName), "*.xml");

        fileChooser->launchAsync (
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& chooser)
            {
                auto file = chooser.getResult();
                if (file == juce::File())
                    return;

                // Ensure .xml extension
                if (file.getFileExtension().toLowerCase() != ".xml")
                    file = file.withFileExtension ("xml");

                auto state = audioProcessor.apvts.copyState();
                std::unique_ptr<juce::XmlElement> xml (state.createXml());
                xml->writeTo (file);

                // Update the preset folder to wherever the user saved,
                // then refresh the list and point to the newly saved file.
                presetFolder = file.getParentDirectory();
                refreshPresetList();

                // Find the saved file in the refreshed list
                currentPresetIndex = presetFiles.indexOf (file);
                updateNameLabel();
            });
    }

    void triggerLoad()
    {
        juce::File startDir = (presetFolder != juce::File() && presetFolder.isDirectory())
                              ? presetFolder
                              : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

        fileChooser = std::make_unique<juce::FileChooser> (
            "Load Synth Preset", startDir, "*.xml");

        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& chooser)
            {
                auto file = chooser.getResult();
                if (! file.existsAsFile())
                    return;

                std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (file));
                if (xml != nullptr && xml->hasTagName (audioProcessor.apvts.state.getType()))
                    audioProcessor.apvts.replaceState (juce::ValueTree::fromXml (*xml));

                // Switch the active folder to wherever this file lives,
                // refresh the list, and point the index at this file.
                presetFolder = file.getParentDirectory();
                refreshPresetList();
                currentPresetIndex = presetFiles.indexOf (file);
                updateNameLabel();
            });
    }

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------
    FMPluginAudioProcessor& audioProcessor;

    juce::TextButton initButton, saveButton, loadButton;
    juce::TextButton randOpsButton, randModButton, randAudioButton;
    juce::TextButton prevButton, nextButton, folderButton;
    juce::Label      presetNameLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::ComboBox oversamplingSelector;
    juce::Label    oversamplingLabel;
    juce::ComboBox polyphonySelector;
    juce::Label    polyphonyLabel;

    // Preset folder state
    juce::File              presetFolder;
    juce::Array<juce::File> presetFiles;
    int                     currentPresetIndex { -1 };
};
