// OperatorsPage.h
#pragma once
#include <JuceHeader.h>
#include "Constants.h"
#include "OAOColors.h"

struct CompactOperatorGroup : public juce::Component
{
    CompactOperatorGroup (juce::AudioProcessorValueTreeState& valueTreeState, int opIndex, OAOColors& c)
        : apvts (valueTreeState), opNum (juce::String (opIndex + 1)), colors (c)
    {
        auto setupSlider = [this] (juce::Slider& slider, juce::Label& label, const juce::String& text, bool opcontrol)
        {
            slider.setSliderStyle (opcontrol ? juce::Slider::LinearHorizontal : juce::Slider::LinearHorizontal); // bool to set different slider depending on if control is operator or envelope control
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 12);
            addAndMakeVisible (slider);
            
            label.setText (text, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (label);
        };
        setupSlider (ratioSlider, ratioLabel, "Ratio", true);
        setupSlider (detuneSlider, detuneLabel, "Detune", true);
        setupSlider (phaseSlider, phaseLabel, "Phase", true);
        setupSlider (foldSlider, foldLabel, "Fold", true);
        // Envelope source — picks which shared envelope generator (see the Envelopes panel) drives this operator
        envSourceLabel.setText ("Env", juce::dontSendNotification);
        envSourceLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (envSourceLabel);
        for (int e = 0; e < ProjectConfig::numEnvelopes; ++e)
            envSourceSelector.addItem ("Env " + juce::String (e + 1), e + 1);
        addAndMakeVisible (envSourceSelector);
        // label operator
        opHeaderLabel.setText (opNum, juce::dontSendNotification);
        opHeaderLabel.setFont (juce::FontOptions (39.0f, juce::Font::bold));
        addAndMakeVisible (opHeaderLabel);
        // Letting us swap oscillator mode for easier operator control
        freqModeSelector.addItemList ({ "Std", "Sync", "Hz", "LFO" }, 1);
        addAndMakeVisible (freqModeSelector);
	modeSelector.addItemList ({ "Wave", "Additive", "Sample", "Effect" }, 1);
        addAndMakeVisible (modeSelector);
        waveShapeSelector.addItemList ({ "Sine", "Triangle", "Saw", "Square", "Pulse", "SquarePWM", "White Noise", "Pink Noise" }, 1);
        addAndMakeVisible (waveShapeSelector);
        effectTypeSelector.addItemList (ProjectConfig::getEffectTypeChoices(), 1);
        addAndMakeVisible (effectTypeSelector);

        // Load Sample button — only shown in Sample mode
        loadSampleButton.setButtonText ("Load Sample");
        loadSampleButton.setVisible (false);
        addAndMakeVisible (loadSampleButton);
        loadSampleButton.onClick = [this]
        {
            fileChooser = std::make_unique<juce::FileChooser> (
                "Load Sample for Operator " + opNum,
                juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");

            fileChooser->launchAsync (juce::FileBrowserComponent::openMode |
                                      juce::FileBrowserComponent::canSelectFiles,
                [this] (const juce::FileChooser& fc)
                {
                    auto results = fc.getResults();
                    if (results.size() > 0 && onLoadSample)
                    {
                        int opIdx = opNum.getIntValue() - 1; // Convert "1"-"6" back to 0-index
                        onLoadSample (opIdx, results[0]);

                        // Update button text to show the loaded filename
                        loadSampleButton.setButtonText (results[0].getFileNameWithoutExtension());
                    }
                });
        };

        // APVTS Links
        freqModeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "FREQ_MODE_" + opNum, freqModeSelector);
	ratioAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "RATIO_" + opNum, ratioSlider);
        detuneAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "DETUNE_" + opNum, detuneSlider);
        phaseAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "PHASE_" + opNum, phaseSlider);
        foldAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "FOLD_" + opNum, foldSlider);
        envSourceAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "ENV_SRC_" + opNum, envSourceSelector);
        modeAttach      = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "MODE_" + opNum, modeSelector);
        waveShapeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "WAVE_SHAPE_" + opNum, waveShapeSelector);
        effectTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "FILTER_TYPE_" + opNum, effectTypeSelector);

        // Safe UI state triggers using the stored class member reference
        modeSelector.onChange = [this]() { updateUIState(); };
        waveShapeSelector.onChange = [this]() { updateUIState(); };
	freqModeSelector.onChange = [this]() { updateUIState(); };
	effectTypeSelector.onChange = [this]() {updateUIState(); };
        updateUIState(); // on load
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (colors.background);
        g.fillRoundedRectangle (bounds, 4.0f);

        g.setColour (colors.text.withAlpha (0.15f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 4.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (static_cast<int> (getWidth() * 0.02f),
                                              static_cast<int> (getHeight() * 0.02f));
        float w          = static_cast<float> (area.getWidth());
        float h          = static_cast<float> (area.getHeight());

        // --- LEFT COLUMN: Operator num on one side, 3 stacked selectors on right---
	int opNumW = juce::jmax (70, juce::roundToInt (w * 0.03f));
	auto opNum = area.removeFromLeft (opNumW);
	opHeaderLabel.setBounds (opNum);
        int leftColW  = juce::jmax (70, juce::roundToInt (w * 0.13f));
        auto leftCol  = area.removeFromLeft (leftColW);

        int selectorH = juce::jmax (16, juce::roundToInt (h * 0.33f));
        freqModeSelector.setBounds (leftCol.removeFromTop (selectorH).reduced (1));
        modeSelector.setBounds     (leftCol.removeFromTop (selectorH).reduced (1));

        if (effectTypeSelector.isVisible())
            effectTypeSelector.setBounds (leftCol.removeFromTop (selectorH).reduced (1));
        else if (loadSampleButton.isVisible())
            loadSampleButton.setBounds (leftCol.removeFromTop (selectorH).reduced (1));
        else
            waveShapeSelector.setBounds (leftCol.removeFromTop (selectorH).reduced (1));

        area.removeFromLeft (juce::roundToInt (w * 0.01f)); // small gap before knobs

        // Knobs and Sliders in a row :D
        int Width = area.getWidth() / 5;

        // Shared text box / label sizing
        int textBoxW = juce::roundToInt (sharedKnobTarget * ProjectConfig::textBoxWidthFraction);
        int textBoxH = juce::jlimit (12, 70, juce::roundToInt (sharedKnobTarget * colors.textBoxHeightFraction));
        int labelH   = juce::jmax (10, juce::roundToInt (sharedKnobTarget * colors.textBoxHeightFraction));

        // Clamp each knob's box to the shared target diameter (centered within its column).
        int targetBoxSize = sharedKnobTarget + 8;
        int knobAreaH     = area.getHeight() - labelH;
        int knobBoxW      = juce::jmin (Width, targetBoxSize);
        int knobBoxH       = juce::jmin (knobAreaH, targetBoxSize + textBoxH);

        auto clampKnob = [knobBoxW, knobBoxH] (juce::Rectangle<int> box)
        {
            return box.withSizeKeepingCentre (knobBoxW, knobBoxH);
        };
        
        auto rArea = area.removeFromLeft (Width);
        ratioLabel.setBounds  (rArea.removeFromTop (labelH));
        ratioSlider.setBounds (clampKnob (rArea));
        
        auto dArea = area.removeFromLeft (Width);
        detuneLabel.setBounds  (dArea.removeFromTop (labelH));
        detuneSlider.setBounds (clampKnob (dArea));
        
        auto pArea = area.removeFromLeft (Width);
        phaseLabel.setBounds  (pArea.removeFromTop (labelH));
        phaseSlider.setBounds (clampKnob (pArea));
        
        auto lArea = area.removeFromLeft (Width);
        foldLabel.setBounds  (lArea.removeFromTop (labelH));
        foldSlider.setBounds (clampKnob (lArea));
    
        // --- ENVELOPE SOURCE ---
        // Takes the space the four ADSR sliders used to occupy; the actual ADSR now lives
        // in the shared Envelopes panel, this just picks which one drives this operator.
        auto envArea = area.removeFromLeft (Width);
        envSourceLabel.setBounds (envArea.removeFromTop (labelH));
        envSourceSelector.setBounds (envArea.reduced (2).withSizeKeepingCentre (
            envArea.getWidth(), juce::jmin (envArea.getHeight(), labelH * 2)));

        ratioSlider.setTextBoxStyle  (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        detuneSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        phaseSlider.setTextBoxStyle  (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        foldSlider.setTextBoxStyle   (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
    }

    void lookAndFeelChanged() override
    {
        juce::Component::lookAndFeelChanged();

        // Update Labels
        opHeaderLabel.setColour (juce::Label::textColourId, colors.text);
        ratioLabel.setColour    (juce::Label::textColourId, colors.text);
        detuneLabel.setColour   (juce::Label::textColourId, colors.text);
        phaseLabel.setColour    (juce::Label::textColourId, colors.text);
        foldLabel.setColour     (juce::Label::textColourId, colors.text);
        envSourceLabel.setColour (juce::Label::textColourId, colors.text);

        // Helper lambda to update ComboBoxes cleanly
        auto updateComboBox = [this](juce::ComboBox& cb) {
            cb.setColour (juce::ComboBox::backgroundColourId, colors.surface);
            cb.setColour (juce::ComboBox::textColourId, colors.text);
            cb.sendLookAndFeelChange();
        };

        updateComboBox (modeSelector);
        updateComboBox (waveShapeSelector);
        updateComboBox (effectTypeSelector);
        updateComboBox (envSourceSelector);

        // Helper lambda to update Sliders cleanly
        auto updateSlider = [this](juce::Slider& s) {
            s.setColour (juce::Slider::textBoxBackgroundColourId, colors.surface);
            s.setColour (juce::Slider::textBoxTextColourId, colors.text);
            s.sendLookAndFeelChange();
        };

        updateSlider (ratioSlider);
        updateSlider (detuneSlider);
        updateSlider (phaseSlider);
        updateSlider (foldSlider);
    }

    // Callback fired when the user picks a file. Receives (opIndex, file).
    std::function<void(int, juce::File)> onLoadSample;

    void setSampleButtonText (const juce::String& name)
    {
        loadSampleButton.setButtonText (name.isNotEmpty() ? name : "Load Sample");
    }

private:
    // Combined logic method avoids layout text fighting
    OAOColors& colors;
    void updateUIState()
    {
        int selectedMode   = modeSelector.getSelectedId();
        bool isWaveMode    = (selectedMode == 1);
        bool isAdditiveMode = (selectedMode == 2);
        bool isSampleMode = (selectedMode == 3);
	bool isEffectMode  = (selectedMode == 4);
	int freqMode        = freqModeSelector.getSelectedId();
        bool isStd          = (freqMode == 1);
        bool isSync         = (freqMode == 2);
        bool isHz           = (freqMode == 3);
        bool isLFO          = (freqMode == 4);
        int selectedEffect = effectTypeSelector.getSelectedId();
        waveShapeSelector.setVisible (isWaveMode);
        effectTypeSelector.setVisible (isEffectMode);
        loadSampleButton.setVisible (isSampleMode);
        phaseSlider.setVisible (true);
        phaseLabel.setVisible  (true);

        // Swap freqMode selector items depending on whether we're in sample mode or not
        if (isSampleMode)
        {
            if (freqModeSelector.getItemText (0) != "One-shot")
            {
                int savedId = freqModeSelector.getSelectedId();
                freqModeSelector.clear (juce::dontSendNotification);
                freqModeSelector.addItemList ({ "One-shot", "Loop", "Ping-pong", "Stutter" }, 1);
                freqModeSelector.setSelectedId (savedId > 0 ? savedId : 1, juce::dontSendNotification);
            }
        }
        else
        {
            if (freqModeSelector.getItemText (0) != "Std")
            {
                int savedId = freqModeSelector.getSelectedId();
                freqModeSelector.clear (juce::dontSendNotification);
                freqModeSelector.addItemList ({ "Std", "Sync", "Hz", "LFO" }, 1);
                freqModeSelector.setSelectedId (savedId > 0 ? savedId : 1, juce::dontSendNotification);
            }
            // Re-read after any potential restore
            freqMode   = freqModeSelector.getSelectedId();
            isStd      = (freqMode == 1);
            isSync     = (freqMode == 2);
            isHz       = (freqMode == 3);
            isLFO      = (freqMode == 4);
        }

        // Knob labels covering all four modes
        if (isSampleMode)
        {
            ratioLabel.setText  ("Speed", juce::dontSendNotification);
            detuneLabel.setText ("Start", juce::dontSendNotification);
            phaseLabel.setText  ("End",   juce::dontSendNotification);
            foldLabel.setText   ("Boundary",  juce::dontSendNotification);
            ratioSlider.setTextValueSuffix ("");
        }
        else if (isAdditiveMode)
        {
            ratioLabel.setText (isHz ? "Freq" : (isSync ? "Sync Rate" : "Ratio"), juce::dontSendNotification);
            ratioSlider.setTextValueSuffix (isHz ? " Hz" : (isSync ? "x" : ""));
            detuneLabel.setText ("Tilt",     juce::dontSendNotification);
            phaseLabel.setText  ("Stretch",  juce::dontSendNotification);
            foldLabel.setText   ("Odd/Even", juce::dontSendNotification);
        }
        else if (isEffectMode)
        {
            auto labels = ProjectConfig::getEffectKnobLabels (selectedEffect - 1);
            ratioLabel.setText  (labels[0], juce::dontSendNotification);
            detuneLabel.setText (labels[1], juce::dontSendNotification);
            phaseLabel.setText  (labels[2], juce::dontSendNotification);
            foldLabel.setText   (labels[3], juce::dontSendNotification);
        }
        else // Wave mode
        {
            bool isPWM = (waveShapeSelector.getSelectedId() == 5 ||
                          waveShapeSelector.getSelectedId() == 6);
            if      (isHz)   ratioLabel.setText ("Freq x1000", juce::dontSendNotification);
            else if (isLFO)  ratioLabel.setText ("LFO Rate",   juce::dontSendNotification);
            else if (isSync) ratioLabel.setText ("Sync Rate",  juce::dontSendNotification);
            else             ratioLabel.setText ("Ratio",      juce::dontSendNotification);

            detuneLabel.setText ("Detune",                juce::dontSendNotification);
            phaseLabel.setText  (isPWM ? "PWM" : "Phase", juce::dontSendNotification);
            foldLabel.setText   ("Fold",                  juce::dontSendNotification);

            if      (isHz)   ratioSlider.setTextValueSuffix (" Hz");
            else if (isLFO)  ratioSlider.setTextValueSuffix (" Hz");
            else if (isSync) ratioSlider.setTextValueSuffix ("x");
            else             ratioSlider.setTextValueSuffix ("");
        }
        if (getWidth() > 0 && getHeight() > 0)
        resized();
    }

    // Keep state reference completely safe inside class lifecycle
    juce::AudioProcessorValueTreeState& apvts;
    juce::String opNum;

    juce::Slider ratioSlider, detuneSlider, phaseSlider, foldSlider;
    
    juce::Label opHeaderLabel;
    juce::Label ratioLabel, detuneLabel, phaseLabel, foldLabel;
    juce::Label envSourceLabel;
    
    juce::ComboBox modeSelector;
    juce::ComboBox waveShapeSelector;
    juce::ComboBox effectTypeSelector;
    juce::ComboBox freqModeSelector;
    juce::ComboBox envSourceSelector; // which shared envelope generator drives this operator
    juce::TextButton loadSampleButton;
    std::unique_ptr<juce::FileChooser> fileChooser;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveShapeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> effectTypeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> freqModeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> envSourceAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ratioAttach, detuneAttach, phaseAttach, foldAttach;

public:
    // Set from OperatorsPage::resized()
    void setSharedKnobTarget (int targetDiameter)
    {
        if (sharedKnobTarget != targetDiameter)
        {
            sharedKnobTarget = targetDiameter;
            resized();
        }
    }
private:
    int sharedKnobTarget = 90; //default, overwritten in page
};

// One macro slot that drives up to 4 independent targets at once, each weighted by its own amount knob
struct MacroSlot : public juce::Component
{
    MacroSlot (juce::AudioProcessorValueTreeState& apvts, int macroIndex, OAOColors& c)
        : colors (c)
    {
        juce::String s = juce::String (macroIndex + 1);
        static const char* letters[] = { "A", "B", "C", "D" };

        macroSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        macroSlider.setRange (-1.0, 1.0, 0.001);
        macroSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 14);
        addAndMakeVisible (macroSlider);

        macroLabel.setText ("Macro " + s, juce::dontSendNotification);
        macroLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (macroLabel);

        valAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, "MACRO_VAL_" + s, macroSlider);

        for (int t = 0; t < ProjectConfig::numMacroTargets; ++t)
        {
            juce::String letter = letters[t];

            addAndMakeVisible (targetSelector[t]);
            ModChoices::buildTargetMenu (targetSelector[t]);
            targetAttach[t] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                apvts, "MACRO_TGT_" + letter + "_" + s, targetSelector[t]);

            amountSlider[t].setSliderStyle (juce::Slider::LinearHorizontal);
            amountSlider[t].setRange (-1.0, 1.0, 0.001);
            addAndMakeVisible (amountSlider[t]);
            amountAttach[t] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, "MACRO_AMT_" + letter + "_" + s, amountSlider[t]);

            targetLabel[t].setText (letter, juce::dontSendNotification);
            targetLabel[t].setJustificationType (juce::Justification::centred);
            addAndMakeVisible (targetLabel[t]);
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (colors.background);
        g.fillRoundedRectangle (bounds, 4.0f);

        g.setColour (colors.text.withAlpha (0.15f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 4.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (juce::roundToInt (getWidth() * 0.02f),
                                               juce::roundToInt (getHeight() * 0.04f));

        // Knob on the left, 4 stacked [target dropdown | amount knob] rows on the right
        int knobW = juce::jmin (area.getWidth() / 4, area.getHeight());
        auto knobArea = area.removeFromLeft (knobW);

        int macroLabelH = juce::jmax (12, juce::roundToInt (getHeight() * 0.14f));
        macroLabel.setBounds (knobArea.removeFromTop (macroLabelH));
        macroSlider.setBounds (knobArea);

        area.removeFromLeft (juce::roundToInt (getWidth() * 0.02f)); // gap

        int numTargets = ProjectConfig::numMacroTargets;
        int rowH = area.getHeight() / numTargets;
        int tagW = juce::jmax (18, juce::roundToInt (area.getWidth() * 0.06f));
        int amtTextW = juce::jmax (32, juce::roundToInt (area.getWidth() * 0.13f));
        int amtW = juce::jmax (amtTextW * 2, juce::roundToInt (area.getWidth() * 0.38f));

        for (int t = 0; t < numTargets; ++t)
        {
            bool isLast = (t == numTargets - 1);
            auto row = area.removeFromTop (isLast ? area.getHeight() : rowH);

            targetLabel[t].setBounds (row.removeFromLeft (tagW));

            auto amtArea = row.removeFromRight (amtW).reduced (2, juce::roundToInt (rowH * 0.12f));
            amountSlider[t].setTextBoxStyle (juce::Slider::TextBoxRight, false, amtTextW, amtArea.getHeight());
            amountSlider[t].setBounds (amtArea);

            targetSelector[t].setBounds (row.reduced (2, juce::roundToInt (rowH * 0.12f)));
        }
    }

    void lookAndFeelChanged() override
    {
        juce::Component::lookAndFeelChanged();

        macroLabel.setColour (juce::Label::textColourId, colors.text);
        macroSlider.setColour (juce::Slider::textBoxBackgroundColourId, colors.surface);
        macroSlider.setColour (juce::Slider::textBoxTextColourId, colors.text);
        macroSlider.sendLookAndFeelChange();

        for (int t = 0; t < ProjectConfig::numMacroTargets; ++t)
        {
            targetLabel[t].setColour (juce::Label::textColourId, colors.text);
            targetSelector[t].setColour (juce::ComboBox::backgroundColourId, colors.surface);
            targetSelector[t].setColour (juce::ComboBox::textColourId, colors.text);
            amountSlider[t].setColour (juce::Slider::textBoxBackgroundColourId, colors.surface);
            amountSlider[t].setColour (juce::Slider::textBoxTextColourId, colors.text);

            targetLabel[t].sendLookAndFeelChange();
            targetSelector[t].sendLookAndFeelChange();
            amountSlider[t].sendLookAndFeelChange();
        }
    }

private:
    OAOColors& colors;
    juce::Slider   macroSlider;
    juce::Label    macroLabel;

    juce::ComboBox targetSelector[ProjectConfig::numMacroTargets];
    juce::Slider   amountSlider[ProjectConfig::numMacroTargets];
    juce::Label    targetLabel[ProjectConfig::numMacroTargets];

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   valAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAttach[ProjectConfig::numMacroTargets];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   amountAttach[ProjectConfig::numMacroTargets];
};

// One shared envelope generator (attack/decay/sustain/release) that operators can pick
// via their own "Env" source selector (see CompactOperatorGroup::envSourceSelector).
struct EnvelopeSlot : public juce::Component
{
    EnvelopeSlot (juce::AudioProcessorValueTreeState& apvts, int envIndex, OAOColors& c)
        : colors (c)
    {
        juce::String s = juce::String (envIndex + 1);

        envLabel.setText ("Env " + s, juce::dontSendNotification);
        envLabel.setJustificationType (juce::Justification::centred);
        envLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        addAndMakeVisible (envLabel);

        static const char* names[] = { "A", "D", "S", "R" };
        juce::Slider::SliderStyle style = juce::Slider::LinearHorizontal;
        for (int k = 0; k < 4; ++k)
        {
            knob[k].setSliderStyle (style);
            knob[k].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 12);
            addAndMakeVisible (knob[k]);

            knobLabel[k].setText (names[k], juce::dontSendNotification);
            knobLabel[k].setJustificationType (juce::Justification::centred);
            addAndMakeVisible (knobLabel[k]);
        }

        attackAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "ENV_ATTACK_"  + s, knob[0]);
        decayAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "ENV_DECAY_"   + s, knob[1]);
        sustainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "ENV_SUSTAIN_" + s, knob[2]);
        releaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "ENV_RELEASE_" + s, knob[3]);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (colors.background);
        g.fillRoundedRectangle (bounds, 4.0f);

        g.setColour (colors.text.withAlpha (0.15f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 4.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (juce::roundToInt (getWidth() * 0.02f),
                                               juce::roundToInt (getHeight() * 0.04f));

        int labelH = juce::jmax (12, juce::roundToInt (getHeight() * 0.16f));
        envLabel.setBounds (area.removeFromTop (labelH));

        int knobW = area.getWidth() / 4;
        for (int k = 0; k < 4; ++k)
        {
            auto col = area.removeFromLeft (knobW);
            knobLabel[k].setBounds (col.removeFromTop (labelH));
            knob[k].setBounds (col.reduced (2, 0));
        }
    }

    void lookAndFeelChanged() override
    {
        juce::Component::lookAndFeelChanged();

        envLabel.setColour (juce::Label::textColourId, colors.text);
        for (int k = 0; k < 4; ++k)
        {
            knobLabel[k].setColour (juce::Label::textColourId, colors.text);
            knob[k].setColour (juce::Slider::textBoxBackgroundColourId, colors.surface);
            knob[k].setColour (juce::Slider::textBoxTextColourId, colors.text);
            knobLabel[k].sendLookAndFeelChange();
            knob[k].sendLookAndFeelChange();
        }
    }

private:
    OAOColors& colors;
    juce::Label  envLabel;
    juce::Slider knob[4];       // Attack, Decay, Sustain, Release
    juce::Label  knobLabel[4];

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach, decayAttach, sustainAttach, releaseAttach;
};

