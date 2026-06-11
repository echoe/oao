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
        setupButton (smartRandButton,   "Rand");
	setupButton (drumRandButton,   "D");
	setupButton (effectsRandButton,   "E");

        initButton.onClick      = [this] { triggerInit(); };
        saveButton.onClick      = [this] { triggerSave(); };
        loadButton.onClick      = [this] { triggerLoad(); };
        smartRandButton.onClick   = [this] { triggerSmartRandomizer (); };
        drumRandButton.onClick   = [this] { triggerDrumRandomizer (); };
	effectsRandButton.onClick   = [this] { triggerEffectsRandomizer (); };

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
        smartRandButton.setBounds   (area.removeFromLeft (medBtnWidth).reduced (1));
        drumRandButton.setBounds   (area.removeFromLeft (smallBtnWidth).reduced (1));
        effectsRandButton.setBounds   (area.removeFromLeft (smallBtnWidth).reduced (1));
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
    // Preset folder / navigation helper functions

    // Rescans the current preset folder and sorts results alphabetically.
    void refreshPresetList()
    {
        presetFiles.clear();
        if (presetFolder == juce::File() || ! presetFolder.isDirectory())
            return;

        presetFolder.findChildFiles (presetFiles, juce::File::findFiles, false, "*.xml");
        presetFiles.sort(); // juce::Array<File> sorts lexicographically
    }

    // Updates the name label to reflect the current index (or a fallback).
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

    // Loads the preset at presetFiles[currentPresetIndex] into the APVTS.
    void loadCurrentPreset()
    {
        if (currentPresetIndex < 0 || currentPresetIndex >= presetFiles.size())
            return;

        auto file = presetFiles[currentPresetIndex];
        if (! file.existsAsFile())
            return;

        // Parse the XML, re-wrap as binary, then call setStateInformation
        // so sample data embedded in the file is also restored.
        std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (file));
        if (xml != nullptr)
        {
            juce::MemoryBlock stateData;
            audioProcessor.copyXmlToBinary (*xml, stateData);
            audioProcessor.setStateInformation (stateData.getData(), (int) stateData.getSize());
        }
        updateNameLabel();
    }

    // Steps through the preset list by +1 or -1, wrapping around.
    void navigatePreset (int delta)
    {
        if (presetFiles.isEmpty())
            return;

        currentPresetIndex = (currentPresetIndex + delta + presetFiles.size())
                             % presetFiles.size();
        loadCurrentPreset();
    }

    // Opens a folder chooser so the user can pick their preset directory.
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

    // Preset Button Functions
    void triggerInit()
    {
        auto& parameters = audioProcessor.apvts.processor.getParameters();
        for (auto* param : parameters)
            if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
                rangedParam->setValueNotifyingHost (rangedParam->getDefaultValue());

        currentPresetIndex = -1;
        updateNameLabel();
    }

    void triggerSmartRandomizer()
    {
        auto& prng = juce::Random::getSystemRandom();
        auto& parameters = audioProcessor.apvts.processor.getParameters();
        
        for (auto* param : parameters)
        {
            if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
            {
                juce::String paramID = rangedParam->getParameterID();
                float newValue = prng.nextFloat(); // Default fallback
                bool shouldRandomize = true;
    
                // ENVELOPES (Keep them playable, avoid infinite silence)
                if (paramID.startsWith("ATTACK"))
                {
                    // Favor faster attacks (0.0 to 0.3) so notes actually trigger instantly
                    newValue = prng.nextFloat() * 0.3f; 
                }
                else if (paramID.startsWith("DECAY") || paramID.startsWith("RELEASE"))
                {
                    // Avoid absolute zero or eternity; give it a moderate-to-snappy feel
                    newValue = 0.05f + (prng.nextFloat() * 0.6f); 
                }
                else if (paramID.startsWith("SUSTAIN"))
                {
                    // Sustains can be anything, but favor audible levels (0.2 to 1.0)
                    newValue = 0.2f + (prng.nextFloat() * 0.8f);
                }
                
                // OUTPUTS & VOLUMES (Prevent speaker-blowing or dead silence)
                else if (paramID.startsWith("OUT") || paramID.contains("VOLUME"))
                {
		    // for outputs, let's randomly only do half of them
		    if (prng.nextFloat() > 0.5f) {
                        newValue = 0.0f;
                    } else {
                    // when we do output, keep output volumes in a safe area (e.g., 50% to 85%)
                        newValue = 0.5f + (prng.nextFloat() * 0.35f);
                    }
                }
                
                // ROUTING & MODULATION (Full chaos here is usually messy)
                else if (paramID.startsWith("AUDIO_ROUTE_") || paramID.startsWith("MOD_"))
                {
                    // Introduce "sparsity": 85% chance it stays 0 (off) so everything isn't routing everywhere at once
                    if (prng.nextFloat() > 0.15f) {
                        newValue = 0.0f;
                    } else {
                        newValue = prng.nextFloat(); // Standard modulation depth
                    }
                }
                
                // EFFECTS (FX_)
                else if (paramID.startsWith("FX_"))
                {
                    if (paramID.contains("MIX") || paramID.contains("WET"))
                    {
                        // Don't completely drown the sound in FX; cap wetness at 60%
                        newValue = prng.nextFloat() * 0.6f;
                    }
                    else if (paramID.contains("FEEDBACK"))
                    {
                        // Keep feedback under 0.75 to prevent infinite, exploding feedback loops
                        newValue = prng.nextFloat() * 0.75f;
                    }
                    else 
                    {
                        newValue = prng.nextFloat();
                    }
                }
		else if (paramID.startsWith("MODE_")) // let's not pick external in
		{
                    int targetedChoice = prng.nextInt(2);
                    auto parameterRange = rangedParam->getNormalisableRange();
                        newValue = parameterRange.convertTo0to1(static_cast<float>(targetedChoice));
		}
                
                // OPERATORS, DETUNE, SHAPE, FILTERS, ETC.
                else if (paramID.startsWith("WAVE_SHAPE") || paramID.startsWith("FILTER_TYPE") || 
			 paramID.startsWith("TEMPO_SYNC") || paramID.startsWith("DETUNE") || 
                         paramID.startsWith("PHASE") || paramID.startsWith("FOLD"))
                {
                    // These are safe to randomize completely across their standard normalized range
                    newValue = prng.nextFloat();
                }
		else if (paramID.startsWith("RATIO"))
		{
		    newValue = prng.nextFloat()*0.5; //Let's say 8 is the max for ratio for randomization
                }
		else
                {
                    // If it's a parameter we didn't explicitly target, don't touch it (e.g., Master Vol, Bypass)
                    shouldRandomize = false; 
                }
    
                // Apply the randomized value if it matches our criteria
                if (shouldRandomize)
                {
                    rangedParam->setValueNotifyingHost(newValue);
                }
            }
        }
    
        currentPresetIndex = -1;
        updateNameLabel();
    }

    void triggerDrumRandomizer()
    {
        auto& prng = juce::Random::getSystemRandom();
        auto& parameters = audioProcessor.apvts.processor.getParameters();
        
        for (auto* param : parameters)
        {
            if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
            {
                juce::String paramID = rangedParam->getParameterID();
                float newValue = prng.nextFloat(); // Default fallback
                bool shouldRandomize = true;
    
                // --- 1. THE PERCUSSIVE ENVELOPE (The most critical part) ---
                if (paramID.startsWith("ATTACK"))
                {
                    // Drums must click/punch instantly. 
                    // 90% of the time, force it nearly to 0. 10% of the time, allow a tiny fade (shakers/claps).
                    if (prng.nextFloat() > 0.1f)
                        newValue = prng.nextFloat() * 0.02f; 
                    else
                        newValue = prng.nextFloat() * 0.15f; 
                }
                else if (paramID.startsWith("DECAY") || paramID.startsWith("RELEASE"))
                {
                    // This defines the drum. 
                    // Short = Hats/Clicks. Medium = Snares/Kicks. Long = 808s/Cymbals.
                    // Keep it mostly under 0.5 to stay tight.
                    newValue = 0.02f + (prng.nextFloat() * 0.45f); 
                }
                else if (paramID.startsWith("SUSTAIN"))
                {
                    // Drums do NOT sustain. Hitting and holding a key shouldn't freeze a drum sound.
                    // Force to 0 (or incredibly low so it dies out).
                    newValue = 0.0f;
                }
                
                // --- 2. OUTPUTS & GAIN ---
                else if (paramID.startsWith("GAIN")) 
                {
                    shouldRandomize = false; // Still don't touch the main gain!
                }
                else if (paramID.startsWith("OUT") || paramID.contains("VOLUME"))
                {
                    // Drum layers usually need to punch through. Keep volumes healthy.
                    if (prng.nextFloat() > 0.3f) {
                        newValue = 0.6f + (prng.nextFloat() * 0.4f); // 60% to 100%
                    } else {
                        newValue = 0.0f; // Mute some layers entirely for simpler, tighter drums
                    }
                }
                
                // --- 3. MODULATION (Crucial for Pitch-Envelopes & Snare Noise) ---
                else if (paramID.startsWith("MOD_"))
                {
                    // Drums rely heavily on fast modulation (like a pitch envelope for a kick punch).
                    // Give it a higher chance of being active (50/50), and let it hit hard.
                    if (prng.nextFloat() > 0.5f) {
                        newValue = 0.0f;
                    } else {
                        newValue = 0.3f + (prng.nextFloat() * 0.7f); 
                    }
                }
                else if (paramID.startsWith("AUDIO_ROUTE_"))
                {
                    // Keep routing sparse so it doesn't turn to mush.
                    if (prng.nextFloat() > 0.2f) {
                        newValue = 0.0f;
                    } else {
                        newValue = prng.nextFloat(); 
                    }
                }
                
                // --- 4. EFFECTS ---
                else if (paramID.startsWith("FX_"))
                {
                    if (paramID.contains("MIX") || paramID.contains("WET"))
                    {
                        // Too much reverb/delay ruins drum transients. Keep it very low or off.
                        // 60% chance of NO effects. Otherwise, max 30% wet.
                        if (prng.nextFloat() > 0.4f)
                            newValue = 0.0f;
                        else
                            newValue = prng.nextFloat() * 0.3f;
                    }
                    else if (paramID.contains("FEEDBACK"))
                    {
                        // Short feedback for metallic resonances/resonators
                        newValue = prng.nextFloat() * 0.4f;
                    }
                    else 
                    {
                        newValue = prng.nextFloat();
                    }
                }
                else if (paramID.startsWith("MODE_")) 
                {
                    int targetedChoice = prng.nextInt(2);
                    auto parameterRange = rangedParam->getNormalisableRange();
                    newValue = parameterRange.convertTo0to1(static_cast<float>(targetedChoice));
                }
                
                // --- 5. TIMBRE (Pitch, Shape, Filter) ---
                else if (paramID.startsWith("RATIO"))
                {
                    // For FM drums: Low ratio (0-1) = Kicks/Toms. High/Weird ratio (3+) = Hats/Metallic.
                    // Randomize across the lower half to avoid ear-piercing squeals.
                    newValue = prng.nextFloat() * 0.6f; 
                }
                else if (paramID.startsWith("WAVE_SHAPE") || paramID.startsWith("FILTER_TYPE") || 
                         paramID.startsWith("DETUNE") || paramID.startsWith("PHASE") || 
                         paramID.startsWith("FOLD") || paramID.startsWith("TEMPO_SYNC"))
                {
                    // Total chaos here is great. Noise waves make snares; sine waves make kicks; 
                    // folding makes industrial percussion. Let it fly.
                    newValue = prng.nextFloat();
                }
                else
                {
                    shouldRandomize = false; 
                }
    
                // Apply the randomized value
                if (shouldRandomize)
                {
                    rangedParam->setValueNotifyingHost(newValue);
                }
            }
        }
    
        currentPresetIndex = -1;
        updateNameLabel();
    }

    void triggerEffectsRandomizer()
    {
        auto& prng = juce::Random::getSystemRandom();
        auto& parameters = audioProcessor.apvts.processor.getParameters();
        
        for (auto* param : parameters)
        {
            if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(param))
            {
                juce::String paramID = rangedParam->getParameterID();
                
                // Default to false. We ONLY want to touch FX parameters.
                bool shouldRandomize = false; 
                float newValue = 0.0f;
    
                if (paramID.startsWith("FX_"))
                {
                    shouldRandomize = true;
    
                    if (paramID.contains("MIX") || paramID.contains("WET"))
                    {
                        // 20% chance the effect gets bypassed entirely for dynamics,
                        // otherwise, let it explore the full wet/dry range.
                        if (prng.nextFloat() > 0.8f) {
                            newValue = 0.0f;
                        } else {
                            newValue = prng.nextFloat(); 
                        }
                    }
                    else if (paramID.contains("FEEDBACK") || paramID.contains("REGEN"))
                    {
                        // Safety mechanism: cap feedback at 85% to prevent infinite, speaker-blowing loops
                        newValue = prng.nextFloat() * 0.85f;
                    }
                    else if (paramID.contains("TIME") || paramID.contains("RATE") || paramID.contains("DEPTH"))
                    {
                        // Modulation rates, delay times, and depths are safe for full chaos
                        newValue = prng.nextFloat();
                    }
                    else if (paramID.contains("TYPE") || paramID.contains("MODE"))
                    {
                        // Properly randomize discrete choices (like Reverb Type or Delay Mode)
                        // using the parameter's actual range.
                        auto parameterRange = rangedParam->getNormalisableRange();
                        
                        // We safely grab a random int between 0 and the max range of the parameter
                        int targetedChoice = prng.nextInt(static_cast<int>(parameterRange.end) + 1); 
                        newValue = parameterRange.convertTo0to1(static_cast<float>(targetedChoice));
                    }
                    else 
                    {
                        // Catch-all for any other FX parameters (Drive, Tone, Width, etc.)
                        newValue = prng.nextFloat();
                    }
                }
    
                // Apply the randomized value ONLY if it matched "FX_"
                if (shouldRandomize)
                {
                    rangedParam->setValueNotifyingHost(newValue);
                }
            }
        }
    
        currentPresetIndex = -1;
        updateNameLabel();
    }

    void triggerSave()
    {
        juce::String defaultName = (currentPresetIndex >= 0 && ! presetFiles.isEmpty())
                                   ? presetFiles[currentPresetIndex].getFileNameWithoutExtension()
                                   : "New Preset";

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

                if (file.getFileExtension().toLowerCase() != ".xml")
                    file = file.withFileExtension ("xml");

                // Use getStateInformation so sample audio is embedded in the file
                juce::MemoryBlock stateData;
                audioProcessor.getStateInformation (stateData);

                // getStateInformation gives us binary-wrapped XML; unwrap it to write readable XML
                std::unique_ptr<juce::XmlElement> xml (audioProcessor.getXmlFromBinary (
                    stateData.getData(), (int) stateData.getSize()));
                if (xml != nullptr)
                    xml->writeTo (file);

                presetFolder = file.getParentDirectory();
                refreshPresetList();
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
                if (xml != nullptr)
                {
                    juce::MemoryBlock stateData;
                    audioProcessor.copyXmlToBinary (*xml, stateData);
                    audioProcessor.setStateInformation (stateData.getData(), (int) stateData.getSize());
                }

                presetFolder = file.getParentDirectory();
                refreshPresetList();
                currentPresetIndex = presetFiles.indexOf (file);
                updateNameLabel();
            });
    }

    FMPluginAudioProcessor& audioProcessor;

    juce::TextButton initButton, saveButton, loadButton;
    juce::TextButton smartRandButton, drumRandButton, effectsRandButton;
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
