#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class Oscilloscope : public juce::Component,
                     public juce::Timer
{
public:
    Oscilloscope (FMPluginAudioProcessor& p) : processor (p)
    {
        startTimerHz (30);
    }

    void timerCallback() override { repaint(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black);

        // Centre line
        g.setColour (juce::Colours::darkgrey);
        g.drawHorizontalLine (getHeight() / 2, 0.0f, (float)getWidth());

        // Waveform
        g.setColour (juce::Colours::limegreen);

        const auto& buf     = processor.scopeBuffer;
        int         readPos = processor.scopeWritePos.load (std::memory_order_relaxed);
        int         size    = (int)buf.size();
        float       w       = (float)getWidth();
        float       h       = (float)getHeight();

        juce::Path path;
        bool started = false;

        for (int i = 0; i < size; ++i)
        {
            int   idx    = (readPos + i) % size;
            float sample = buf[idx];
            float x      = (float)i / (float)(size - 1) * w;
            float y      = (h / 2.0f) - (sample * h / 2.0f * 0.9f);

            if (!started) { path.startNewSubPath (x, y); started = true; }
            else            path.lineTo (x, y);
        }

        g.strokePath (path, juce::PathStrokeType (1.0f));

        // Border
        g.setColour (juce::Colours::darkgrey.brighter (0.2f));
        g.drawRect (getLocalBounds(), 1);
    }

private:
    FMPluginAudioProcessor& processor;
};
