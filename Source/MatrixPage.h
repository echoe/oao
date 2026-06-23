#pragma once
#include <JuceHeader.h>
#include "Constants.h"
#include "OAOColors.h"
#include "OAOLookAndFeel.h"

// --- ONE ROW OF THE MODULATION MATRIX ---
struct ModMatrixSlot : public juce::Component
{
    ModMatrixSlot (juce::AudioProcessorValueTreeState& apvts, int slotIndex, OAOColors& c)
        : colors(c)
    {
        juce::String s = juce::String (slotIndex + 1); // slots are 1-indexed in param IDs

        // Source dropdown — items are 1-indexed in ComboBox, 0-indexed in AudioParameterChoice
        sourceSelector.addItemList (ModChoices::sources(), 1);
        sourceSelector.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (sourceSelector);

        // Target selector. We stack and create the menu so it's usable  without scrolling through 80 choices
        addAndMakeVisible(targetSelector);
        buildTargetMenu();
        
        // restore value from parameter on construction:
        if (auto* param = apvts.getRawParameterValue("MOD_TGT_" + s))
        {
            int idx = static_cast<int>(param->load(std::memory_order_relaxed));
            targetSelector.setSelectedId(idx + 1, juce::dontSendNotification);
        }

        targetSelector.onChange = [this, &apvts, s]()
        {
            // Map ComboBox ID back to parameter index
            int selectedId = targetSelector.getSelectedId();
            if (auto* param = apvts.getParameter("MOD_TGT_" + s))
            {
                // ComboBox IDs are 1-based, parameter choice indices are 0-based
                float normalized = param->convertTo0to1(selectedId - 1);
                param->setValueNotifyingHost(normalized);
            }
        };
        
        // Bi-polar amount knob
        amountSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        amountSlider.setRange (-1.0, 1.0, 0.001);
        amountSlider.setValue (0.0, juce::dontSendNotification);
        amountSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                              (int)(getWidth() * 0.3f), (int)(getHeight() * 0.2f));
	addAndMakeVisible (amountSlider);

        // Row label
        rowSLabel.setText ("Src. " + s, juce::dontSendNotification);
        rowSLabel.setJustificationType (juce::Justification::centred);
	addAndMakeVisible (rowSLabel);

        // Row label
        rowTLabel.setText ("Tgt. " + s, juce::dontSendNotification);
        rowTLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (rowTLabel);

        // Depth label
        depthLabel.setText ("Depth", juce::dontSendNotification);
        depthLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (depthLabel);

