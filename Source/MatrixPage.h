#pragma once
#include <JuceHeader.h>
#include "Constants.h"
#include "OAOColors.h"

// Shared choice lists for modulation — keep these in sync with PluginProcessor.cpp
namespace ModChoices
{
    inline juce::StringArray sources()
    {
        return { "None", "Op 1", "Op 2", "Op 3", "Op 4", "Op 5", "Op 6" };
    }

    inline juce::StringArray targets()
    {
        return {
            "None",
            "Op 1 Ratio",  "Op 1 Detune", "Op 1 Phase", "Op 1 Fold", "Op 1 Level",
            "Op 2 Ratio",  "Op 2 Detune", "Op 2 Phase", "Op 2 Fold", "Op 2 Level",
            "Op 3 Ratio",  "Op 3 Detune", "Op 3 Phase", "Op 3 Fold", "Op 3 Level",
            "Op 4 Ratio",  "Op 4 Detune", "Op 4 Phase", "Op 4 Fold", "Op 4 Level",
            "Op 5 Ratio",  "Op 5 Detune", "Op 5 Phase", "Op 5 Fold", "Op 5 Level",
            "Op 6 Ratio",  "Op 6 Detune", "Op 6 Phase", "Op 6 Fold", "Op 6 Level",
	    "Fx 1 Ratio",  "Fx 1 Detune", "Fx 1 Phase", "Fx 1 Fold", "Fx 1 Level",
	    "Fx 2 Ratio",  "Fx 2 Detune", "Fx 2 Phase", "Fx 2 Fold", "Fx 2 Level",
	    "Fx 3 Ratio",  "Fx 3 Detune", "Fx 3 Phase", "Fx 3 Fold", "Fx 3 Level",
        };
    }
}

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

        targetSelector.addItemList (ModChoices::targets(), 1);
        targetSelector.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (targetSelector);

        // Bi-polar amount slider
        amountSlider.setSliderStyle (juce::Slider::LinearVertical);
        amountSlider.setRange (-1.0, 1.0, 0.001);
        amountSlider.setValue (0.0, juce::dontSendNotification);
        amountSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 15);
        addAndMakeVisible (amountSlider);

        // Row label
        rowLabel.setText ("S" + s, juce::dontSendNotification);
        rowLabel.setJustificationType (juce::Justification::centred);
        rowLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
	addAndMakeVisible (rowLabel);

        // Attach to APVTS — param IDs must match PluginProcessor exactly
        srcAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, "MOD_SRC_" + s, sourceSelector);
        tgtAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, "MOD_TGT_" + s, targetSelector);
        amtAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, "MOD_AMT_" + s, amountSlider);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (2, 3);
        int w = area.getWidth();
        rowLabel.setBounds       (area.removeFromLeft (w * 0.06f));
        sourceSelector.setBounds (area.removeFromLeft (w * 0.25f).reduced (2, 0));
        targetSelector.setBounds (area.removeFromLeft (w * 0.35f).reduced (2, 0));
        amountSlider.setBounds   (area.reduced (2, 0));
    }

    void paint (juce::Graphics& g) override {}

    void lookAndFeelChanged() override
    {
        juce::Component::lookAndFeelChanged();

        // 3. Force the Row Label text color
        rowLabel.setColour (juce::Label::textColourId, colors.text);

        // 4. Force the ComboBox colors
        sourceSelector.setColour (juce::ComboBox::backgroundColourId, colors.surface);
        sourceSelector.setColour (juce::ComboBox::textColourId, colors.text);

        targetSelector.setColour (juce::ComboBox::backgroundColourId, colors.surface);
        targetSelector.setColour (juce::ComboBox::textColourId, colors.text);

        // 5. Do the slider text boxes just like the main page
        amountSlider.setColour (juce::Slider::textBoxBackgroundColourId, colors.surface);
        amountSlider.setColour (juce::Slider::textBoxTextColourId, colors.text);

        // Nudge everything
        rowLabel.sendLookAndFeelChange();
        sourceSelector.sendLookAndFeelChange();
        targetSelector.sendLookAndFeelChange();
        amountSlider.sendLookAndFeelChange();
    }

