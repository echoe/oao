//PresetBar.h
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

    // Called by the editor to handle scale changes
    std::function<void(float)> onScaleChanged;

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
        setupButton (randRouteButton, "R-Route");

        initButton.onClick      = [this] { triggerInit(); };
        saveButton.onClick      = [this] { triggerSave(); };
        loadButton.onClick      = [this] { triggerLoad(); };
        randOpsButton.onClick   = [this] { triggerRandomizer (RandomTarget::OperatorsAndEnvelopes); };
        randModButton.onClick   = [this] { triggerRandomizer (RandomTarget::ModulationMatrix); };
        randRouteButton.onClick = [this] { triggerRandomizer (RandomTarget::RoutingMatrix); };

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

        // Scale selector (50% to 200% in 25% steps)
        scaleLabel.setText ("Size:", juce::dontSendNotification);
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
            float percent   = 50.0f + (selectedId - 1) * 25.0f; // 50, 75, 100, 125...
            float scale     = percent / 100.0f;
            if (onScaleChanged)
                onScaleChanged (scale);
        };
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.brighter (0.1f));
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
    }

    void resized() override //Handles drawing everything on the bar
    {
        auto area = getLocalBounds().reduced (2);
        // 6 buttons + 3 labels + 3 dropdowns = 12 slots
	int labelWidth    = 40;
        int dropdownWidth = 40;
	int remaining     = area.getWidth() - ((labelWidth + dropdownWidth) * 3);
	int slotWidth     = remaining / 6;

        initButton.setBounds        (area.removeFromLeft (slotWidth).reduced (1));
        randOpsButton.setBounds     (area.removeFromLeft (slotWidth).reduced (1));
        randModButton.setBounds     (area.removeFromLeft (slotWidth).reduced (1));
        randRouteButton.setBounds   (area.removeFromLeft (slotWidth).reduced (1));
        saveButton.setBounds        (area.removeFromLeft (slotWidth).reduced (1));
        loadButton.setBounds        (area.removeFromLeft (slotWidth).reduced (1));
    	oversamplingLabel.setBounds    (area.removeFromLeft (labelWidth).reduced (1));
    	oversamplingSelector.setBounds (area.removeFromLeft (dropdownWidth).reduced (1));
	polyphonyLabel.setBounds       (area.removeFromLeft (labelWidth).reduced (1));
        polyphonySelector.setBounds    (area.removeFromLeft (dropdownWidth).reduced (1));
    	scaleLabel.setBounds           (area.removeFromLeft (labelWidth).reduced (1));
    	scaleSelector.setBounds        (area.reduced (1));

    }

private:
    void triggerInit()
    {
        auto& parameters = audioProcessor.apvts.processor.getParameters();
        for (auto* param : parameters)
            if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
                rangedParam->setValueNotifyingHost (rangedParam->getDefaultValue());
    }
    void triggerRandomizer (RandomTarget target) //handles all randomization
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
                    case RandomTarget::OperatorsAndEnvelopes: //picks all vars on purpose to randomize
                        if (paramID.startsWith ("MODE_") || paramID.startsWith ("WAVE_SHAPE") || paramID.startsWith ("FILTER_TYPE") || paramID.startsWith ("TEMPO_SYNC") || paramID.startsWith ("RATIO") || paramID.startsWith ("DETUNE") || paramID.startsWith ("PHASE") || paramID.startsWith ("FOLD") || paramID.startsWith ("ATTACK") || paramID.startsWith ("DECAY") || paramID.startsWith ("SUSTAIN") || paramID.startsWith ("RELEASE") || paramID.startsWith ("OUT"))
                            shouldRandomize = true;
                        break;
                    case RandomTarget::ModulationMatrix: //randomizes FM matrix
                        if (paramID.startsWith ("MOD_"))
                            shouldRandomize = true;
                        break;
                    case RandomTarget::RoutingMatrix: //randomizes audio routing
                        if (paramID.startsWith ("AUDIO_ROUTE_"))
                            shouldRandomize = true;
                        break;
                }
                if (shouldRandomize)
                    rangedParam->setValueNotifyingHost (prng.nextFloat());
            }
        }
    }
    void triggerSave()
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Save Synth Preset",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.xml");
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& chooser)
            {
                auto file = chooser.getResult();
                if (file != juce::File())
                {
                    auto state = audioProcessor.apvts.copyState();
                    std::unique_ptr<juce::XmlElement> xml (state.createXml());
                    xml->writeTo (file);
                }
            });
    }
    void triggerLoad()
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Load Synth Preset",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.xml");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& chooser)
            {
                auto file = chooser.getResult();
                if (file.existsAsFile())
                {
                    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (file));
                    if (xml != nullptr && xml->hasTagName (audioProcessor.apvts.state.getType()))
                        audioProcessor.apvts.replaceState (juce::ValueTree::fromXml (*xml));
                }
            });
    }
    FMPluginAudioProcessor& audioProcessor;
    juce::TextButton initButton, saveButton, loadButton;
    juce::TextButton randOpsButton, randModButton, randRouteButton;
    juce::ComboBox oversamplingSelector;
    juce::ComboBox scaleSelector;
    juce::Label oversamplingLabel;
    juce::Label scaleLabel;
    juce::ComboBox polyphonySelector;
    juce::Label polyphonyLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;
};
