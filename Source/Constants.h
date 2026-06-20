//Constants.h
#pragma once
#include <JuceHeader.h>

namespace ProjectConfig
{
    // Operator count
    static constexpr int numOperators = 6;
    // ModSlots, for FMVoice and FMOperator
    static constexpr int numModSlots = 6;
    // EffectSlots, for EffectsPage
    static constexpr int numEffects = 3;
    // Base plugin size
    static constexpr int pluginSizeX = 900;
    static constexpr int pluginSizeY = 800;
    // Effects list
    inline juce::StringArray getEffectTypeChoices()
    {
        return { "None", "Lowpass", "Highpass", "Bandpass", "Filter Drive", "Comb", "Formant", "Compressor", "3-bar EQ", "OTT", "Lofi", "Tape", "Chorus", "Old Chorus", "Distortion", "Bitcrush", "Ring Mod", "AP Reverb", "AP Delay", "Timeshift Delay", "Shimmer Delay", "DJFX Delay", "Scatter", "Granular", "Color Bass", "Spectral Freeze" };
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
	    case 23: return { "Grain Size","Scatter",  "Damping",   "Feedback" }; // Granular
	    case 24: return { "Drive",     "Shimmer",  "Tone",      "Decay"    }; // Color Bass
            case 25: return { "Freeze On", "Phase Rand",    "Pitch",     "Blur"     }; // Spectral Freeze
	    default: return { "A",         "B",        "C",         "D"        };
	}
    }
}

namespace ModChoices // Choices for LFOs
{
    inline juce::StringArray sources()
    {
        return { "None", "Op 1", "Op 2", "Op 3", "Op 4", "Op 5", "Op 6", "FX LFO 1", "FX LFO 2", "FX LFO 3", "Velocity", "Mod Wheel" };
    }

    inline juce::StringArray targets()
    {
        juce::StringArray t;
        t.add("None");                                          // 0
        for (int op = 0; op < 6; ++op)                         // 1-30
        {
            t.add("Op " + juce::String(op+1) + " Knob A");
            t.add("Op " + juce::String(op+1) + " Knob B");
            t.add("Op " + juce::String(op+1) + " Knob C");
            t.add("Op " + juce::String(op+1) + " Knob D");
            t.add("Op " + juce::String(op+1) + " Level");
        }
        for (int fx = 0; fx < 3; ++fx)                         // 31-45
        {
            t.add("FX " + juce::String(fx+1) + " Knob A");
            t.add("FX " + juce::String(fx+1) + " Knob B");
            t.add("FX " + juce::String(fx+1) + " Knob C");
            t.add("FX " + juce::String(fx+1) + " Knob D");
            t.add("FX " + juce::String(fx+1) + " Mix");
        }
        for (int src = 0; src < 6; ++src)                      // 46-81
            for (int dst = 0; dst < 6; ++dst)
                t.add("Op " + juce::String(src+1) + " → Op " + juce::String(dst+1));
        return t;
    }

    inline juce::StringArray fxtargets()
    {
        return {
            "None",
            "FX 1 Knob A", "FX 1 Knob B", "FX 1 Knob C", "FX 1 Knob D", "FX 1 Mix",
            "FX 2 Knob A", "FX 2 Knob B", "FX 2 Knob C", "FX 2 Knob D", "FX 2 Mix",
            "FX 3 Knob A", "FX 3 Knob B", "FX 3 Knob C", "FX 3 Knob D", "FX 3 Mix",
        };
    }
}
