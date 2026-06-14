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
    // Effects list
    inline juce::StringArray getEffectTypeChoices()
    {
        return { "None", "Lowpass", "Highpass", "Bandpass", "Filter Drive", "Comb", "Formant", "Compressor", "3-bar EQ", "OTT", "Lofi", "Tape", "Chorus", "Old Chorus", "Distortion", "Bitcrush", "Ring Mod", "AP Reverb", "AP Delay", "Timeshift Delay", "Shimmer Delay", "DJFX Delay", "Scatter", "Granular", "Color Bass", "Spectral Freeze" };
    }
    // FX LFO target list — 15 entries (3 slots x 5 params), index 0 = None
    // Order must stay in sync with the fxRatioModSum/fxDetuneModSum/etc. arrays
    // and with the dispatch in PluginProcessor (fxBase = tgtIdx - 1, fxSlot = fxBase/5, fxParam = fxBase%5)
    inline juce::StringArray getFXLFOTargetChoices()
    {
        return {
            "None",
            "FX 1 Knob A", "FX 1 Knob B", "FX 1 Knob C", "FX 1 Knob D", "FX 1 Mix",
            "FX 2 Knob A", "FX 2 Knob B", "FX 2 Knob C", "FX 2 Knob D", "FX 2 Mix",
            "FX 3 Knob A", "FX 3 Knob B", "FX 3 Knob C", "FX 3 Knob D", "FX 3 Mix",
        };
    }

    // Effects knobs list
    inline std::array<const char*, 4> getEffectKnobLabels (int effectTypeIndex)
    {
        switch (effectTypeIndex)
        {
            case 0:  return { "--",        "--",       "--",        "--"       }; // None
            case 1:  return { "Cutoff",    "Resonance","Keytrack",  "Feedback" }; // Lowpass
            case 2:  return { "Cutoff",    "Resonance","Keytrack",  "Feedback" }; // Highpass
            case 3:  return { "Cutoff",    "Resonance","Keytrack",  "Feedback" }; // Bandpass
            case 4:  return { "Cutoff",    "Resonance","Overdrive", "Mode"     }; // Filter w/Drive
            case 5:  return { "Cutoff",    "Damping",  "Keytrack",  "Feedback" }; // Comb
            case 6:  return { "Vowel",     "Nasal",    "Vowel Mod", "Drive"    }; // Formant. End of effects
	    case 7:  return { "Threshold", "Ratio",    "Attack",    "Release"  }; // Compressor. EQ-ish ...
            case 8:  return { "Low Gain",  "Mid Gain", "High Gain", "Gain"     }; // 3-bar EQ.
            case 9:  return { "Depth",     "Time",     "Upward",    "Tone"     }; // OTT
            case 10: return { "Decimate",  "Bits",     "Wear",      "Tone"     }; // Lofi
	    case 11: return { "Wobble",    "Age",      "Saturation","Bias"     }; // Tape
            case 12: return { "Rate",      "Depth",    "Spread",    "Voices"   }; // Chorus. Normal effects
	    case 13: return { "Rate",      "Depth",    "Mode",      "Warmth"   }; // Old Chorus
	    case 14: return { "Drive",     "Flavor",   "Tone",      "Degrade"  }; // Distortion
	    case 15: return { "Bits",      "Rate",     "Jitter",    "Noise"    }; // Bitcrush
            case 16: return { "Frequency", "Shape",    "Depth",     "Feedback" }; // Ring Mod
	    case 17: return { "Size",      "Decay",    "Diffusion", "Damping"  }; // AP Reverb
	    case 18: return { "Time",      "Feedback", "Diffusion", "Damping"  }; // AP Delay. The delays
            case 19: return { "Time",      "Feedback", "Damping",   "Drive"    }; // Time-shifting Delay
            case 20: return { "Time",      "Feedback", "Shimmer",   "Diffusion"}; // Ambient Delay 
            case 21: return { "Buffer",    "Speed",    "Loop On",   "Drift"    }; // DJFX Delay
	    case 22: return { "Type",      "Size",     "Speed",     "Depth"    }; // Scatter. More strange effects
	    case 23: return { "Grain Size","Damping",  "Scatter",   "Feedback" }; // Granular
	    case 24: return { "Drive",     "Shimmer",  "Tone",      "Decay"    }; // Color Bass
            case 25: return { "Freeze On", "Blend",    "Pitch",     "Blur"     }; // Spectral Freeze
	    default: return { "A",         "B",        "C",         "D"        };
	}
    }
}
