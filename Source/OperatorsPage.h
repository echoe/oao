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

        // FM Inputs — up to numFMInputSlots incoming connections, shown compactly on the
        // operator's own card instead of a global matrix page. Each slot is a thin wrapper
        // around the same underlying MOD_src_dest params the (now-optional/advanced) Matrix
        // page edits directly, so nothing here changes the data model or breaks presets.
        for (int slot = 0; slot < numFMInputSlots; ++slot)
        {
            fmInputSource[slot].addItem ("--", 1);
            for (int src = 0; src < ProjectConfig::numOperators; ++src)
                fmInputSource[slot].addItem ("Op " + juce::String (src + 1), src + 2);
            addAndMakeVisible (fmInputSource[slot]);

            fmInputAmount[slot].setSliderStyle (juce::Slider::LinearHorizontal);
            fmInputAmount[slot].setTextBoxStyle (juce::Slider::TextBoxRight, false, 34, 14);
            addAndMakeVisible (fmInputAmount[slot]);

            fmInputSource[slot].onChange = [this, slot] { onFMInputSourceChanged (slot); };
        }

        // Audio Route — two independent destination slots, so an operator can feed two
        // places at once (e.g. main mix + another operator, or two other operators).
        // Column headers ("Out" / "Out2") now live in the page-level header bar above the
        // operator list, so no per-row label is needed here.
        for (int slot = 0; slot < numAudioOutSlots; ++slot)
        {
            audioOutTarget[slot].addItem ("None", 1);
            audioOutTarget[slot].addItem ("Out", 2);
            for (int op = 0; op < ProjectConfig::numOperators; ++op)
            {
                if (op == opIndex) continue; // no self audio routing here — self FM feedback already covers that case
                audioOutTarget[slot].addItem ("Op " + juce::String (op + 1), 3 + op);
            }
            addAndMakeVisible (audioOutTarget[slot]);
            audioOutTarget[slot].onChange = [this, slot] { onAudioOutTargetChanged (slot); };

            audioOutAmount[slot].setSliderStyle (juce::Slider::LinearHorizontal);
            audioOutAmount[slot].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 36, 12);
            addAndMakeVisible (audioOutAmount[slot]);
        }
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

        refreshFMInputsFromState(); // populate FM input slots from any existing MOD_ connections
        refreshAudioOutFromState(); // populate Audio Out from any existing OUT_/AUDIO_ROUTE_ state
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (colors.background);
        g.fillRoundedRectangle (bounds, 4.0f);

        g.setColour (colors.text.withAlpha (0.15f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 4.0f, 1.0f);

        // Thin dividers marking the 5 sections: (1) op num + selectors, (2) tweak knobs,
        // (3) envelope, (4) FM inputs, (5) audio out. Positions are computed in resized().
        g.setColour (colors.text.withAlpha (0.08f));
        float dTop = bounds.getY() + 4.0f, dBottom = bounds.getBottom() - 4.0f;
        for (int dx : { section1Divider, section2Divider, section3Divider, section5Divider })
            if (dx > 0)
                g.drawVerticalLine (dx, dTop, dBottom);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (static_cast<int> (getWidth() * 0.02f),
                                              static_cast<int> (getHeight() * 0.02f));
        float w          = static_cast<float> (area.getWidth());
        float h          = static_cast<float> (area.getHeight());

        // Shared text box / label sizing (used by every knob-style slider in sections 2 and 4)
        int textBoxW = juce::roundToInt (sharedKnobTarget * ProjectConfig::textBoxWidthFraction);
        int textBoxH = juce::jlimit (12, 70, juce::roundToInt (sharedKnobTarget * colors.textBoxHeightFraction));
        int labelH   = juce::jmax (10, juce::roundToInt (sharedKnobTarget * colors.textBoxHeightFraction));
        int gap      = juce::jmax (4, juce::roundToInt (w * 0.018f));

        // ============================================================
        // SECTION 1 — operator number + Mode/Wave/Effect/FreqMode selectors
        // ============================================================
        int opNumW = juce::jmax (70, juce::roundToInt (w * 0.03f));
        auto opNum = area.removeFromLeft (opNumW);
        opHeaderLabel.setBounds (opNum);
        int leftColW  = juce::jmax (70, juce::roundToInt (w * 0.13f));
        auto leftCol  = area.removeFromLeft (leftColW);
        section1Divider = area.getX();

        int selectorH = juce::jmax (16, juce::roundToInt (h * 0.33f));
        freqModeSelector.setBounds (leftCol.removeFromTop (selectorH).reduced (1));
        modeSelector.setBounds     (leftCol.removeFromTop (selectorH).reduced (1));

        if (effectTypeSelector.isVisible())
            effectTypeSelector.setBounds (leftCol.removeFromTop (selectorH).reduced (1));
        else if (loadSampleButton.isVisible())
            loadSampleButton.setBounds (leftCol.removeFromTop (selectorH).reduced (1));
        else
            waveShapeSelector.setBounds (leftCol.removeFromTop (selectorH).reduced (1));

        area.removeFromLeft (gap);

        // ============================================================
        // SECTION 5 — Audio Route: two sub-columns, "Out" and "Out2" (carved from the right
        // first, fixed width, so section 4 gets whatever's left over — see below)
        // ============================================================
        int audioColW = juce::jmax (70, juce::roundToInt (w * 0.065f)) * numAudioOutSlots;
        auto audioArea = area.removeFromRight (audioColW);
        area.removeFromRight (gap);
        section5Divider = area.getRight();

        // ============================================================
        // SECTION 2 — the four operator tweak knobs (Ratio/Detune/Phase/Fold)
        // ============================================================
        int knobColW = juce::jmax (sharedKnobTarget + 4, juce::roundToInt (w * 0.058f));

        int targetBoxSize = sharedKnobTarget + 8;
        int knobAreaH     = area.getHeight() - labelH;
        int knobBoxW      = juce::jmin (knobColW, targetBoxSize);
        int knobBoxH      = juce::jmin (knobAreaH, targetBoxSize + textBoxH);

        auto clampKnob = [knobBoxW, knobBoxH] (juce::Rectangle<int> box)
        {
            return box.withSizeKeepingCentre (knobBoxW, knobBoxH);
        };

        auto rArea = area.removeFromLeft (knobColW);
        ratioLabel.setBounds  (rArea.removeFromTop (labelH));
        ratioSlider.setBounds (clampKnob (rArea));

        auto dArea = area.removeFromLeft (knobColW);
        detuneLabel.setBounds  (dArea.removeFromTop (labelH));
        detuneSlider.setBounds (clampKnob (dArea));

        auto pArea = area.removeFromLeft (knobColW);
        phaseLabel.setBounds  (pArea.removeFromTop (labelH));
        phaseSlider.setBounds (clampKnob (pArea));

        auto lArea = area.removeFromLeft (knobColW);
        foldLabel.setBounds  (lArea.removeFromTop (labelH));
        foldSlider.setBounds (clampKnob (lArea));

        ratioSlider.setTextBoxStyle  (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        detuneSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        phaseSlider.setTextBoxStyle  (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        foldSlider.setTextBoxStyle   (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);

        area.removeFromLeft (gap);
        section2Divider = area.getX();

        // ============================================================
        // SECTION 3 — envelope source (deliberately smaller than a knob column: it's just
        // a dropdown, not a value to dial in)
        // ============================================================
        int envColW = juce::jmax (50, juce::roundToInt (w * 0.05f));
        auto envArea = area.removeFromLeft (envColW);
        envSourceLabel.setBounds (envArea.removeFromTop (labelH));
        envSourceSelector.setBounds (envArea.reduced (2).withSizeKeepingCentre (
            envArea.getWidth(), juce::jmin (envArea.getHeight(), labelH * 2)));

        area.removeFromLeft (gap);
        section3Divider = area.getX();

        // ============================================================
        // SECTION 4 — FM Inputs: whatever's left after the other four sections, which is
        // deliberately most of the row. Laid out the same way as the section 2 knobs (label/
        // selector on top, slider below). Slightly smaller than a full tweak knob, with a
        // visible gap between slots so they don't run together.
        // ============================================================
        auto fmInputArea = area;
        int fmGap  = juce::jmax (3, gap / 2);
        int fmSlotW = (fmInputArea.getWidth() - fmGap * (numFMInputSlots - 1)) / numFMInputSlots;
        int fmComboH = juce::jmax (16, labelH + 4);
        int fmKnobBoxW = juce::jmin (knobBoxW, juce::roundToInt (knobBoxW * 0.82f));
        int fmKnobBoxH = juce::jmin (knobBoxH, juce::roundToInt (knobBoxH * 0.9f));
        auto clampFmKnob = [fmKnobBoxW, fmKnobBoxH] (juce::Rectangle<int> box)
        {
            return box.withSizeKeepingCentre (fmKnobBoxW, fmKnobBoxH);
        };

        for (int slot = 0; slot < numFMInputSlots; ++slot)
        {
            auto col = fmInputArea.removeFromLeft (fmSlotW);
            if (slot < numFMInputSlots - 1)
                fmInputArea.removeFromLeft (fmGap);
            fmInputSource[slot].setBounds (col.removeFromTop (fmComboH));
            col.removeFromTop (2);
            fmInputAmount[slot].setBounds (clampFmKnob (col));
            fmInputAmount[slot].setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        }

        // ============================================================
        // SECTION 5 (laid out) — Out / Out2: destination combo + amount slider, stacked,
        // side by side. No per-slot header text — the page-level header bar labels these.
        // ============================================================
        int audioSlotW = audioArea.getWidth() / numAudioOutSlots;
        for (int slot = 0; slot < numAudioOutSlots; ++slot)
        {
            auto aArea = audioArea.removeFromLeft (audioSlotW).reduced (2, 0);
            audioOutTarget[slot].setBounds (aArea.removeFromTop (fmComboH));
            aArea.removeFromTop (2);
            audioOutAmount[slot].setBounds (aArea);
            audioOutAmount[slot].setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        }
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
        for (auto& cb : audioOutTarget)
            updateComboBox (cb);
        for (auto& cb : fmInputSource)
            updateComboBox (cb);

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
        for (auto& s : fmInputAmount)
            updateSlider (s);
        for (auto& s : audioOutAmount)
            updateSlider (s);
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

public:
    // Re-scan this operator's incoming MOD_src_dest cells and rebind the FM Input slots to
    // whichever are currently non-zero. Needed after anything that writes those params directly
    // instead of through a slot's own combo box — a preset load, or the Algorithm quick-select.
    void refreshFMInputsFromState()
    {
        int destIdx = opNum.getIntValue() - 1;

        for (int slot = 0; slot < numFMInputSlots; ++slot)
        {
            fmInputAmountAttach[slot].reset();
            fmInputBoundSrc[slot] = -1;
        }

        int slot = 0;
        for (int src = 0; src < ProjectConfig::numOperators && slot < numFMInputSlots; ++src)
        {
            auto* raw = apvts.getRawParameterValue ("MOD_" + juce::String (src) + "_" + juce::String (destIdx));
            if (raw != nullptr && raw->load (std::memory_order_relaxed) > 0.0001f)
            {
                fmInputSource[slot].setSelectedId (src + 2, juce::dontSendNotification);
                fmInputAmountAttach[slot] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    apvts, "MOD_" + juce::String (src) + "_" + juce::String (destIdx), fmInputAmount[slot]);
                fmInputAmount[slot].setEnabled (true);
                fmInputBoundSrc[slot] = src;
                ++slot;
            }
        }
        for (; slot < numFMInputSlots; ++slot)
        {
            fmInputSource[slot].setSelectedId (1, juce::dontSendNotification);
            fmInputAmount[slot].setEnabled (false);
            fmInputAmount[slot].setValue (0.0, juce::dontSendNotification);
        }
    }

    // Called when a slot's source dropdown changes — rebinds the amount slider to the newly
    // chosen MOD_src_dest cell, and zeroes out the cell the slot is leaving so it doesn't
    // linger as an invisible active connection nobody can see in this compact view.
    void onFMInputSourceChanged (int slot)
    {
        int destIdx  = opNum.getIntValue() - 1;
        int selId    = fmInputSource[slot].getSelectedId();
        int newSrc   = (selId <= 1) ? -1 : (selId - 2);
        int oldSrc   = fmInputBoundSrc[slot];

        if (oldSrc != -1 && oldSrc != newSrc)
        {
            if (auto* p = apvts.getParameter ("MOD_" + juce::String (oldSrc) + "_" + juce::String (destIdx)))
                p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
        }

        fmInputAmountAttach[slot].reset();

        if (newSrc != -1)
        {
            fmInputAmountAttach[slot] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, "MOD_" + juce::String (newSrc) + "_" + juce::String (destIdx), fmInputAmount[slot]);
            fmInputAmount[slot].setEnabled (true);
        }
        else
        {
            fmInputAmount[slot].setEnabled (false);
            fmInputAmount[slot].setValue (0.0, juce::dontSendNotification);
        }

        fmInputBoundSrc[slot] = newSrc;
    }

    // Re-scan this operator's OUT_ level and outgoing AUDIO_ROUTE_ cells, and set the Audio Out
    // control to whichever is currently active. If both happen to be active at once (possible if
    // someone used the old Audio Matrix page directly, or both slots here happen to point at
    // the same destination), only the first numAudioOutSlots found are shown; that's a real
    // limit of this compact view, not a bug.
    void refreshAudioOutFromState()
    {
        int srcIdx = opNum.getIntValue() - 1;
        for (int slot = 0; slot < numAudioOutSlots; ++slot)
        {
            audioOutAmountAttach[slot].reset();
            boundAudioOutParamID[slot] = {};
        }

        int slot = 0;

        auto* outParam = apvts.getRawParameterValue ("OUT_" + opNum);
        float outVal = outParam != nullptr ? outParam->load (std::memory_order_relaxed) : 0.0f;
        if (outVal > 0.0001f && slot < numAudioOutSlots)
        {
            bindAudioOutSlot (slot, 2, "OUT_" + opNum);
            ++slot;
        }

        for (int dest = 0; dest < ProjectConfig::numOperators && slot < numAudioOutSlots; ++dest)
        {
            if (dest == srcIdx) continue;
            auto* raw = apvts.getRawParameterValue ("AUDIO_ROUTE_" + juce::String (srcIdx) + "_" + juce::String (dest));
            if (raw != nullptr && raw->load (std::memory_order_relaxed) > 0.0001f)
            {
                bindAudioOutSlot (slot, 3 + dest, "AUDIO_ROUTE_" + juce::String (srcIdx) + "_" + juce::String (dest));
                ++slot;
            }
        }

        for (; slot < numAudioOutSlots; ++slot)
        {
            audioOutTarget[slot].setSelectedId (1, juce::dontSendNotification);
            audioOutAmount[slot].setEnabled (false);
            audioOutAmount[slot].setValue (0.0, juce::dontSendNotification);
        }
    }

    // Shared by refreshAudioOutFromState — binds one Audio Route slot to an already-known param.
    void bindAudioOutSlot (int slot, int comboSelectedId, const juce::String& paramID)
    {
        audioOutTarget[slot].setSelectedId (comboSelectedId, juce::dontSendNotification);
        boundAudioOutParamID[slot] = paramID;
        audioOutAmountAttach[slot] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, paramID, audioOutAmount[slot]);
        audioOutAmount[slot].setEnabled (true);
    }

    // Called when an Audio Route slot's destination changes — rebinds its amount slider to the
    // newly chosen param (OUT_ for "Out", AUDIO_ROUTE_this_target for another operator), and
    // zeroes out whichever param that slot was previously bound to so it doesn't linger active
    // and inaudible-but-invisible.
    void onAudioOutTargetChanged (int slot)
    {
        int srcIdx = opNum.getIntValue() - 1;
        int selId  = audioOutTarget[slot].getSelectedId();

        juce::String newParamID;
        if (selId == 2)
            newParamID = "OUT_" + opNum;
        else if (selId >= 3)
            newParamID = "AUDIO_ROUTE_" + juce::String (srcIdx) + "_" + juce::String (selId - 3);
        // selId == 1 ("None") leaves newParamID empty

        if (boundAudioOutParamID[slot].isNotEmpty() && boundAudioOutParamID[slot] != newParamID)
        {
            if (auto* p = apvts.getParameter (boundAudioOutParamID[slot]))
                p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
        }

        audioOutAmountAttach[slot].reset();
        boundAudioOutParamID[slot] = newParamID;

        if (newParamID.isNotEmpty())
        {
            audioOutAmountAttach[slot] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, newParamID, audioOutAmount[slot]);
            audioOutAmount[slot].setEnabled (true);
        }
        else
        {
            audioOutAmount[slot].setEnabled (false);
            audioOutAmount[slot].setValue (0.0, juce::dontSendNotification);
        }
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

    // FM Inputs — compact per-operator view onto a handful of the MOD_src_dest matrix cells
    // that target this operator, instead of a global 12x12 grid. See refreshFMInputsFromState()
    // and onFMInputSourceChanged() below.
    static constexpr int numFMInputSlots = 4;
    juce::ComboBox fmInputSource[numFMInputSlots];
    juce::Slider   fmInputAmount[numFMInputSlots];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fmInputAmountAttach[numFMInputSlots];
    int fmInputBoundSrc[numFMInputSlots] = { -1, -1, -1, -1 }; // -1 = "--" (no source), else 0-based source op index

    // Audio Route — up to numAudioOutSlots independent destinations for this operator's raw
    // audio signal: the main mix ("Out", bound to OUT_), another operator (bound to
    // AUDIO_ROUTE_thisOp_target, an audio-rate feed into that operator), or None. Two slots
    // means an operator can feed two destinations at once. See refreshAudioOutFromState() /
    // onAudioOutTargetChanged() below.
    static constexpr int numAudioOutSlots = 2;
    juce::ComboBox audioOutTarget[numAudioOutSlots];
    juce::Slider audioOutAmount[numAudioOutSlots];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> audioOutAmountAttach[numAudioOutSlots];
    juce::String boundAudioOutParamID[numAudioOutSlots]; // empty = None; else the OUT_/AUDIO_ROUTE_ id bound

    // X positions of the boundaries between the 5 sections, computed each resized() and drawn
    // as thin dividers in paint() so the sections read as clearly separate at a glance.
    int section1Divider = 0, section2Divider = 0, section3Divider = 0, section5Divider = 0;

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
        envLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
        addAndMakeVisible (envLabel);

        static const char* names[] = { "A", "D", "S", "R" };
        juce::Slider::SliderStyle style = juce::Slider::LinearVertical;
        for (int k = 0; k < 4; ++k)
        {
            knob[k].setSliderStyle (style);
            knob[k].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 44, 15);
            addAndMakeVisible (knob[k]);

            knobLabel[k].setText (names[k], juce::dontSendNotification);
            knobLabel[k].setJustificationType (juce::Justification::centred);
            knobLabel[k].setFont (juce::FontOptions (15.0f, juce::Font::bold));
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
                                               juce::roundToInt (getHeight() * 0.025f));

        int labelH = juce::jmax (16, juce::roundToInt (getHeight() * 0.18f));
        envLabel.setBounds (area.removeFromTop (labelH));

        int knobW = area.getWidth() / 4;
        int faderW = juce::jmin (knobW - 4, juce::roundToInt (getWidth() * 0.05f));
        for (int k = 0; k < 4; ++k)
        {
            auto col = area.removeFromLeft (knobW);
            knobLabel[k].setBounds (col.removeFromTop (labelH));
            knob[k].setBounds (col.withSizeKeepingCentre (juce::jmax (24, faderW), col.getHeight()));
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

// One row of the modulation matrix, condensed for the Operators page's one-page layout:
// Src/Tgt selectors stacked on top, Depth as a full-width horizontal slider underneath
// (rather than a knob beside them, as on the standalone Matrix page's ModMatrixSlot).
// Same param wiring as ModMatrixSlot (MatrixPage.h) — kept as a separate struct rather than
// a shared one so this page's layout can diverge without touching the standalone page.
struct CompactModMatrixSlot : public juce::Component
{
    CompactModMatrixSlot (juce::AudioProcessorValueTreeState& apvts, int slotIndex, OAOColors& c)
        : colors (c)
    {
        juce::String s = juce::String (slotIndex + 1); // slots are 1-indexed in param IDs

        sourceSelector.addItemList (ModChoices::sources(), 1);
        sourceSelector.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (sourceSelector);

        addAndMakeVisible (targetSelector);
        ModChoices::buildTargetMenu (targetSelector);

        if (auto* param = apvts.getRawParameterValue ("MOD_TGT_" + s))
        {
            int idx = static_cast<int> (param->load (std::memory_order_relaxed));
            targetSelector.setSelectedId (idx + 1, juce::dontSendNotification);
        }

        targetSelector.onChange = [this, &apvts, s]()
        {
            int selectedId = targetSelector.getSelectedId();
            if (auto* param = apvts.getParameter ("MOD_TGT_" + s))
            {
                float normalized = param->convertTo0to1 (selectedId - 1);
                param->setValueNotifyingHost (normalized);
            }
        };

        amountSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        amountSlider.setRange (-1.0, 1.0, 0.001);
        amountSlider.setValue (0.0, juce::dontSendNotification);
        addAndMakeVisible (amountSlider);

        rowSLabel.setText ("Src. " + s, juce::dontSendNotification);
        rowSLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (rowSLabel);

        rowTLabel.setText ("Tgt. " + s, juce::dontSendNotification);
        rowTLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (rowTLabel);

        depthLabel.setText ("Depth", juce::dontSendNotification);
        depthLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (depthLabel);

        srcAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, "MOD_SRC_" + s, sourceSelector);
        amtAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, "MOD_AMT_" + s, amountSlider);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (2, 0);

        // --- Depth: a horizontal slider spanning the full width, underneath the Src/Tgt rows ---
        int depthRowH = juce::jmax (26, juce::roundToInt (area.getHeight() * 0.30f));
        auto depthRow = area.removeFromBottom (depthRowH);

        int depthLabelH = juce::jmax (11, juce::roundToInt (depthRowH * 0.32f));
        depthLabel.setBounds (depthRow.removeFromTop (depthLabelH));

        int textBoxW = juce::roundToInt (depthRow.getWidth() * 0.3f);
        int textBoxH = juce::jlimit (12, 20, juce::roundToInt (depthRowH * 0.3f));
        amountSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, textBoxW, textBoxH);
        amountSlider.setBounds (depthRow.reduced (2, 0));

        // --- Source/Target, stacked above ---
        int rowH       = juce::jmax (18, juce::roundToInt (area.getHeight() * 0.42f));
        int stackH     = rowH * 2;
        auto leftBlock = area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), stackH));

        auto tArea = leftBlock.removeFromTop (rowH);
        auto bArea = leftBlock.removeFromTop (rowH);
        int w = tArea.getWidth();

        int labelH = juce::jmax (13, juce::roundToInt (rowH * 0.8f));
        int comboHeight = juce::jmax (15, juce::roundToInt (rowH * 0.65f));

        auto sLabelArea = tArea.removeFromLeft (juce::roundToInt (w * 0.28f));
        rowSLabel.setBounds (sLabelArea.withSizeKeepingCentre (sLabelArea.getWidth(), labelH));
        auto sComboArea = tArea.removeFromLeft (juce::roundToInt (w * 0.72f)).reduced (2, 0);
        sourceSelector.setBounds (sComboArea.withSizeKeepingCentre (sComboArea.getWidth(), comboHeight));

        auto tLabelArea = bArea.removeFromLeft (juce::roundToInt (w * 0.28f));
        rowTLabel.setBounds (tLabelArea.withSizeKeepingCentre (tLabelArea.getWidth(), labelH));
        auto tComboArea = bArea.removeFromLeft (juce::roundToInt (w * 0.72f)).reduced (2, 0);
        targetSelector.setBounds (tComboArea.withSizeKeepingCentre (tComboArea.getWidth(), comboHeight));
    }

    void paint (juce::Graphics&) override {}

    void lookAndFeelChanged() override
    {
        juce::Component::lookAndFeelChanged();

        rowSLabel.setColour (juce::Label::textColourId, colors.text);
        rowTLabel.setColour (juce::Label::textColourId, colors.text);
        depthLabel.setColour (juce::Label::textColourId, colors.text);

        sourceSelector.setColour (juce::ComboBox::backgroundColourId, colors.surface);
        sourceSelector.setColour (juce::ComboBox::textColourId, colors.text);
        targetSelector.setColour (juce::ComboBox::backgroundColourId, colors.surface);
        targetSelector.setColour (juce::ComboBox::textColourId, colors.text);

        amountSlider.setColour (juce::Slider::textBoxBackgroundColourId, colors.surface);
        amountSlider.setColour (juce::Slider::textBoxTextColourId, colors.text);

        rowSLabel.sendLookAndFeelChange();
        rowTLabel.sendLookAndFeelChange();
        depthLabel.sendLookAndFeelChange();
        sourceSelector.sendLookAndFeelChange();
        targetSelector.sendLookAndFeelChange();
        amountSlider.sendLookAndFeelChange();
    }

