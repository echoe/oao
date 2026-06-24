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
        auto setupSlider = [this] (juce::Slider& slider, juce::Label& label, const juce::String& text, bool rotary)
        {
            slider.setSliderStyle (rotary ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearVertical);
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
        setupSlider (attackSlider, attackLabel, "Attack", false);
        setupSlider (decaySlider, decayLabel, "Decay", false);
        setupSlider (sustainSlider, sustainLabel, "Sustain", false);
        setupSlider (releaseSlider, releaseLabel, "Release", false);
        // label operator
        opHeaderLabel.setText ("OPERATOR " + opNum, juce::dontSendNotification);
        opHeaderLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
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
        attackAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "ATTACK_" + opNum, attackSlider);
        decayAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "DECAY_" + opNum, decaySlider);
        sustainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "SUSTAIN_" + opNum, sustainSlider);
        releaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "RELEASE_" + opNum, releaseSlider);
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

        // --- LEFT COLUMN: header label + 3 stacked selectors ---
        int leftColW  = juce::jmax (90, juce::roundToInt (w * 0.16f));
        auto leftCol  = area.removeFromLeft (leftColW);

        opHeaderLabel.setBounds (leftCol.removeFromTop (juce::jmax (14, juce::roundToInt (h * 0.16f))));

        int selectorH = juce::jmax (16, juce::roundToInt (h * 0.24f));
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
        int Width = area.getWidth() / 8;

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
    
        // --- ENVELOPE SLIDERS ---
    
        auto aArea = area.removeFromLeft (Width);
        attackLabel.setBounds  (aArea.removeFromTop (labelH));
        attackSlider.setBounds (clampKnob (aArea));
    
        auto decArea = area.removeFromLeft (Width);
        decayLabel.setBounds  (decArea.removeFromTop (labelH));
        decaySlider.setBounds (clampKnob (decArea));
    
        auto sArea = area.removeFromLeft (Width);
        sustainLabel.setBounds  (sArea.removeFromTop (labelH));
        sustainSlider.setBounds (clampKnob (sArea));
    
        auto relArea = area;
        releaseLabel.setBounds  (relArea.removeFromTop (labelH));
        releaseSlider.setBounds (clampKnob (relArea));

        ratioSlider.setTextBoxStyle  (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        detuneSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        phaseSlider.setTextBoxStyle  (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        foldSlider.setTextBoxStyle   (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        attackSlider.setTextBoxStyle  (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        decaySlider.setTextBoxStyle   (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        sustainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        releaseSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);    	
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
        attackLabel.setColour   (juce::Label::textColourId, colors.text);
        decayLabel.setColour    (juce::Label::textColourId, colors.text);
        sustainLabel.setColour  (juce::Label::textColourId, colors.text);
        releaseLabel.setColour  (juce::Label::textColourId, colors.text);

        // Helper lambda to update ComboBoxes cleanly
        auto updateComboBox = [this](juce::ComboBox& cb) {
            cb.setColour (juce::ComboBox::backgroundColourId, colors.surface);
            cb.setColour (juce::ComboBox::textColourId, colors.text);
            cb.sendLookAndFeelChange();
        };

        updateComboBox (modeSelector);
        updateComboBox (waveShapeSelector);
        updateComboBox (effectTypeSelector);

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
        updateSlider (attackSlider);
        updateSlider (decaySlider);
        updateSlider (sustainSlider);
        updateSlider (releaseSlider);
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
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    
    juce::Label opHeaderLabel;
    juce::Label ratioLabel, detuneLabel, phaseLabel, foldLabel;
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel;
    
    juce::ComboBox modeSelector;
    juce::ComboBox waveShapeSelector;
    juce::ComboBox effectTypeSelector;
    juce::ComboBox freqModeSelector;
    juce::TextButton loadSampleButton;
    std::unique_ptr<juce::FileChooser> fileChooser;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveShapeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> effectTypeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> freqModeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ratioAttach, detuneAttach, phaseAttach, foldAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach, decayAttach, sustainAttach, releaseAttach;

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
    }

    void repaintAll()
    {
        for (auto& op : opModules)
            op->repaint();
        repaint();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (
            juce::roundToInt (getWidth()  * ProjectConfig::outerMargin),
            juce::roundToInt (getHeight() * ProjectConfig::outerMargin));

        int rows = 6;
        int cols = 1;
        int gap     = juce::jmax (2, juce::roundToInt (getHeight() * 0.006f));
        int halfGap = juce::jmax (1, gap / 2);
        int cellWidth = area.getWidth() / cols;
        int cellHeight = area.getHeight() / rows;

        // Computed from the true page dimensions (this component, not a per-row card)
        int sharedKnobTarget = juce::roundToInt (
            juce::jmin (getWidth(), getHeight()) * colors.knobDiameterFraction);
        // Walk down to avoid weird sizing issues
	auto remaining = area;
        for (int r = 0; r < rows; ++r)
        {
            bool isLastRow = (r == rows - 1);
            auto rowBounds = remaining.removeFromTop (isLastRow ? remaining.getHeight() : cellHeight);

            for (int c = 0; c < cols; ++c)
            {
                int index = (r * cols) + c;
                if (index < static_cast<int>(opModules.size()))
                {
                    auto cellBounds = rowBounds.withX (rowBounds.getX() + c * cellWidth).withWidth (cellWidth);

                    // Only inset the sides that actually face a neighboring card
                    // With cols == 1 there's never a horizontal neighbor, so no horizontal inset at all.
                    int top    = (r == 0)       ? 0 : halfGap;
                    int bottom = isLastRow       ? 0 : halfGap;
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
};