private:
    OAOColors& colors;
    juce::Label    rowLabel;
    juce::ComboBox sourceSelector, targetSelector;
    juce::Slider   amountSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> srcAttach, tgtAttach;
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
                OAOColors& c) // <--- Added here
        : paramPrefix (prefix), matrixTitle (title), colors (c)
    {
        // NxN FM grid — param IDs must be "FM_src_dest" (or whatever prefix+"src_dest")
        for (int src = 0; src < ProjectConfig::numOperators; ++src)
        {
            for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
            {
                auto* s = matrixSliders.add (new juce::Slider());
                s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 45, 13);
                addAndMakeVisible (s);

                // e.g. "FM_0_1" or "MOD_0_1" depending on which page this is
                juce::String id = paramPrefix + juce::String (src) + "_" + juce::String (dest);
                matrixAttachments.add (
                    std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, *s));
            }
        }

        // Sidebar: mod slots (MOD_ page) or output levels (FM_ page)
        if (paramPrefix == "MOD_")
        {
            for (int i = 0; i < 6; ++i)
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
                sl->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 12);
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
    }

    void paint (juce::Graphics& g) override
    {
        // Title
	g.setColour (colors.text);
        g.setFont (juce::jlimit (8.0f, 18.0f, cellSize * 0.20f));
        g.drawText (matrixTitle, getLocalBounds().removeFromTop (30), juce::Justification::centred);

        // Row/column labels for the NxN grid
        g.setFont (juce::jlimit (8.0f, 18.0f, cellSize * 0.15f));

        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            // Row labels (left side — "From Op N")
            int y = gridY + i * cellSize + cellSize / 2 - 8;
            g.drawText ("Op " + juce::String (i + 1),
                        gridX - 38, y, 36, 16,
                        juce::Justification::centredRight);

            // Column labels (top — "To Op N")
            int x = gridX + i * cellSize;
            g.drawText ("To Op " + juce::String (i + 1),
                        x, gridY - 18, cellSize, 16,
                        juce::Justification::centred);
        }

        // Sidebar divider
        float splitX = getWidth() * splitRatio;

        // Sidebar header
        g.setFont (juce::jlimit (8.0f, 18.0f, cellSize * 0.15f));
        juce::String sideTitle = (paramPrefix == "MOD_") ? "MOD ROUTING" : "CARRIER OUTPUTS";
        g.drawText (sideTitle,
                    static_cast<int> (splitX) + 8, 10,
                    getWidth() - static_cast<int> (splitX) -16, 18,
                    juce::Justification::centred);
        // change color to draw line
	g.setColour (colors.primary.withAlpha (0.3f));
        g.drawVerticalLine (static_cast<int> (splitX), 45.0f, getHeight() - 10.0f);

    }

    void resized() override
    {
        auto area  = getLocalBounds();
        float w    = static_cast<float> (area.getWidth());
        float h    = static_cast<float> (area.getHeight());
    
        area.removeFromTop (h * 0.05f);
    
        int totalW       = area.getWidth();
        auto gridArea    = area.removeFromLeft (static_cast<int> (totalW * splitRatio));
        auto sidebarArea = area.reduced (w * 0.01f, h * 0.01f);
    
        // Reserve label margins as percentages
        int labelLeft = static_cast<int> (w * 0.04f);
        int labelTop  = static_cast<int> (h * 0.03f);
    
        int availableW = gridArea.getWidth()  - labelLeft;
        int availableH = gridArea.getHeight() - labelTop;
    
        cellSize = std::min (availableW, availableH) / ProjectConfig::numOperators;
        gridX    = gridArea.getX() + labelLeft;
        gridY    = gridArea.getY() + labelTop;
    
        // Textbox sizing — legible relative to cell size
        int textBoxW = static_cast<int> (cellSize * 0.85f);
        int textBoxH = juce::jlimit (12, 22, static_cast<int> (cellSize * 0.22f));
    
        int idx = 0;
        for (int src = 0; src < ProjectConfig::numOperators; ++src)
            for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
                if (auto* s = matrixSliders[idx++])
                {
                    s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
                    s->setBounds (gridX + dest * cellSize,
                                  gridY + src * cellSize,
                                  cellSize - 4, cellSize - 2);
                }
    
        if (paramPrefix == "MOD_")
        {
            int slotH = sidebarArea.getHeight() / 6;
            for (auto& slot : modSlots)
                slot->setBounds (sidebarArea.removeFromTop (slotH));
        }
        else
        {
            int rowH    = sidebarArea.getHeight() / ProjectConfig::numOperators;
            int labelW  = static_cast<int> (sidebarArea.getWidth() * 0.35f);
            for (int i = 0; i < ProjectConfig::numOperators; ++i)
            {
                auto cell = sidebarArea.removeFromTop (rowH).reduced (0, h * 0.005f);
                if (auto* lb = outputLabels[i])  lb->setBounds (cell.removeFromLeft (labelW));
                if (auto* sl = outputSliders[i])
                {
                    sl->setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                                         cell.getWidth(), static_cast<int> (rowH * 0.25f));
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
    int cellSize  = 90;
    static constexpr float splitRatio = 0.65f;
    juce::OwnedArray<juce::Slider>     matrixSliders;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> matrixAttachments;
    std::vector<std::unique_ptr<ModMatrixSlot>> modSlots;
    juce::OwnedArray<juce::Slider>     outputSliders;
    juce::OwnedArray<juce::Label>      outputLabels;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachments;
};