private:
    OAOColors& colors;
    juce::Label    rowSLabel, rowTLabel, depthLabel;
    juce::ComboBox sourceSelector, targetSelector;
    juce::Slider   amountSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> srcAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amtAttach;
};

// A single row of column headers sitting above the operator list, explaining what each of the
// 5 (well, 6 counting Out/Out2 separately) sections of every operator bar means at a glance.
// Mirrors CompactOperatorGroup's exact section-width formulas (see its resized()) so the
// labels line up with the columns below — if that layout changes, this needs to match it.
struct OperatorsHeaderBar : public juce::Component
{
    explicit OperatorsHeaderBar (OAOColors& c) : colors (c)
    {
        auto setup = [this] (juce::Label& l, const juce::String& text)
        {
            l.setText (text, juce::dontSendNotification);
            l.setJustificationType (juce::Justification::centred);
            l.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            addAndMakeVisible (l);
        };
        setup (optionsLabel,  "Operator Options");
        setup (controlsLabel, "Operator Controls");
        setup (envLabel,      "Env");
        setup (fmLabel,       "FM coming into Operator");
        setup (outLabel,      "Out");
        setup (out2Label,     "Out2");
    }

    // Must be called before resized(), same as CompactOperatorGroup::setSharedKnobTarget()
    void setSharedKnobTarget (int targetDiameter)
    {
        sharedKnobTarget = targetDiameter;
        if (getWidth() > 0 && getHeight() > 0)
            resized();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (static_cast<int> (getWidth() * 0.02f), 0);
        float w = static_cast<float> (area.getWidth());

        int gap = juce::jmax (4, juce::roundToInt (w * 0.018f));

        // Section 1 — Operator Options (op number + Mode/Wave/Effect/FreqMode selectors)
        int opNumW   = juce::jmax (70, juce::roundToInt (w * 0.03f));
        int leftColW = juce::jmax (70, juce::roundToInt (w * 0.13f));
        optionsLabel.setBounds (area.removeFromLeft (opNumW + leftColW));
        area.removeFromLeft (gap);

        // Section 5 — Out / Out2 (carved from the right first, same as the operator rows)
        int audioColW  = juce::jmax (70, juce::roundToInt (w * 0.065f)) * 2;
        auto audioArea = area.removeFromRight (audioColW);
        area.removeFromRight (gap);
        outLabel.setBounds  (audioArea.removeFromLeft (audioArea.getWidth() / 2));
        out2Label.setBounds (audioArea);

        // Section 2 — Operator Controls (the four tweak knobs)
        int knobColW = juce::jmax (sharedKnobTarget + 4, juce::roundToInt (w * 0.058f));
        controlsLabel.setBounds (area.removeFromLeft (knobColW * 4));
        area.removeFromLeft (gap);

        // Section 3 — Env
        int envColW = juce::jmax (50, juce::roundToInt (w * 0.05f));
        envLabel.setBounds (area.removeFromLeft (envColW));
        area.removeFromLeft (gap);

        // Section 4 — FM coming into Operator (whatever's left)
        fmLabel.setBounds (area);
    }