        // Attach to APVTS — param IDs must match PluginProcessor exactly
        srcAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, "MOD_SRC_" + s, sourceSelector);
        amtAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, "MOD_AMT_" + s, amountSlider);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (2, 0);

        // --- Right block: Depth Knob & Label ---
        auto rightArea = area.removeFromRight (juce::roundToInt (area.getWidth() * 0.28f));

        // 1. Fix the TextBox visibility!
        // We set the style here instead of the constructor because we finally have non-zero bounds.
        int textBoxW = static_cast<int> (rightArea.getWidth() * 0.8f);
        int textBoxH = juce::jlimit (12, 20, static_cast<int> (area.getHeight() * 0.2f));
        amountSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);

        // 2. Center the knob block directly in the area so it matches the grid's vertical center exactly.
        int knobSize = juce::jmin (area.getHeight(), rightArea.getWidth());
        auto knobBlock = rightArea.withSizeKeepingCentre (rightArea.getWidth(), knobSize);
        amountSlider.setBounds (knobBlock);

        // 3. Float the "Depth" label in the empty space above the knob.
        // By taking the space from rightArea.getY() to knobBlock.getY(), the label
        // doesn't push the knob off-center.
        int labelHeight = knobBlock.getY() - rightArea.getY();
        depthLabel.setBounds (rightArea.getX(), rightArea.getY(), rightArea.getWidth(), labelHeight);

        // --- Left block: Source/Target ---
        // (Keep the updated dropdown sizing logic from the previous step here)
        int rowH       = juce::jmax (20, juce::roundToInt (area.getHeight() * 0.25f));
        int stackH     = rowH * 2;
        auto leftBlock = area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), stackH));

        auto tArea = leftBlock.removeFromTop (rowH);
        auto bArea = leftBlock.removeFromTop (rowH);
        int w = tArea.getWidth();

        int labelH = juce::jmax (14, juce::roundToInt (rowH * 0.8f));
        int comboHeight = juce::jmax (16, juce::roundToInt (rowH * 0.65f));

        auto sLabelArea = tArea.removeFromLeft (w * 0.3f);
        rowSLabel.setBounds (sLabelArea.withSizeKeepingCentre (sLabelArea.getWidth(), labelH));
        auto sComboArea = tArea.removeFromLeft (w * 0.7f).reduced (2, 0);
        sourceSelector.setBounds (sComboArea.withSizeKeepingCentre (sComboArea.getWidth(), comboHeight));

        auto tLabelArea = bArea.removeFromLeft (w * 0.3f);
        rowTLabel.setBounds (tLabelArea.withSizeKeepingCentre (tLabelArea.getWidth(), labelH));
        auto tComboArea = bArea.removeFromLeft (w * 0.7f).reduced (2, 0);
        targetSelector.setBounds (tComboArea.withSizeKeepingCentre (tComboArea.getWidth(), comboHeight));
    }

    void buildTargetMenu()
    {
        targetSelector.clear(juce::dontSendNotification);
        targetSelector.addItem("None", 1);
    
        // Operator params — nested by operator
        juce::PopupMenu opMenu;
        for (int op = 0; op < 6; ++op)
        {
            juce::PopupMenu sub;
            int base = op * 5 + 2; // offset by 1 for None, +1 for ComboBox 1-indexing
            sub.addItem(base + 0, "Knob A");
            sub.addItem(base + 1, "Knob B");
            sub.addItem(base + 2, "Knob C");
            sub.addItem(base + 3, "Knob D");
            sub.addItem(base + 4, "Level");
            opMenu.addSubMenu("Op " + juce::String(op + 1), sub);
        }
        targetSelector.getRootMenu()->addSubMenu("Operators", opMenu);
    
        // FX params — nested by slot
        juce::PopupMenu fxMenu;
        for (int fx = 0; fx < 6; ++fx)
        {
            juce::PopupMenu sub;
            int base = fx * 5 + 32;
            sub.addItem(base + 0, "Knob A");
            sub.addItem(base + 1, "Knob B");
            sub.addItem(base + 2, "Knob C");
            sub.addItem(base + 3, "Knob D");
            sub.addItem(base + 4, "Mix");
            fxMenu.addSubMenu("FX " + juce::String(fx + 1), sub);
        }
        targetSelector.getRootMenu()->addSubMenu("Effects", fxMenu);
    
        // FM Matrix — nested by source operator
        juce::PopupMenu matrixMenu;
        for (int src = 0; src < 6; ++src)
        {
            juce::PopupMenu sub;
            for (int dst = 0; dst < 6; ++dst)
            {
                int id = 62 + src * 6 + dst; // 61 entries before this + 1 for 1-indexing
                sub.addItem(id, "Op " + juce::String(dst + 1));
            }
            matrixMenu.addSubMenu("Op " + juce::String(src + 1), sub);
        }
        targetSelector.getRootMenu()->addSubMenu("FM Matrix", matrixMenu);
    }

    void paint (juce::Graphics& g) override {}

    void lookAndFeelChanged() override
    {
        juce::Component::lookAndFeelChanged();

        // 3. Force the Row Label text color
        rowSLabel.setColour (juce::Label::textColourId, colors.text);
        rowTLabel.setColour (juce::Label::textColourId, colors.text);
        depthLabel.setColour (juce::Label::textColourId, colors.text);

        // 4. Force the ComboBox colors
        sourceSelector.setColour (juce::ComboBox::backgroundColourId, colors.surface);
        sourceSelector.setColour (juce::ComboBox::textColourId, colors.text);

        targetSelector.setColour (juce::ComboBox::backgroundColourId, colors.surface);
        targetSelector.setColour (juce::ComboBox::textColourId, colors.text);

        // 5. Do the slider text boxes just like the main page
        amountSlider.setColour (juce::Slider::textBoxBackgroundColourId, colors.surface);
        amountSlider.setColour (juce::Slider::textBoxTextColourId, colors.text);

        // Nudge everything
        rowSLabel.sendLookAndFeelChange(); rowTLabel.sendLookAndFeelChange();
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
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   amtAttach;
};


