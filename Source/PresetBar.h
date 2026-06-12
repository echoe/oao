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

    // ============================================================
    //  RECIPE SYSTEM — shared types and helpers
    // ============================================================

    struct OpRecipe
    {
        float ratio     = 1.0f;   // real Hz ratio, 0.01..16.0
        float detune    = 0.0f;   // real cents, -50..50
        float phase     = 0.0f;   // real degrees, 0..360
        float fold      = 0.0f;   // 0..1
        float volume    = -1.0f;  // 0..1; -1 = randomize freely
        float attack    = 0.01f;  // seconds, 0.001..5.0
        float decay     = 0.20f;  // seconds, 0.01..5.0
        float sustain   = 0.0f;   // 0..1
        float release   = 0.20f;  // seconds, 0.01..5.0
        int   waveShape = 0;      // discrete index (0=Sine,1=Tri,2=Saw,3=Sq,4=Noise)
        bool  freeRatio = false;  // true = ignore ratio field, randomize
        bool  freeWave  = false;  // true = ignore waveShape field, randomize
    };

    struct SynthRecipe
    {
        const char* name;
        float       weight;
        OpRecipe    ops[6];
        float       modDensity;    // fraction of MOD_ slots that fire
        float       routeDensity;  // fraction of AUDIO_ROUTE_ slots that fire
    };

    struct DrumRecipe
    {
        const char* name;
        float       weight;
        OpRecipe    ops[6];
        float       modDensity;
        float       modDepthMin;
        float       modDepthMax;
    };

    // Wobble a real-unit value by ±amount, then clamp to [lo, hi]
    static float wobbleReal (float value, juce::Random& prng, float amount,
                             float lo, float hi)
    {
        float wobbled = value + (prng.nextFloat() * 2.0f - 1.0f) * amount;
        return juce::jlimit (lo, hi, wobbled);
    }

    // Set a parameter using its real (non-normalized) value
    void setReal (const juce::String& id, float realValue)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                audioProcessor.apvts.getParameter (id)))
        {
            float norm = p->getNormalisableRange().convertTo0to1 (realValue);
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
        }
    }

    // Set a discrete AudioParameterChoice by index
    void setChoice (const juce::String& id, int index)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                audioProcessor.apvts.getParameter (id)))
        {
            auto range = p->getNormalisableRange();
            p->setValueNotifyingHost (range.convertTo0to1 (static_cast<float> (index)));
        }
    }

    // Apply one OpRecipe to one operator (1-indexed).
    // Locks MODE_ to Wave (0) and FREQ_MODE_ to free-running (0).
    void applyOpRecipe (int opIndex, const OpRecipe& op, juce::Random& prng)
    {
        juce::String s = juce::String (opIndex + 1);

        // Lock to Wave mode so ratio/detune/phase/fold are guaranteed active
        setChoice ("MODE_"      + s, 0);
        setChoice ("FREQ_MODE_" + s, 0);  // free-running, not tempo-synced

        // Wave shape
        if (!op.freeWave)
        {
            setChoice ("WAVE_SHAPE_" + s, op.waveShape);
        }
        else
        {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                    audioProcessor.apvts.getParameter ("WAVE_SHAPE_" + s)))
            {
                int n = static_cast<int> (p->getNormalisableRange().end) + 1;
                setChoice ("WAVE_SHAPE_" + s, prng.nextInt (n));
            }
        }

        // Ratio — wobble is proportional so small ratios stay small
        if (!op.freeRatio)
            setReal ("RATIO_" + s, wobbleReal (op.ratio, prng,
                                               op.ratio * 0.07f, 0.01f, 16.0f));
        else
            setReal ("RATIO_" + s, 0.01f + prng.nextFloat() * 15.99f);

        // Detune — wobble ±3 cents absolute
        setReal ("DETUNE_" + s, wobbleReal (op.detune, prng, 3.0f, -50.0f, 50.0f));

        // Phase — wobble ±15 degrees
        setReal ("PHASE_" + s, wobbleReal (op.phase, prng, 15.0f, 0.0f, 360.0f));

        // Fold
        setReal ("FOLD_" + s, wobbleReal (op.fold, prng, 0.05f, 0.0f, 1.0f));

        // Output level
        if (op.volume >= 0.0f)
            setReal ("OUT_" + s, wobbleReal (op.volume, prng, 0.06f, 0.0f, 1.0f));
        else
            setReal ("OUT_" + s, prng.nextFloat());

        // Envelopes — wobble proportional to value so short times stay short
        setReal ("ATTACK_"  + s, wobbleReal (op.attack,  prng, op.attack  * 0.10f, 0.001f, 5.0f));
        setReal ("DECAY_"   + s, wobbleReal (op.decay,   prng, op.decay   * 0.10f, 0.01f,  5.0f));
        setReal ("SUSTAIN_" + s, wobbleReal (op.sustain, prng, 0.05f,               0.0f,   1.0f));
        setReal ("RELEASE_" + s, wobbleReal (op.release, prng, op.release * 0.10f, 0.01f,  5.0f));
    }

    // Randomize all 3 FX slots with type-first, label-aware knob logic.
    // drumMode adjusts mix levels and biases toward transient-safe effect types.
    void randomizeFXKnob (juce::RangedAudioParameter* p,
                          const juce::String& label,
                          juce::Random& prng,
                          bool drumMode)
    {
        float newValue = 0.0f;

        if (label == "--")
            newValue = 0.0f;
        else if (label == "Feedback" || label == "Regen")
            newValue = drumMode ? prng.nextFloat() * 0.35f : prng.nextFloat() * 0.82f;
        else if (label == "Cutoff")
            newValue = drumMode ? 0.5f + prng.nextFloat() * 0.5f
                                : 0.3f + prng.nextFloat() * 0.7f;
        else if (label == "Resonance")
            newValue = prng.nextFloat() * 0.7f;
        else if (label == "Threshold")
            newValue = 0.4f + prng.nextFloat() * 0.6f;
        else if (label == "Ratio")
            newValue = prng.nextFloat() * 0.6f;
        else if (label == "Bits")
            newValue = 0.2f + prng.nextFloat() * 0.8f;
        else if (label == "Pitch" || label == "Shimmer")
            newValue = 0.2f + prng.nextFloat() * 0.6f;
        else if (label.contains ("Gain"))
            newValue = 0.3f + prng.nextFloat() * 0.5f;
        else if (label == "Mode"   || label == "Type"    || label == "Voices" ||
                 label == "Flavor" || label == "Shape"   || label == "Loop On" ||
                 label == "Freeze On")
        {
            auto range = p->getNormalisableRange();
            int choice = prng.nextInt (static_cast<int> (range.end) + 1);
            newValue   = range.convertTo0to1 (static_cast<float> (choice));
        }
        else
            newValue = prng.nextFloat();

        p->setValueNotifyingHost (newValue);
    }

    void randomizeAllFXSlots (juce::Random& prng, bool drumMode)
    {
        auto& apvts     = audioProcessor.apvts;
        auto  typeChoices = ProjectConfig::getEffectTypeChoices();
        int   numTypes    = typeChoices.size();

        for (int slot = 1; slot <= 3; ++slot)
        {
            juce::String s = juce::String (slot);

            // 1. Pick effect type
            int newTypeIndex = 0;
            if (drumMode)
            {
                // Drum-friendly: filters, compressor, EQ, distortion, lofi, bitcrush, comb
                static const int drumFriendly[] = { 0, 1, 2, 3, 4, 5, 7, 8, 10, 14, 15 };
                constexpr int    numFriendly    = 11;
                newTypeIndex = prng.nextFloat() < 0.70f
                             ? drumFriendly[prng.nextInt (numFriendly)]
                             : prng.nextInt (numTypes);
            }
            else
            {
                newTypeIndex = 1 + prng.nextInt (numTypes - 1); // skip None
            }

            if (auto* typeParam = dynamic_cast<juce::RangedAudioParameter*> (
                    apvts.getParameter ("FX_TYPE_" + s)))
            {
                auto range = typeParam->getNormalisableRange();
                typeParam->setValueNotifyingHost (
                    range.convertTo0to1 (static_cast<float> (newTypeIndex)));
            }

            // 2. Sync
            if (auto* syncParam = dynamic_cast<juce::RangedAudioParameter*> (
                    apvts.getParameter ("FX_SYNC_" + s)))
                syncParam->setValueNotifyingHost (
                    (!drumMode && prng.nextFloat() < 0.30f) ? 1.0f : 0.0f);

            // 3. Mix
            if (auto* mixParam = dynamic_cast<juce::RangedAudioParameter*> (
                    apvts.getParameter ("FX_MIX_" + s)))
            {
                float mix = 0.0f;
                if (newTypeIndex == 0)
                    mix = 0.0f;
                else if (drumMode)
                    mix = prng.nextFloat() < 0.55f ? 0.0f : prng.nextFloat() * 0.35f;
                else
                    mix = prng.nextFloat() < 0.20f ? 0.0f : prng.nextFloat();
                mixParam->setValueNotifyingHost (mix);
            }

            // 4. Knobs — label-aware
            auto labels = ProjectConfig::getEffectKnobLabels (newTypeIndex);
            const juce::StringArray knobIDs = { "FX_RATIO_", "FX_DETUNE_",
                                                "FX_PHASE_", "FX_FOLD_" };
            for (int k = 0; k < 4; ++k)
                if (auto* knobParam = dynamic_cast<juce::RangedAudioParameter*> (
                        apvts.getParameter (knobIDs[k] + s)))
                    randomizeFXKnob (knobParam, labels[k], prng, drumMode);
        }
    }

    // ============================================================
    //  SMART (SYNTH) RANDOMIZER
    // ============================================================
    void triggerSmartRandomizer()
    {
        auto& prng       = juce::Random::getSystemRandom();
        auto& apvts      = audioProcessor.apvts;
        auto& parameters = apvts.processor.getParameters();
        
        // Synth recipes — ratio/detune/phase/fold/volume in real units;
        // envelopes in seconds. waveShape: 0=Sine 1=Tri 2=Saw 3=Sq 4=Noise.
        // OpRecipe fields: ratio, detune, phase, fold, volume,
        //                  attack, decay, sustain, release, waveShape, freeRatio, freeWave
        static const SynthRecipe synthRecipes[] =
        {
            // ---- PAD ----
            // Slow swell, three detuned carriers, rich modulation
            { "Pad", 0.25f,
              {
                { 1.0f,  0.0f,  0.0f, 0.00f, 0.75f, 0.80f, 1.20f, 0.80f, 1.50f, 0, false, false },
                { 2.0f,  8.0f,  0.0f, 0.00f, 0.55f, 0.90f, 1.20f, 0.70f, 1.40f, 1, false, false },
                { 1.5f, -8.0f,  0.0f, 0.00f, 0.40f, 0.95f, 1.20f, 0.60f, 1.30f, 0, false, false },
                { 3.0f,  0.0f,  0.0f, 0.10f, 0.00f, 0.70f, 1.20f, 0.80f, 1.40f, 0, false, false },
                { 2.0f,  5.0f,  0.0f, 0.00f, 0.00f, 0.75f, 1.20f, 0.70f, 1.30f, 0, false, false },
                { 4.0f, -5.0f,  0.0f, 0.15f, 0.00f, 0.80f, 1.20f, 0.60f, 1.20f, 1, false, false },
              },
              0.55f, 0.20f
            },

            // ---- LEAD ----
            // Punchy single carrier, saw wave, moderate modulation
            { "Lead", 0.20f,
              {
                { 1.0f,  0.0f,  0.0f, 0.00f, 0.90f, 0.001f, 0.20f, 0.65f, 0.25f, 2, false, false },
                { 3.0f,  0.0f,  0.0f, 0.05f, 0.00f, 0.001f, 0.15f, 0.50f, 0.20f, 0, false, false },
                { 2.0f,  0.0f,  0.0f, 0.00f, 0.00f, 0.001f, 0.18f, 0.40f, 0.18f, 0, false, false },
                { 0.5f,  0.0f,  0.0f, 0.00f, 0.35f, 0.001f, 0.22f, 0.50f, 0.22f, 0, false, false },
                { 1.0f,  0.0f,  0.0f, 0.00f, 0.00f, 0.001f, 0.18f, 0.00f, 0.18f, 0, true,  false },
                { 1.0f,  0.0f,  0.0f, 0.00f, 0.00f, 0.001f, 0.18f, 0.00f, 0.18f, 0, true,  false },
              },
              0.35f, 0.15f
            },

            // ---- BASS ----
            // Low ratios, tight envelope, sine carriers
            { "Bass", 0.20f,
              {
                { 1.0f,  0.0f,  0.0f, 0.00f, 0.85f, 0.001f, 0.18f, 0.55f, 0.18f, 0, false, false },
                { 0.5f,  0.0f,  0.0f, 0.00f, 0.60f, 0.001f, 0.15f, 0.50f, 0.15f, 0, false, false },
                { 2.0f,  0.0f,  0.0f, 0.05f, 0.00f, 0.001f, 0.12f, 0.40f, 0.12f, 0, false, false },
                { 5.0f,  0.0f,  0.0f, 0.00f, 0.00f, 0.001f, 0.04f, 0.00f, 0.04f, 0, false, false },
                { 2.0f,  0.0f,  0.0f, 0.00f, 0.00f, 0.001f, 0.12f, 0.00f, 0.12f, 0, false, false },
                { 3.0f,  0.0f,  0.0f, 0.00f, 0.00f, 0.001f, 0.12f, 0.00f, 0.12f, 0, false, false },
              },
              0.30f, 0.10f
            },

            // ---- BELL / PLUCK ----
            // Fast attack+decay, sustain=0, inharmonic DX-style ratios
            { "Bell", 0.20f,
              {
                { 1.00f, 0.0f,  0.0f, 0.00f, 0.80f, 0.001f, 0.80f, 0.00f, 1.20f, 0, false, false },
                { 2.52f, 2.0f,  0.0f, 0.00f, 0.50f, 0.001f, 0.70f, 0.00f, 1.00f, 0, false, false },
                { 3.50f, 0.0f,  0.0f, 0.00f, 0.00f, 0.001f, 0.65f, 0.00f, 0.90f, 0, false, false },
                { 5.83f, 0.0f,  0.0f, 0.00f, 0.00f, 0.001f, 0.55f, 0.00f, 0.80f, 0, false, false },
                { 7.00f, 0.0f,  0.0f, 0.05f, 0.00f, 0.001f, 0.45f, 0.00f, 0.70f, 0, false, false },
                { 1.00f, 0.0f,  0.0f, 0.00f, 0.00f, 0.001f, 0.40f, 0.00f, 0.60f, 0, true,  false },
              },
              0.45f, 0.15f
            },

            // ---- ORGAN ----
            // All carriers, drawbar ratios, gate envelope (instant on/off, full sustain)
            { "Organ", 0.15f,
              {
                { 1.0f, 0.0f, 0.0f, 0.0f, 0.80f, 0.001f, 0.001f, 1.0f, 0.01f, 0, false, false },
                { 2.0f, 0.0f, 0.0f, 0.0f, 0.65f, 0.001f, 0.001f, 1.0f, 0.01f, 0, false, false },
                { 3.0f, 0.0f, 0.0f, 0.0f, 0.50f, 0.001f, 0.001f, 1.0f, 0.01f, 0, false, false },
                { 4.0f, 0.0f, 0.0f, 0.0f, 0.35f, 0.001f, 0.001f, 1.0f, 0.01f, 0, false, false },
                { 6.0f, 0.0f, 0.0f, 0.0f, 0.25f, 0.001f, 0.001f, 1.0f, 0.01f, 0, false, false },
                { 8.0f, 0.0f, 0.0f, 0.0f, 0.20f, 0.001f, 0.001f, 1.0f, 0.01f, 0, false, false },
              },
              0.20f, 0.10f
            },
        };
        constexpr int numSynthRecipes = 5;

        // Weighted draw
        float synthTotal = 0.0f;
        for (auto& r : synthRecipes) synthTotal += r.weight;
        float synthPick = prng.nextFloat() * synthTotal;
        float synthAccum = 0.0f;
        int synthChosen = 0;
        for (int i = 0; i < numSynthRecipes; ++i)
        {
            synthAccum += synthRecipes[i].weight;
            if (synthPick <= synthAccum) { synthChosen = i; break; }
        }
        const SynthRecipe& recipe = synthRecipes[synthChosen];

        // Apply per-operator recipe
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
            applyOpRecipe (i, recipe.ops[i], prng);

        // Modulation matrix, routing, and leftovers
        for (auto* param : parameters)
        {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
            {
                juce::String id = p->getParameterID();

                if (id.startsWith ("MOD_"))
                {
                    float v = prng.nextFloat() < recipe.modDensity
                            ? 0.15f + prng.nextFloat() * 0.60f
                            : 0.0f;
                    p->setValueNotifyingHost (v);
                }
                else if (id.startsWith ("AUDIO_ROUTE_"))
                {
                    p->setValueNotifyingHost (
                        prng.nextFloat() < recipe.routeDensity ? prng.nextFloat() : 0.0f);
                }
                else if (id.startsWith ("TEMPO_SYNC"))
                {
                    p->setValueNotifyingHost (prng.nextFloat() < 0.70f ? 0.0f : 1.0f);
                }
                // All operator params already set by applyOpRecipe; skip them here
            }
        }

        randomizeAllFXSlots (prng, /*drumMode=*/false);

        currentPresetIndex = -1;
        updateNameLabel();
    }

    // ============================================================
    //  DRUM RANDOMIZER
    // ============================================================
    void triggerDrumRandomizer()
    {
        auto& prng       = juce::Random::getSystemRandom();
        auto& apvts      = audioProcessor.apvts;
        auto& parameters = apvts.processor.getParameters();

        // Drum recipes — same OpRecipe fields as synth.
        // op[0..1] = carriers (OUT audible), op[2..5] = modulators (volume=0.0).
        static const DrumRecipe drumRecipes[] =
        {
            // ---- KICK ----
            // op1: sine carrier, main body
            // op2: sine sub layer
            // op3: high-ratio click modulator (very short)
            // op4: pitch-envelope modulator (makes the FM sweep)
            { "Kick", 0.28f,
              {
                { 1.0f, 0.0f, 0.0f, 0.00f, 0.85f, 0.001f, 0.35f, 0.0f, 0.30f, 0, false, false },
                { 2.0f, 0.0f, 0.0f, 0.00f, 0.40f, 0.001f, 0.25f, 0.0f, 0.20f, 0, false, false },
                { 7.0f, 0.0f, 0.0f, 0.00f, 0.00f, 0.001f, 0.04f, 0.0f, 0.04f, 0, false, false },
                { 1.0f, 0.0f, 0.0f, 0.00f, 0.00f, 0.001f, 0.18f, 0.0f, 0.15f, 0, false, false },
                { 1.0f, 0.0f, 0.0f, 0.00f, 0.00f, 0.001f, 0.15f, 0.0f, 0.12f, 0, true,  false },
                { 2.0f, 0.0f, 0.0f, 0.00f, 0.00f, 0.001f, 0.15f, 0.0f, 0.12f, 0, true,  false },
              },
              0.55f, 0.35f, 0.75f
            },

            // ---- SNARE ----
            // op1: sine body carrier
            // op2: noise carrier for the rattle
            // op3: high-ratio crack modulator
            // op4: body FM modulator
            { "Snare", 0.25f,
              {
                { 1.0f, 0.0f, 0.0f, 0.00f, 0.70f, 0.001f, 0.12f, 0.0f, 0.12f, 0, false, false },
                { 1.0f, 0.0f, 0.0f, 0.00f, 0.65f, 0.001f, 0.15f, 0.0f, 0.14f, 4, false, false },
                { 9.0f, 5.0f, 0.0f, 0.00f, 0.00f, 0.001f, 0.05f, 0.0f, 0.05f, 3, false, false },
                { 2.0f, 0.0f, 0.0f, 0.00f, 0.00f, 0.001f, 0.10f, 0.0f, 0.10f, 0, false, false },
                { 1.0f, 0.0f, 0.0f, 0.00f, 0.00f, 0.001f, 0.10f, 0.0f, 0.10f, 0, true,  true  },
                { 1.0f, 0.0f, 0.0f, 0.00f, 0.00f, 0.001f, 0.10f, 0.0f, 0.10f, 0, true,  true  },
              },
              0.50f, 0.25f, 0.70f
            },

            // ---- CLOSED HAT ----
            // All inharmonic high ratios, noise/square waves, very short decay
            { "ClosedHat", 0.18f,
              {
                {  5.0f,  3.0f, 0.0f, 0.0f, 0.75f, 0.001f, 0.030f, 0.0f, 0.030f, 4, false, false },
                {  7.0f, -3.0f, 0.0f, 0.0f, 0.55f, 0.001f, 0.025f, 0.0f, 0.025f, 3, false, false },
                { 11.0f,  0.0f, 0.0f, 0.0f, 0.00f, 0.001f, 0.025f, 0.0f, 0.025f, 4, false, false },
                { 13.0f,  2.0f, 0.0f, 0.0f, 0.00f, 0.001f, 0.020f, 0.0f, 0.020f, 3, false, false },
                {  9.0f,  0.0f, 0.0f, 0.0f, 0.00f, 0.001f, 0.020f, 0.0f, 0.020f, 4, false, false },
                {  7.0f, -2.0f, 0.0f, 0.0f, 0.00f, 0.001f, 0.020f, 0.0f, 0.020f, 3, false, false },
              },
              0.60f, 0.30f, 0.65f
            },

            // ---- OPEN HAT ----
            // Same inharmonic character as closed hat, longer decay/release
            { "OpenHat", 0.12f,
              {
                {  5.0f,  3.0f, 0.0f, 0.0f, 0.75f, 0.001f, 0.30f, 0.0f, 0.35f, 4, false, false },
                {  7.0f, -3.0f, 0.0f, 0.0f, 0.55f, 0.001f, 0.25f, 0.0f, 0.30f, 3, false, false },
                { 11.0f,  0.0f, 0.0f, 0.0f, 0.00f, 0.001f, 0.22f, 0.0f, 0.25f, 4, false, false },
                { 13.0f,  2.0f, 0.0f, 0.0f, 0.00f, 0.001f, 0.20f, 0.0f, 0.22f, 3, false, false },
                {  9.0f,  0.0f, 0.0f, 0.0f, 0.00f, 0.001f, 0.18f, 0.0f, 0.20f, 4, true,  false },
                {  7.0f, -2.0f, 0.0f, 0.0f, 0.00f, 0.001f, 0.18f, 0.0f, 0.18f, 3, true,  false },
              },
              0.55f, 0.25f, 0.60f
            },

            // ---- METALLIC / CYMBAL ----
            // Classic Yamaha DX cymbal ratios: 1.0, 1.41, 1.57, 1.73, 2.0, 2.52
            { "Metallic", 0.10f,
              {
                { 1.00f,  0.0f, 0.0f, 0.20f, 0.70f, 0.001f, 0.40f, 0.0f, 0.45f, 3, false, false },
                { 1.41f,  3.0f, 0.0f, 0.20f, 0.50f, 0.001f, 0.35f, 0.0f, 0.40f, 3, false, false },
                { 1.57f,  0.0f, 0.0f, 0.10f, 0.00f, 0.001f, 0.30f, 0.0f, 0.35f, 3, false, false },
                { 1.73f, -3.0f, 0.0f, 0.10f, 0.00f, 0.001f, 0.25f, 0.0f, 0.30f, 3, false, false },
                { 2.52f,  0.0f, 0.0f, 0.20f, 0.00f, 0.001f, 0.22f, 0.0f, 0.25f, 3, false, false },
                { 2.00f,  2.0f, 0.0f, 0.30f, 0.00f, 0.001f, 0.20f, 0.0f, 0.22f, 4, false, false },
              },
              0.65f, 0.35f, 0.80f
            },

            // ---- TOM ----
            // Like a kick but pitched higher, some body FM
            { "Tom", 0.07f,
              {
                { 1.0f, 0.0f, 0.0f, 0.00f, 0.80f, 0.001f, 0.28f, 0.0f, 0.25f, 0, false, false },
                { 2.0f, 0.0f, 0.0f, 0.00f, 0.45f, 0.001f, 0.22f, 0.0f, 0.20f, 0, false, false },
                { 5.0f, 0.0f, 0.0f, 0.00f, 0.00f, 0.001f, 0.07f, 0.0f, 0.07f, 0, false, false },
                { 1.0f, 0.0f, 0.0f, 0.00f, 0.00f, 0.001f, 0.18f, 0.0f, 0.15f, 0, false, false },
                { 1.0f, 0.0f, 0.0f, 0.00f, 0.00f, 0.001f, 0.15f, 0.0f, 0.12f, 0, true,  false },
                { 2.0f, 0.0f, 0.0f, 0.00f, 0.00f, 0.001f, 0.15f, 0.0f, 0.12f, 0, true,  false },
              },
              0.50f, 0.30f, 0.70f
            },
        };
        constexpr int numDrumRecipes = 6;

        // Weighted draw
        float drumTotal = 0.0f;
        for (auto& r : drumRecipes) drumTotal += r.weight;
        float drumPick = prng.nextFloat() * drumTotal;
        float drumAccum = 0.0f;
        int drumChosen = 0;
        for (int i = 0; i < numDrumRecipes; ++i)
        {
            drumAccum += drumRecipes[i].weight;
            if (drumPick <= drumAccum) { drumChosen = i; break; }
        }
        const DrumRecipe& recipe = drumRecipes[drumChosen];

        // Apply per-operator recipe
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
            applyOpRecipe (i, recipe.ops[i], prng);

        // Modulation matrix, routing, and leftovers
        for (auto* param : parameters)
        {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
            {
                juce::String id = p->getParameterID();

                if (id.startsWith ("MOD_"))
                {
                    float v = prng.nextFloat() < recipe.modDensity
                            ? recipe.modDepthMin
                              + prng.nextFloat() * (recipe.modDepthMax - recipe.modDepthMin)
                            : 0.0f;
                    p->setValueNotifyingHost (v);
                }
                else if (id.startsWith ("AUDIO_ROUTE_"))
                {
                    p->setValueNotifyingHost (
                        prng.nextFloat() < 0.80f ? 0.0f : prng.nextFloat());
                }
                else if (id.startsWith ("TEMPO_SYNC"))
                {
                    p->setValueNotifyingHost (0.0f); // drums never tempo-sync
                }
            }
        }

        randomizeAllFXSlots (prng, /*drumMode=*/true);

        currentPresetIndex = -1;
        updateNameLabel();
    }

    // ============================================================
    //  EFFECTS RANDOMIZER
    // ============================================================
    void triggerEffectsRandomizer()
    {
        auto& prng = juce::Random::getSystemRandom();
        randomizeAllFXSlots (prng, /*drumMode=*/false);
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