// --- THE PARENT VIEW MANAGER CLASS ---
class OperatorsPage : public juce::Component
{
public:
    // Callback fired when any operator's Load Sample button is used.
    // Receives (opIndex 0-based, file).
    std::function<void(int, juce::File)> onLoadSample;

    // Called after preset load to update the Load button text to the restored sample name.
    void setSampleButtonText (int opIndex, const juce::String& name)
    {
        if (opIndex >= 0 && opIndex < (int) opModules.size())
            opModules[opIndex]->setSampleButtonText (name);
    }

    OperatorsPage (juce::AudioProcessorValueTreeState& apvts, OAOColors& c) : colors (c)
    {
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            opModules.push_back (std::make_unique<CompactOperatorGroup> (apvts, i, colors));
            // Forward load-sample events up to whoever owns the OperatorsPage
            opModules.back()->onLoadSample = [this] (int opIdx, juce::File file)
            {
                if (onLoadSample)
                    onLoadSample (opIdx, file);
            };
            addAndMakeVisible (*opModules.back());
        }

        for (int i = 0; i < ProjectConfig::numMacros; ++i)
        {
            macroSlots.push_back (std::make_unique<MacroSlot> (apvts, i, colors));
            addAndMakeVisible (*macroSlots.back());
        }

        for (int i = 0; i < ProjectConfig::numEnvelopes; ++i)
        {
            envelopeSlots.push_back (std::make_unique<EnvelopeSlot> (apvts, i, colors));
            addAndMakeVisible (*envelopeSlots.back());
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colors.panelGap);
    }

    void lookAndFeelChanged() override
    {
        juce::Component::lookAndFeelChanged();

        for (auto& module : opModules)
        {
            if (module != nullptr)
                module->lookAndFeelChanged();
        }

        for (auto& macro : macroSlots)
        {
            if (macro != nullptr)
                macro->lookAndFeelChanged();
        }

        for (auto& env : envelopeSlots)
        {
            if (env != nullptr)
                env->lookAndFeelChanged();
        }
    }

    void repaintAll()
    {
        for (auto& op : opModules)
            op->repaint();
        for (auto& macro : macroSlots)
            macro->repaint();
        for (auto& env : envelopeSlots)
            env->repaint();
        repaint();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (
            juce::roundToInt (getWidth()  * ProjectConfig::outerMargin),
            juce::roundToInt (getHeight() * ProjectConfig::outerMargin));

        int gap = juce::jmax (2, juce::roundToInt (getHeight() * 0.006f));

        // --- Macro row: fixed height across the top, one column per macro ---
        int macroRowHeight = juce::roundToInt (area.getHeight() * 0.15f);
        auto macroRowArea  = area.removeFromTop (macroRowHeight);
        area.removeFromTop (gap);

        int numMacros  = static_cast<int> (macroSlots.size());
        if (numMacros > 0)
        {
            int macroCellW = macroRowArea.getWidth() / numMacros;
            int macroGap   = juce::jmax (1, gap / 2);
            for (int i = 0; i < numMacros; ++i)
            {
                bool isLast = (i == numMacros - 1);
                auto cell = macroRowArea.removeFromLeft (isLast ? macroRowArea.getWidth() : macroCellW);
                if (! isLast) macroRowArea.removeFromLeft (macroGap);
                macroSlots[i]->setBounds (cell);
            }
        }

        // --- Envelope row: fixed height beneath the macros, one column per shared envelope ---
        int envRowHeight = juce::roundToInt (area.getHeight() * 0.12f);
        auto envRowArea  = area.removeFromTop (envRowHeight);
        area.removeFromTop (gap);

        int numEnvs = static_cast<int> (envelopeSlots.size());
        if (numEnvs > 0)
        {
            int envCellW = envRowArea.getWidth() / numEnvs;
            int envGap   = juce::jmax (1, gap / 2);
            for (int i = 0; i < numEnvs; ++i)
            {
                bool isLast = (i == numEnvs - 1);
                auto cell = envRowArea.removeFromLeft (isLast ? envRowArea.getWidth() : envCellW);
                if (! isLast) envRowArea.removeFromLeft (envGap);
                envelopeSlots[i]->setBounds (cell);
            }
        }

        int rows = ProjectConfig::numOperators;
        int cols = 1;
        int halfGap = juce::jmax (1, gap / 2);
        int cellWidth = area.getWidth() / cols;
        int cellHeight = area.getHeight() / rows;

        // Computed from the true page dimensions (this component, not a per-row card)
        int sharedKnobTarget = juce::roundToInt (
            juce::jmin (getWidth(), getHeight()) * colors.knobDiameterFraction);
	float sharedFontSize = static_cast<float>(cellHeight) * 0.4f;
        // Walk down to avoid weird sizing issues
	auto remaining = area;
        for (int r = 0; r < rows; ++r)
        {
            auto rowBounds = remaining.removeFromTop (cellHeight);

            for (int c = 0; c < cols; ++c)
            {
                int index = (r * cols) + c;
                if (index < static_cast<int>(opModules.size()))
                {
                    auto cellBounds = rowBounds.withX (rowBounds.getX() + c * cellWidth).withWidth (cellWidth);

                    // Only inset the sides that actually face a neighboring card
                    // With cols == 1 there's never a horizontal neighbor, so no horizontal inset at all.
                    // testing setting these to gap and not halGap
		    int top    = halfGap;
                    int bottom = halfGap;
                    cellBounds.removeFromTop (top);
                    cellBounds.removeFromBottom (bottom);

                    // Must be set before setBounds
                    opModules[index]->setSharedKnobTarget (sharedKnobTarget);
                    opModules[index]->setBounds (cellBounds);
                }
            }
        }
    }

private:
    OAOColors& colors;
    std::vector<std::unique_ptr<CompactOperatorGroup>> opModules;
    std::vector<std::unique_ptr<MacroSlot>> macroSlots;
    std::vector<std::unique_ptr<EnvelopeSlot>> envelopeSlots;
};