// --- MAIN MATRIX PAGE ---
class MatrixPage : public juce::Component
{
public:
    // 1. Add OAOColors& to the constructor
    MatrixPage (juce::AudioProcessorValueTreeState& apvts,
                const juce::String& prefix,
                const juce::String& title,
                OAOColors& c)
        : paramPrefix (prefix), matrixTitle (title), colors (c)
    {
        // NxN grid — param IDs must be "MATRIX_src_dest" (or whatever prefix+"src_dest")
        for (int src = 0; src < ProjectConfig::numOperators; ++src)
        {
            for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
            {
                auto* s = matrixSliders.add (new juce::Slider());
                s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                s->setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                    (int)(knobSize * ProjectConfig::textBoxWidthFraction), (int)(knobSize * ProjectConfig::textBoxHeightFraction));
		addAndMakeVisible (s);

                // e.g. "AUDIO_ROUTE_0_1" or "MOD_0_1" depending on which page this is
                juce::String id = paramPrefix + juce::String (src) + "_" + juce::String (dest);
                matrixAttachments.add (
                    std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, *s));
                matrixParams.add (apvts.getParameter (id));
            }
        }

        // Sidebar: mod slots (MOD_ page) or output levels (AUDIO_ROUTE_ page)
        if (paramPrefix == "MOD_")
        {
            for (int i = 0; i < ProjectConfig::numOperators; ++i)
            {
                modSlots.push_back (std::make_unique<ModMatrixSlot> (apvts, i, colors));
                addAndMakeVisible (*modSlots.back());
            }
        }
        else
        {
            for (int i = 0; i < ProjectConfig::numOperators; ++i)
            {
                auto* sl = outputSliders.add (new juce::Slider());
                sl->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                sl->setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                    (int)(knobSize * 0.45f), (int)(knobSize * ProjectConfig::textBoxHeightFraction));
		addAndMakeVisible (sl);

                auto* lb = outputLabels.add (new juce::Label());
                lb->setText ("Out " + juce::String (i + 1), juce::dontSendNotification);
                lb->setJustificationType (juce::Justification::centred);
                addAndMakeVisible (lb);

                outputAttachments.add (
                    std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                        apvts, "OUT_" + juce::String (i + 1), *sl));
            }
        }

        // Reset button — top left, zeroes out every cell in this page's matrix
        addAndMakeVisible (resetButton);
        resetButton.onClick = [this]()
        {
            for (auto* p : matrixParams)
                if (p != nullptr)
                    p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
        };
    }

    void recalcGeometry()
    {
        cachedW = static_cast<float> (getWidth());
        cachedH = static_cast<float> (getHeight());
    
        // Shared outer margin, same value the card itself is drawn with in paint() —
        // stored here so resized() (e.g. the Reset button) can align to it exactly.
        outerW = cachedW * ProjectConfig::outerMargin;
        outerH = cachedH * ProjectConfig::outerMargin;

        int labelLeft = static_cast<int> (cachedW * 0.055f);
        int labelTop  = static_cast<int> (cachedH * 0.05f);
    
        int gridAreaW  = static_cast<int> (cachedW * splitRatio);
        int availableW = gridAreaW - labelLeft;
        int availableH = static_cast<int> (cachedH * 0.99f) - labelTop;
    
        // Horizontal and vertical pitch computed independently, so the grid's overall
        // bounding box can be a wide rectangle even though each knob stays round.
        cellW    = availableW / ProjectConfig::numOperators;
        cellH    = availableH / ProjectConfig::numOperators;
        // Clamp toward the shared knob target so this page's knobs match
        // OperatorsPage/EffectsPage even as layouts evolve independently.
        int sharedTarget = juce::roundToInt (juce::jmin (cachedW, cachedH) * ProjectConfig::knobDiameterFraction);
        knobSize = juce::jmin (std::min (cellW, cellH), sharedTarget);
        gridX    = labelLeft;
        gridY    = labelTop;
        labelH   = knobSize * 0.26f;
    }

    void paint (juce::Graphics& g) override
    {
        recalcGeometry();

        // Backdrop — the space between/around the grid and sidebar regions
        g.fillAll (colors.panelGap);

        float splitX = getWidth() * splitRatio;
        float cardGap = juce::jmax (2.0f, cachedH * 0.006f);
        float halfCardGap = juce::jmax (1.0f, cardGap * 0.5f);

        // Card behind the grid region — extended left/up to also cover the row/column
        // label strips and the Reset button corner, not just the cell bounds themselves
        float cardLeft = outerW;
        float cardTop  = outerH;
        juce::Rectangle<float> gridCard (
            cardLeft,
            cardTop,
            (float) (gridX + ProjectConfig::numOperators * cellW) + halfCardGap - cardLeft,
            (float) (gridY + ProjectConfig::numOperators * cellH) + outerH - cardTop);
        g.setColour (colors.background);
        g.fillRoundedRectangle (gridCard, 4.0f);
        g.setColour (colors.text.withAlpha (0.15f));
        g.drawRoundedRectangle (gridCard.reduced (1.0f), 4.0f, 1.0f);

        // Calculate the exact bottom of the grid so the sidebar can match it
        float gridBottom = (float)(gridY + ProjectConfig::numOperators * cellH);
        // Card behind the sidebar region — insets only half the gap from the shared
        // split line so the visible gap between the two cards isn't doubled
        juce::Rectangle<float> sidebarCard (
            splitX + halfCardGap,
            outerH,
            (float) getWidth() - splitX - halfCardGap - outerW,
            gridBottom);
        g.setColour (colors.background);
        g.fillRoundedRectangle (sidebarCard, 4.0f);
        g.setColour (colors.text.withAlpha (0.15f));
        g.drawRoundedRectangle (sidebarCard.reduced (1.0f), 4.0f, 1.0f);
        
        // Safely grab our custom LookAndFeel to use the selected font
        auto* oaoLnf = dynamic_cast<OAOLookAndFeel*> (&getLookAndFeel());
        
        // Quick helper to apply the font if the LookAndFeel exists
        auto applyFont = [&](float height) {
            if (oaoLnf != nullptr) g.setFont (oaoLnf->getCustomFont (height));
            else                   g.setFont (juce::FontOptions (height));
        };

        // Row/column labels for the NxN grid
        g.setColour (colors.text);
        applyFont (juce::jmax (10.0f, knobSize * 0.20f)); // <-- larger
        
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            // Row labels (left side — "Op N")
            int y = gridY + i * cellH;
            g.drawText ("Op " + juce::String (i + 1),
                0, y,
                gridX - 2, cellH,
                juce::Justification::centredRight);
        
            // Column labels (top — "To Op N")
            int x = gridX + i * cellW;
            g.drawText ("To Op " + juce::String (i + 1),
                x, gridY - (int)labelH,
                cellW, (int)labelH,
                juce::Justification::centred);
        }
        
        // Sidebar divider
        // draw line
        g.setColour (colors.primary.withAlpha (0.3f));
        g.drawVerticalLine (static_cast<int> (splitX), getHeight() * 0.05f, getHeight() - 10.0f);
    }

    void resized() override
    {
        recalcGeometry();
        auto area        = getLocalBounds();

        // Reset button — sits in the empty corner where row labels (left) and
        // column labels (top) meet, instead of occupying its own separate strip.
        // Anchored to outerW/outerH (the same margin the card itself is drawn with
        // in paint()) so it sits flush inside the card edge instead of poking past it.
        int btnX = juce::roundToInt (outerW) + 2;
        int btnY = gridY - static_cast<int> (labelH) + 1;
        int btnMaxW = juce::jmax (10, gridX - btnX - 2);
        int btnMaxH = juce::jmax (10, static_cast<int> (labelH) - 4);
        int btnW = juce::jlimit (10, btnMaxW, static_cast<int> (cachedW * 0.05f));
        int btnH = juce::jlimit (10, btnMaxH, static_cast<int> (labelH * 0.7f));
        resetButton.setBounds (btnX +10, btnY, btnW, btnH);
    
        int totalW       = area.getWidth();
        auto gridArea    = area.removeFromLeft (static_cast<int> (totalW * splitRatio));
        auto sidebarAreaFull = area.reduced (cachedW * 0.01f, 0);

        // Align the sidebar's row pitch and vertical origin with the grid's, so
        // sidebar row N lines up exactly with grid row N (true vertical symmetry).
        auto sidebarArea = sidebarAreaFull;
        sidebarArea.removeFromTop (gridY);

        // Textbox sizing — legible relative to knob size, shared with OperatorsPage/EffectsPage
        int textBoxW = static_cast<int> (knobSize * ProjectConfig::textBoxWidthFraction);
        int textBoxH = juce::jlimit (12, 70, static_cast<int> (knobSize * ProjectConfig::textBoxHeightFraction));
    
        int idx = 0;
        for (int src = 0; src < ProjectConfig::numOperators; ++src)
            for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
                if (auto* s = matrixSliders[idx++])
                {
                    s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
                    //Make it a rectangle so the whole screen is filled
		    juce::Rectangle<int> cell (gridX + dest * cellW, gridY + src * cellH, cellW, cellH);
                    int boxH = juce::jmin (cellH, knobSize - 2 + textBoxH);
                    s->setBounds (cell.withSizeKeepingCentre (knobSize - 4, boxH));
                }
    
        if (paramPrefix == "MOD_")
        {
            for (int i = 0; i < ProjectConfig::numOperators; ++i)
                if (i < static_cast<int> (modSlots.size()))
                    modSlots[i]->setBounds (sidebarArea.removeFromTop (cellH));
        }
        else
        {
            int labelW  = static_cast<int> (sidebarArea.getWidth() * 0.35f);
            for (int i = 0; i < ProjectConfig::numOperators; ++i)
            {
                auto cell = sidebarArea.removeFromTop (cellH).reduced (0, cachedH * 0.005f);
                if (auto* lb = outputLabels[i])
                {
                    auto labelCell = cell.removeFromLeft (labelW);
                    int  outLabelH = juce::jlimit (12, static_cast<int> (cellH * 0.3f),
                                                   static_cast<int> (cellH * 0.22f));
                    lb->setBounds (labelCell.withSizeKeepingCentre (labelCell.getWidth(), outLabelH));
                }
                if (auto* sl = outputSliders[i])
                {
                    sl->setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                                         cell.getWidth(), static_cast<int> (cellH * 0.25f));
                    sl->setBounds (cell);
                }
            }
        }
    
        repaint();
    }

    void lookAndFeelChanged() override
    {
        juce::Component::lookAndFeelChanged();

        // Use the colors from your struct! (Change these to whichever ones you prefer)
        auto newBgColor    = colors.surface;
        auto newTextColour = colors.text;
        auto newOutline    = juce::Colours::transparentBlack;

        for (auto* s : matrixSliders)
        {
            if (s != nullptr)
            {
                s->setColour (juce::Slider::textBoxBackgroundColourId, newBgColor);
                s->setColour (juce::Slider::textBoxTextColourId, newTextColour);
                s->setColour (juce::Slider::textBoxOutlineColourId, newOutline);
                s->sendLookAndFeelChange();
            }
        }

        for (auto* s : outputSliders)
        {
            if (s != nullptr)
            {
                s->setColour (juce::Slider::textBoxBackgroundColourId, newBgColor);
                s->setColour (juce::Slider::textBoxTextColourId, newTextColour);
                s->setColour (juce::Slider::textBoxOutlineColourId, newOutline);
                s->sendLookAndFeelChange();
            }
        }

        for (auto& slot : modSlots)
        {
            if (slot != nullptr)
                slot->lookAndFeelChanged();
        }
	for (auto* label : outputLabels)
        {
            if (label != nullptr)
            {
                label->setColour (juce::Label::textColourId, colors.text);
                label->sendLookAndFeelChange();
            }
        }

        resetButton.setColour (juce::TextButton::buttonColourId, colors.surface);
        resetButton.setColour (juce::TextButton::textColourOffId, colors.text);
        resetButton.sendLookAndFeelChange();
    }

    void refreshColors()
    {
        for (auto* s : matrixSliders)
            if (s != nullptr)
            {
                s->setColour (juce::Slider::textBoxTextColourId,       colors.text);
                s->setColour (juce::Slider::textBoxBackgroundColourId, colors.background);
                s->setColour (juce::Slider::textBoxOutlineColourId,    colors.primary.withAlpha (0.3f));
            }
        repaint();
    }

private:
    juce::String paramPrefix;
    juce::String matrixTitle;
    OAOColors& colors; // Store colors reference
    // Grid geometry — tweak these to taste
    int gridX     = 45;
    int gridY     = 70;
    int cellW     = 90; // horizontal pitch between grid columns
    int cellH     = 90; // vertical pitch between grid rows
    int knobSize  = 86; // actual round knob diameter, derived from min(cellW, cellH)
    float labelH  = 18.0f; // height of the column-label strip above the grid
    float cachedW = 0.0f;
    float cachedH = 0.0f;
    float outerW  = 0.0f; // shared outer margin (matches the card edge in paint())
    float outerH  = 0.0f;
    static constexpr float splitRatio = 0.70f;
    juce::TextButton resetButton { "Reset" };
    juce::OwnedArray<juce::Slider>     matrixSliders;
    juce::Array<juce::RangedAudioParameter*> matrixParams; // parallel to matrixSliders, raw (non-owning)
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> matrixAttachments;
    std::vector<std::unique_ptr<ModMatrixSlot>> modSlots;
    juce::OwnedArray<juce::Slider>     outputSliders;
    juce::OwnedArray<juce::Label>      outputLabels;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachments;
};
