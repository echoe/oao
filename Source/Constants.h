//Constants.h
#pragma once
#include <JuceHeader.h>

namespace ProjectConfig
{
    // Operator count
    static constexpr int numOperators = 6;
    // ModSlots, for FMVoice and FMOperator
    static constexpr int numModSlots = 6;
    // Base plugin size
    static constexpr int pluginSizeX = 950;
    static constexpr int pluginSizeY = 680;
    // Filters list
    inline juce::StringArray getFilterTypeChoices()
    {
        return { "None", "Lowpass", "Highpass", "Bandpass", "Comb", "Granular", "Formant", "Tape" };
    }
    // Filters knobs list
    inline std::array<const char*, 4> getFilterKnobLabels (int filterTypeIndex)
    {
        switch (filterTypeIndex)
        {
            case 0:  return { "--",         "--",        "--",        "--"      }; // None
            case 1:  return { "Cutoff",     "Resonance", "Keytrack",  "Feedback"}; // Lowpass
            case 2:  return { "Cutoff",     "Resonance", "Keytrack",  "Feedback"}; // Highpass
            case 3:  return { "Cutoff",     "Resonance", "Keytrack",  "Feedback"}; // Bandpass
            case 4:  return { "Cutoff",     "Feedback",  "Keytrack",  "Damping" }; // Comb
            case 5:  return { "Grain Size", "Damping",   "Scatter",   "Feedback"}; // Granular
            case 6:  return { "Vowel",      "Nasal",     "Vowel Mod", "Drive"   }; // Formant
	    case 7:  return { "Wobble",     "Age",       "Saturation","Bias"    }; // Tape
            default: return { "A",          "B",         "C",         "D"       };
        }
    }
}
