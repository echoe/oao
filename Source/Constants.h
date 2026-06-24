//Constants.h
#pragma once
#include <JuceHeader.h>

namespace ProjectConfig
{
    // Operator count, mod slots, effects, operator parameters, effect parameters
    static constexpr int numOperators = 6;
    static constexpr int numOpParams = 5;
    static constexpr int numFxParams = 5;
    static constexpr int numModSlots = 6;
    static constexpr int numEffects = 6;
    // Number of voices
    static constexpr int numVoices = 8;
    // Base plugin size/visual tweaks
    static constexpr int pluginSizeX = 900;
    static constexpr int pluginSizeY = 720;
    static constexpr float outerMargin = 0.005f;
    static constexpr float textBoxWidthFraction  = 1.0f; // Generally meaningless with our design.
    // knobDiameter and textboxheightfraction live in OAOColors.h now so end users can change them.
    // Effects list
    inline juce::StringArray getEffectTypeChoices()
    {
        return { "None", "Lowpass", "Highpass", "Bandpass", "Filter Drive", "Comb", "Formant", "Compressor", "3-bar EQ", "OTT", "Lofi", "Tape", "Chorus", "Old Chorus", "Distortion", "Bitcrush", "Ring Mod", "AP Reverb", "AP Delay", "Timeshift Delay", "Shimmer Delay", "DJFX Delay", "Scatter", "Granular", "Color Bass", "Spectral Freeze", "Looper" };
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
            case 25: return { "Freeze On", "Phase Rnd","Pitch",     "Blur"     }; // Spectral Freeze
            case 26: return { "Stop/Play","Rec/Pass/Dub","Decay",   "Fade"     }; // Looper
	    default: return { "A",         "B",        "C",         "D"        };
	}
    }
}

namespace ModChoices // Choices for LFOs
{
    inline juce::StringArray sources()
    {
        return { "None", "Op 1", "Op 2", "Op 3", "Op 4", "Op 5", "Op 6", "FX LFO 1", "FX LFO 2", "FX LFO 3", "FX LFO 4", "FX LFO 5", "FX LFO 6", "Velocity", "Mod Wheel" };
    }

    inline juce::StringArray targets()
    {
        juce::StringArray t;
        t.add("None");                                          // 0
        for (int op = 0; op < ProjectConfig::numOperators; ++op)                         // 1-30
        {
            t.add("Op " + juce::String(op+1) + " Knob A");
            t.add("Op " + juce::String(op+1) + " Knob B");
            t.add("Op " + juce::String(op+1) + " Knob C");
            t.add("Op " + juce::String(op+1) + " Knob D");
            t.add("Op " + juce::String(op+1) + " Level");
        }
        for (int fx = 0; fx < ProjectConfig::numEffects; ++fx)                         // 31-60
        {
            t.add("FX " + juce::String(fx+1) + " Knob A");
            t.add("FX " + juce::String(fx+1) + " Knob B");
            t.add("FX " + juce::String(fx+1) + " Knob C");
            t.add("FX " + juce::String(fx+1) + " Knob D");
            t.add("FX " + juce::String(fx+1) + " Mix");
        }
        for (int src = 0; src < ProjectConfig::numOperators; ++src)                      // 46-81
            for (int dst = 0; dst < ProjectConfig::numOperators; ++dst)
                t.add("Op " + juce::String(src+1) + " → Op " + juce::String(dst+1));
        return t;
    }

    inline juce::StringArray fxtargets()
    {
        juce::StringArray fxt;
	fxt.add("None");
        for (int fx = 0; fx < ProjectConfig::numEffects; ++fx)                         // 31-60
        {
            fxt.add("FX " + juce::String(fx+1) + " Knob A");
            fxt.add("FX " + juce::String(fx+1) + " Knob B");
            fxt.add("FX " + juce::String(fx+1) + " Knob C");
            fxt.add("FX " + juce::String(fx+1) + " Knob D");
            fxt.add("FX " + juce::String(fx+1) + " Mix");
        }
	return fxt;
    }
    inline void buildTargetMenu (juce::ComboBox& targetSelector)
    {
        targetSelector.clear (juce::dontSendNotification);
        targetSelector.addItem ("None", 1);

        // Operator params — nested by operator
        juce::PopupMenu opMenu;
        for (int op = 0; op < ProjectConfig::numOperators; ++op)
        {
            juce::PopupMenu sub;
            int base = op * 5 + 2; // offset by 1 for None, +1 for ComboBox 1-indexing
            juce::String opName = "Op " + juce::String (op + 1) + " ";
            sub.addItem (base + 0, opName + "Knob A");
            sub.addItem (base + 1, opName + "Knob B");
            sub.addItem (base + 2, opName + "Knob C");
            sub.addItem (base + 3, opName + "Knob D");
            sub.addItem (base + 4, opName + "Level");
            opMenu.addSubMenu ("Op " + juce::String (op + 1), sub);
        }
        targetSelector.getRootMenu()->addSubMenu ("Operators", opMenu);

        // FX params — nested by slot
        juce::PopupMenu fxMenu;
        for (int fx = 0; fx < ProjectConfig::numEffects; ++fx)
        {
            juce::PopupMenu sub;
            int base = fx * 5 + 32;
            juce::String fxName = "FX " + juce::String (fx + 1) + " ";
            sub.addItem (base + 0, fxName + "Knob A");
            sub.addItem (base + 1, fxName + "Knob B");
            sub.addItem (base + 2, fxName + "Knob C");
            sub.addItem (base + 3, fxName + "Knob D");
            sub.addItem (base + 4, fxName + "Mix");
            fxMenu.addSubMenu ("FX " + juce::String (fx + 1), sub);
        }
        targetSelector.getRootMenu()->addSubMenu ("Effects", fxMenu);

        // FM Matrix — nested by source operator
        juce::PopupMenu matrixMenu;
        for (int src = 0; src < ProjectConfig::numOperators; ++src)
        {
            juce::PopupMenu sub;
            juce::String srcName = "Op " + juce::String (src + 1) + " -> ";
            for (int dst = 0; dst < ProjectConfig::numOperators; ++dst)
            {
                int id = 62 + src * ProjectConfig::numOperators + dst; // 61 entries before this + 1 for 1-indexing
                sub.addItem (id, srcName + "Op " + juce::String (dst + 1));
            }
            matrixMenu.addSubMenu ("Op " + juce::String (src + 1), sub);
        }
        targetSelector.getRootMenu()->addSubMenu ("FM Matrix", matrixMenu);
    }
}
