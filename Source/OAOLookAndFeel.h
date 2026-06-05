#pragma once
#include <JuceHeader.h>
#include "OAOColors.h"

class OAOLookAndFeel : public juce::LookAndFeel_V4
{
public:
    float currentScale = 1.0f;
    OAOLookAndFeel (OAOColors& c) : colors (c)
    {
        applyColors();
    }

    // Text scaling handling. first the default
    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return juce::FontOptions (juce::jlimit (9.0f, 16.0f, box.getHeight() * 0.45f) * currentScale);
    }
    
    juce::Font getPopupMenuFont() override
    {
        return juce::FontOptions (13.0f * currentScale); // popups stay fixed, just scale
    }
    
    juce::Font getLabelFont (juce::Label& label) override
    {
        return juce::FontOptions (juce::jlimit (8.0f, 14.0f, label.getHeight() * 0.7f) * currentScale);
    }
    
    juce::Font getTextButtonFont (juce::TextButton& button, int buttonHeight) override
    {
        float baseSize = juce::jmin (14.0f * currentScale, buttonHeight * 0.6f);
        return juce::FontOptions (juce::jlimit (9.0f, 18.0f, baseSize));
    }

    void applyColors()
    {
        // Set JUCE color IDs to match our theme
        setColour (juce::ResizableWindow::backgroundColourId,     colors.background);
        setColour (juce::DocumentWindow::backgroundColourId,      colors.background);
        // Sliders
        setColour (juce::Slider::backgroundColourId,              colors.surface);
        setColour (juce::Slider::thumbColourId,                   colors.primary);
        setColour (juce::Slider::trackColourId,                   colors.secondary.withAlpha (0.4f));
        setColour (juce::Slider::rotarySliderFillColourId,        colors.primary);
        setColour (juce::Slider::rotarySliderOutlineColourId,     colors.surface);
        setColour (juce::Slider::textBoxTextColourId,             colors.text);
        setColour (juce::Slider::textBoxBackgroundColourId,       colors.background);
        setColour (juce::Slider::textBoxOutlineColourId,          colors.primary.withAlpha (0.3f));
        setColour (juce::Slider::textBoxHighlightColourId,        colors.primary.withAlpha (0.3f));
        // TextButton
        setColour (juce::TextButton::buttonColourId,              colors.surface);
        setColour (juce::TextButton::buttonOnColourId,            colors.primary.withAlpha (0.3f));
        setColour (juce::TextButton::textColourOffId,             colors.text);
        setColour (juce::TextButton::textColourOnId,              colors.primary);
	// ComboBox
        setColour (juce::ComboBox::backgroundColourId,            colors.surface);
        setColour (juce::ComboBox::textColourId,                  colors.text);
        setColour (juce::ComboBox::outlineColourId,               colors.primary.withAlpha (0.4f));
        setColour (juce::ComboBox::buttonColourId,                colors.primary.withAlpha (0.2f));
        setColour (juce::ComboBox::arrowColourId,                 colors.primary);
	// PopupMenu
        setColour (juce::PopupMenu::backgroundColourId,           colors.surface);
        setColour (juce::PopupMenu::textColourId,                 colors.text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colors.primary.withAlpha (0.3f));
        setColour (juce::PopupMenu::highlightedTextColourId,      colors.primary);
        // Labels
        setColour (juce::Label::textColourId,                     colors.text);
        setColour (juce::Label::backgroundColourId,               juce::Colours::transparentBlack);
        // ToggleButton
        setColour (juce::ToggleButton::textColourId,              colors.text);
        setColour (juce::ToggleButton::tickColourId,              colors.primary);
        setColour (juce::ToggleButton::tickDisabledColourId,      colors.textDim);
    }

    // Custom rotary knob
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override
    {
        float radius    = juce::jmin (width / 2.0f, height / 2.0f) - 4.0f;
        float centreX   = x + width  * 0.5f;
        float centreY   = y + height * 0.5f;
        float angle     = rotaryStartAngle + sliderPosProportional 
                          * (rotaryEndAngle - rotaryStartAngle);

        // --- Outer glow ring ---
        float glowRadius = radius + 3.0f;
        juce::ColourGradient glowGrad (
            colors.primary.withAlpha (0.4f), centreX, centreY,
            colors.primary.withAlpha (0.0f), centreX + glowRadius, centreY, true);
        g.setGradientFill (glowGrad);
        g.fillEllipse (centreX - glowRadius, centreY - glowRadius,
                       glowRadius * 2.0f, glowRadius * 2.0f);
        // --- Background circle ---
        g.setColour (colors.surface);
        g.fillEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);
        // --- Arc track ---
        juce::Path track;
        track.addCentredArc (centreX, centreY, radius - 3.0f, radius - 3.0f,
                              0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (colors.textDim.withAlpha (0.4f));
        g.strokePath (track, juce::PathStrokeType (2.0f));
        // --- Filled arc (value) ---
        juce::Path arc;
        arc.addCentredArc (centreX, centreY, radius - 3.0f, radius - 3.0f,
                           0.0f, rotaryStartAngle, angle, true);
        juce::ColourGradient arcGrad (
            colors.secondary, centreX - radius, centreY,
            colors.primary,   centreX + radius, centreY, false);
        g.setGradientFill (arcGrad);
        g.strokePath (arc, juce::PathStrokeType (2.5f,
                           juce::PathStrokeType::curved,
                           juce::PathStrokeType::rounded));
        // --- Outer ring ---
        g.setColour (colors.primary.withAlpha (0.6f));
        g.drawEllipse (centreX - radius, centreY - radius,
                       radius * 2.0f, radius * 2.0f, 1.5f);
        // --- Indicator dot ---
        float dotRadius  = 3.0f;
        float dotX       = centreX + (radius - 8.0f) * std::cos (angle - juce::MathConstants<float>::halfPi);
        float dotY       = centreY + (radius - 8.0f) * std::sin (angle - juce::MathConstants<float>::halfPi);
        // Dot glow
        juce::ColourGradient dotGlow (
            colors.primary.withAlpha (0.8f), dotX, dotY,
            colors.primary.withAlpha (0.0f), dotX + dotRadius * 3.0f, dotY, true);
        g.setGradientFill (dotGlow);
        g.fillEllipse (dotX - dotRadius * 2.0f, dotY - dotRadius * 2.0f,
                       dotRadius * 4.0f, dotRadius * 4.0f);
        // Dot center
        g.setColour (colors.primary);
        g.fillEllipse (dotX - dotRadius, dotY - dotRadius,
                       dotRadius * 2.0f, dotRadius * 2.0f);
    }

    // Custom button
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour&, bool isHighlighted,
                               bool isDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        bool isOn   = button.getToggleState();
        // Background
        if (isOn)
        {
            juce::ColourGradient grad (
                colors.primary.withAlpha (0.3f), bounds.getX(), bounds.getY(),
                colors.secondary.withAlpha (0.1f), bounds.getRight(), bounds.getBottom(), false);
            g.setGradientFill (grad);
        }
        else
        {
            g.setColour (isHighlighted ? colors.surface.brighter (0.1f) : colors.surface);
        }
        g.fillRoundedRectangle (bounds, 3.0f);
        // Border
        g.setColour (isOn ? colors.primary : colors.textDim.withAlpha (0.4f));
        g.drawRoundedRectangle (bounds, 3.0f, isOn ? 1.5f : 1.0f);
        // Glow on active
        if (isOn)
        {
            g.setColour (colors.primary.withAlpha (0.15f));
            g.drawRoundedRectangle (bounds.expanded (1.5f), 4.0f, 1.5f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool, bool) override
    {
        bool isOn = button.getToggleState();
        g.setColour (isOn ? colors.primary : colors.text);
        g.setFont (juce::Font (juce::FontOptions (15.0f)));
        g.drawText (button.getButtonText(), button.getLocalBounds(),
                    juce::Justification::centred);
    }

    // Custom linear slider
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearHorizontal)
        {
            float trackY  = y + height * 0.5f;
            float trackH  = 3.0f;

            // Track background
            g.setColour (colors.surface);
            g.fillRoundedRectangle (static_cast<float> (x), trackY - trackH * 0.5f,
                                    static_cast<float> (width), trackH, trackH * 0.5f);
            // Filled portion
            juce::ColourGradient grad (
                colors.secondary, static_cast<float> (x), trackY,
                colors.primary,   sliderPos, trackY, false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (static_cast<float> (x), trackY - trackH * 0.5f,
                                    sliderPos - x, trackH, trackH * 0.5f);
            // Thumb
            float thumbR = 6.0f;
            g.setColour (colors.primary);
            g.fillEllipse (sliderPos - thumbR, trackY - thumbR, thumbR * 2.0f, thumbR * 2.0f);
            g.setColour (colors.background);
            g.fillEllipse (sliderPos - thumbR * 0.5f, trackY - thumbR * 0.5f,
                           thumbR, thumbR);
        }
        else
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height,
                                                     sliderPos, minSliderPos,
                                                     maxSliderPos, style, slider);
        }
    }
    OAOColors& colors;
};