    void lookAndFeelChanged() override
    {
        juce::Component::lookAndFeelChanged();
        for (auto* l : { &optionsLabel, &controlsLabel, &envLabel, &fmLabel, &outLabel, &out2Label })
        {
            l->setColour (juce::Label::textColourId, colors.textDim);
            l->sendLookAndFeelChange();
        }
    }

private:
    OAOColors& colors;
    int sharedKnobTarget = 40;
    juce::Label optionsLabel, controlsLabel, envLabel, fmLabel, outLabel, out2Label;
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

    OperatorsPage (juce::AudioProcessorValueTreeState& apvts, OAOColors& c) : colors (c), headerBar (c)
    {
        addAndMakeVisible (headerBar);

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

        // Note: the standalone Matrix page's sidebar actually creates numOperators (12)
        // ModMatrixSlot rows even though only numModSlots (6) MOD_SRC_/MOD_TGT_/MOD_AMT_
        // params exist — slots 7-12 there are bound to params that don't exist. This embedded
        // copy only shows the first 4 of the real 6 slots, to keep the condensed one-page
        // layout from getting too crowded; slots 5-6 are still reachable from that page.
        constexpr int numVisibleModSlots = 4;
        static_assert (numVisibleModSlots <= ProjectConfig::numModSlots, "can't show more slots than exist");
        for (int i = 0; i < numVisibleModSlots; ++i)
        {
            modMatrixSlots.push_back (std::make_unique<CompactModMatrixSlot> (apvts, i, colors));
            addAndMakeVisible (*modMatrixSlots.back());
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

        headerBar.lookAndFeelChanged();

        for (auto& module : opModules)
        {
            if (module != nullptr)
                module->lookAndFeelChanged();
        }

        for (auto& slot : modMatrixSlots)
        {
            if (slot != nullptr)
                slot->lookAndFeelChanged();
        }

        for (auto& env : envelopeSlots)
        {
            if (env != nullptr)
                env->lookAndFeelChanged();
        }
    }

    void repaintAll()
    {
        headerBar.repaint();
        for (auto& op : opModules)
            op->repaint();
        for (auto& slot : modMatrixSlots)
            slot->repaint();
        for (auto& env : envelopeSlots)
            env->repaint();
        repaint();
    }

    // Re-syncs every operator's FM Input rows to whatever is currently in the MOD_ matrix.
    // Call this after anything that rewrites those params directly — a preset load (via
    // FMPluginAudioProcessor::onSamplesRestored) or a PresetBar::onPatchChanged event
    // (Init, the randomizers, the Algorithm quick-select).
    void refreshFMInputsAll()
    {
        for (auto& op : opModules)
            if (op != nullptr)
                op->refreshFMInputsFromState();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (
            juce::roundToInt (getWidth()  * ProjectConfig::outerMargin),
            juce::roundToInt (getHeight() * ProjectConfig::outerMargin));

        // More breathing room between sections and rows than before (was 0.006f).
        int gap = juce::jmax (4, juce::roundToInt (getHeight() * 0.014f));

        // --- Header bar: small fixed height at the very top, labeling each operator-bar section ---
        int headerHeight = juce::jmax (16, juce::roundToInt (area.getHeight() * 0.025f));
        auto headerArea  = area.removeFromTop (headerHeight);
        area.removeFromTop (gap);

        // --- Mod Matrix row: fixed height at the very bottom, one column per slot ---
        int modMatrixRowHeight = juce::roundToInt (area.getHeight() * 0.20f);
        auto modMatrixRowArea  = area.removeFromBottom (modMatrixRowHeight);
        area.removeFromBottom (gap);

        // --- Envelope row: sits just above the mod matrix, beneath the operators. A bit
        // taller than before (was 0.12f) since it's no longer sharing the top with macros.
        int envRowHeight = juce::roundToInt (area.getHeight() * 0.19f);
        auto envRowArea  = area.removeFromBottom (envRowHeight);
        area.removeFromBottom (gap);

        // --- Operators: everything left, at the top ---
        int rows = ProjectConfig::numOperators;
        int cols = 1;
        int rowGap = 0; // no gap between operators — they sit flush against each other
        int cellWidth = area.getWidth() / cols;
        int cellHeight = area.getHeight() / rows;

        // Computed from the true page dimensions (this component, not a per-row card)
        int sharedKnobTarget = juce::roundToInt (
            juce::jmin (getWidth(), getHeight()) * colors.knobDiameterFraction);
	float sharedFontSize = static_cast<float>(cellHeight) * 0.4f;

        headerBar.setSharedKnobTarget (sharedKnobTarget);
        headerBar.setBounds (headerArea);

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
	            cellBounds.removeFromTop (rowGap);
                    cellBounds.removeFromBottom (rowGap);

                    // Must be set before setBounds
                    opModules[index]->setSharedKnobTarget (sharedKnobTarget);
                    opModules[index]->setBounds (cellBounds);
                }
            }
        }

        // --- Envelope row (laid out) ---
        int numEnvs = static_cast<int> (envelopeSlots.size());
        if (numEnvs > 0)
        {
            int envCellW = envRowArea.getWidth() / numEnvs;
            int envGap   = juce::jmax (2, gap / 2);
            for (int i = 0; i < numEnvs; ++i)
            {
                bool isLast = (i == numEnvs - 1);
                auto cell = envRowArea.removeFromLeft (isLast ? envRowArea.getWidth() : envCellW);
                if (! isLast) envRowArea.removeFromLeft (envGap);
                envelopeSlots[i]->setBounds (cell);
            }
        }

        // --- Mod Matrix row (laid out) ---
        int numModMatrix = static_cast<int> (modMatrixSlots.size());
        if (numModMatrix > 0)
        {
            int slotW = modMatrixRowArea.getWidth() / numModMatrix;
            int slotGap = juce::jmax (2, gap / 2);
            for (int i = 0; i < numModMatrix; ++i)
            {
                bool isLast = (i == numModMatrix - 1);
                auto cell = modMatrixRowArea.removeFromLeft (isLast ? modMatrixRowArea.getWidth() : slotW);
                if (! isLast) modMatrixRowArea.removeFromLeft (slotGap);
                modMatrixSlots[i]->setBounds (cell);
            }
        }
    }

private:
    OAOColors& colors;
    OperatorsHeaderBar headerBar;
    std::vector<std::unique_ptr<CompactOperatorGroup>> opModules;
    std::vector<std::unique_ptr<CompactModMatrixSlot>> modMatrixSlots;
    std::vector<std::unique_ptr<EnvelopeSlot>> envelopeSlots;
};
