#pragma once
#include <JuceHeader.h>

struct OAOColors
{
    // Default synthwave palette
    juce::Colour background  { 0xFF0D0D1A }; // deep purple-black
    juce::Colour primary     { 0xFFFF2D9B }; // neon pink
    juce::Colour secondary   { 0xFF00F5FF }; // cyan
    juce::Colour surface     { 0xFF1A1A2E }; // slightly lighter background
    juce::Colour text        { 0xFFE0E0FF }; // soft white-blue
    juce::Colour textDim     { 0xFF6060A0 }; // dimmed text

    void saveToFile()
    {
        juce::PropertiesFile::Options options;
        options.applicationName     = "OAO";
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";

        juce::PropertiesFile prefs (options);
        prefs.setValue ("background",  background.toString());
        prefs.setValue ("primary",     primary.toString());
        prefs.setValue ("secondary",   secondary.toString());
        prefs.setValue ("surface",     surface.toString());
        prefs.setValue ("text",        text.toString());
        prefs.setValue ("textDim",     textDim.toString());
        prefs.saveIfNeeded();
    }

    void loadFromFile()
    {
        juce::PropertiesFile::Options options;
        options.applicationName     = "OAO";
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";

        juce::PropertiesFile prefs (options);
        if (prefs.containsKey ("background"))
        {
            background = juce::Colour::fromString (prefs.getValue ("background"));
            primary    = juce::Colour::fromString (prefs.getValue ("primary"));
            secondary  = juce::Colour::fromString (prefs.getValue ("secondary"));
            surface    = juce::Colour::fromString (prefs.getValue ("surface"));
            text       = juce::Colour::fromString (prefs.getValue ("text"));
            textDim    = juce::Colour::fromString (prefs.getValue ("textDim"));
        }
    }

    // Built-in presets
    void setSynthwave() 
    {
        background = juce::Colour (0xFF0D0D1A);
        primary    = juce::Colour (0xFFFF2D9B);
        secondary  = juce::Colour (0xFF00F5FF);
        surface    = juce::Colour (0xFF1A1A2E);
        text       = juce::Colour (0xFFE0E0FF);
        textDim    = juce::Colour (0xFF6060A0);
    }

    void setIndustrial()
    {
        background = juce::Colour (0xFF141414);
        primary    = juce::Colour (0xFFFFAA00);
        secondary  = juce::Colour (0xFFFF6600);
        surface    = juce::Colour (0xFF1E1E1E);
        text       = juce::Colour (0xFFDDDDDD);
        textDim    = juce::Colour (0xFF666666);
    }

    void setMinimal()
    {
        background = juce::Colour (0xFF0A0A0A);
        primary    = juce::Colour (0xFFFFFFFF);
        secondary  = juce::Colour (0xFFAAAAAA);
        surface    = juce::Colour (0xFF141414);
        text       = juce::Colour (0xFFFFFFFF);
        textDim    = juce::Colour (0xFF555555);
    }

    void setWarmAnalog()
    {
        background = juce::Colour (0xFF1A0F00);
        primary    = juce::Colour (0xFFFF8C42);
        secondary  = juce::Colour (0xFFFFD166);
        surface    = juce::Colour (0xFF261500);
        text       = juce::Colour (0xFFFFE8CC);
        textDim    = juce::Colour (0xFF806040);
    }
    void setMintyBreeze()
    {
        background = juce::Colour (0xFFF4FDFF);
        primary    = juce::Colour (0xFFBAE8E8);
        secondary  = juce::Colour (0xFF2C698D);
        surface    = juce::Colour (0xFFE3F6F5);
        text       = juce::Colour (0xFF272643);
        textDim    = juce::Colour (0xFF667788);
    }

    void setPeachBlossom()
    {
        background = juce::Colour (0xFFFFF5E4);
        primary    = juce::Colour (0xFFFFD1D1);
        secondary  = juce::Colour (0xFFFF9494);
        surface    = juce::Colour (0xFFFFE3E1);
        text       = juce::Colour (0xFF5C3D2E);
        textDim    = juce::Colour (0xFF997766);
    }

    void setLavenderHaze()
    {
        background = juce::Colour (0xFFF3E8FF);
        primary    = juce::Colour (0xFFDCBFFF);
        secondary  = juce::Colour (0xFFD0A2F7);
        surface    = juce::Colour (0xFFE5D4FF);
        text       = juce::Colour (0xFF4A306D);
        textDim    = juce::Colour (0xFF8866AA);
    }

    void setNordicMorning()
    {
        background = juce::Colour (0xFFF0F4F8);
        primary    = juce::Colour (0xFFBCCCDC);
        secondary  = juce::Colour (0xFFF57B51);
        surface    = juce::Colour (0xFFD9E2EC);
        text       = juce::Colour (0xFF102A43);
        textDim    = juce::Colour (0xFF667788);
    }
};
