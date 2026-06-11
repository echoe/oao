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
    static constexpr int pluginSizeX = 1000;
    static constexpr int pluginSizeY = 700;
    // Filters list
    inline juce::StringArray getFilterTypeChoices()
    {
        return { "None", "Lowpass", "Highpass", "Bandpass", "Comb", "Granular", "Formant", "Tape", "Bitcrush", "AP Delay", "AP Reverb", "Compressor", "Scatter", "Ring Mod", "Chorus", "Distortion", "DJFX Delay", "Color Bass", "Ambient Shimmer", "Old Chorus", "OTT", "Spectral Freeze" };
    }
    // Filters knobs list
    inline std::array<const char*, 4> getFilterKnobLabels (int filterTypeIndex)
    {
        switch (filterTypeIndex)
        {
            case 0:  return { "--",        "--",       "--",        "--"       }; // None
            case 1:  return { "Cutoff",    "Resonance","Keytrack",  "Feedback" }; // Lowpass
            case 2:  return { "Cutoff",    "Resonance","Keytrack",  "Feedback" }; // Highpass
            case 3:  return { "Cutoff",    "Resonance","Keytrack",  "Feedback" }; // Bandpass
            case 4:  return { "Cutoff",    "Damping",  "Keytrack",  "Feedback" }; // Comb
            case 5:  return { "Grain Size","Damping",  "Scatter",   "Feedback" }; // Granular
            case 6:  return { "Vowel",     "Nasal",    "Vowel Mod", "Drive"    }; // Formant
	    case 7:  return { "Wobble",    "Age",      "Saturation","Bias"     }; // Tape
            case 8:  return { "Bits",      "Rate",     "Jitter",    "Noise"    }; // Bitcrush
	    case 9:  return { "Time",      "Feedback", "Diffusion", "Damping"  }; // AP Delay
	    case 10: return { "Size",      "Decay",    "Diffusion", "Damping"  }; // AP Reverb
	    case 11: return { "Threshold", "Ratio",    "Attack",    "Release"  }; // Compressor
	    case 12: return { "Type",      "Size",     "Speed",     "Depth"    }; // Scatter
	    case 13: return { "Frequency", "Shape",    "Depth",     "Feedback" }; // Ring Mod
	    case 14: return { "Rate",      "Depth",    "Spread",    "Voices"   }; // Chorus
            case 15: return { "Drive",     "Flavor",   "Tone",      "Degrade"  }; // Distortion
            case 16: return { "Buffer",    "Speed",    "Loop On",   "Drift"    }; // DJFX Delay
            case 17: return { "Drive",     "Shimmer",  "Tone",      "Decay"    }; // Color Bass
	    case 18: return { "Time",      "Feedback", "Shimmer",   "Diffusion"}; // Ambient Shimmer
            case 19: return { "Rate",      "Depth",    "Mode",      "Warmth"   }; // Old Chorus
            case 20: return { "Depth",     "Time",     "Upward",    "Tone"     }; // OTT
            case 21: return { "Freeze On", "Blend",    "Pitch",     "Blur"     }; // Spectral Freeze
	    default: return { "A",         "B",        "C",         "D"        };
	}
    }
}
