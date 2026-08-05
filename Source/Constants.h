//Constants.h
#pragma once
#include <JuceHeader.h>

namespace ProjectConfig
{
    // Operator count, mod slots, effects, operator parameters, effect parameters
    static constexpr int numOperators = 8;
    static constexpr int numOpParams = 5;
    static constexpr int numFxParams = 5;
    static constexpr int numVisModSlots = 4; //visible mod slots in the Operatorspage
    static constexpr int numModSlots = 8; //generally should be same as numOperators for legibility
    static constexpr int numEffects = 4; //FX LFOs follow this setting as well
    static constexpr int numEnvelopes = 4; // shared envelope generator pool; operators pick one via ENV_SRC_N

    // --- Unified modulation depth ---
    // for modulation we scale by a per-target "full swing" range so 1.0 is a good range for all knobs
    // Index order matches operator/FX param layout: 0=Ratio,1=Detune,2=Phase,3=Fold,4=Level/Mix
    static constexpr float modRangeForParam[5] = { 4.0f, 20.0f, 180.0f, 1.0f, 1.0f };

    // FM matrix cells live in a 0..1 "how much FM" space (see maxFmModulationIndex
    // below), so a full-swing modulator can move a cell across its whole usable range.
    static constexpr float modRangeForMatrixCell = 1.0f;

    // --- DX7-style FM depth ---
    // FM matrix cell parameters (MOD_src_dest) are normalized 0..1.
    // ~8*pi is the commonly used approximation for that ceiling 
    static constexpr float maxFmModulationIndex = 8.0f * juce::MathConstants<float>::pi; // ~25.13

    // Self-feedback (an operator modulating itself) is a recursive loop periodic in 2*pi
    // so we set that index range appropriately here.
    static constexpr float maxFmSelfFeedbackIndex = 2.0f * juce::MathConstants<float>::pi; // ~6.28
    // Number of voices
    static constexpr int numVoices = 8;
    // Base plugin size/visual tweaks
    static constexpr int pluginSizeX = 1200;
    static constexpr int pluginSizeY = 800;
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
        juce::StringArray s;
        s.add("None");
        for (int op=0; op < ProjectConfig::numOperators; ++op)
        {
            s.add("Op " + juce::String(op+1));
        }
        for (int fx = 0; fx < ProjectConfig::numEffects; ++fx)
        {
            s.add("FX LFO " + juce::String(fx+1));
        }
        s.add("Velocity");
        s.add("Mod Wheel");
        return s;
    }

    inline juce::StringArray targets()
    {
        juce::StringArray t;
        t.add("None");
        for (int op = 0; op < ProjectConfig::numOperators; ++op)
        {
            t.add("Op " + juce::String(op+1) + " Knob A");
            t.add("Op " + juce::String(op+1) + " Knob B");
            t.add("Op " + juce::String(op+1) + " Knob C");
            t.add("Op " + juce::String(op+1) + " Knob D");
            t.add("Op " + juce::String(op+1) + " Level");
        }
        for (int fx = 0; fx < ProjectConfig::numEffects; ++fx)
        {
            t.add("FX " + juce::String(fx+1) + " Knob A");
            t.add("FX " + juce::String(fx+1) + " Knob B");
            t.add("FX " + juce::String(fx+1) + " Knob C");
            t.add("FX " + juce::String(fx+1) + " Knob D");
            t.add("FX " + juce::String(fx+1) + " Mix");
        }
        for (int src = 0; src < ProjectConfig::numOperators; ++src)
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
            int base = fx * 5 + (ProjectConfig::numOperators * ProjectConfig::numOpParams) + 2; //operator base max, plus this base that increments through the loop
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
                int id = (ProjectConfig::numOperators * ProjectConfig::numOpParams) + 2 + (ProjectConfig::numEffects * ProjectConfig::numFxParams) + src * ProjectConfig::numOperators + dst; 
		// operator base max from previous entry, plus this base that increments through the loop
                sub.addItem (id, srcName + "Op " + juce::String (dst + 1));
            }
            matrixMenu.addSubMenu ("Op " + juce::String (src + 1), sub);
        }
        targetSelector.getRootMenu()->addSubMenu ("FM Matrix", matrixMenu);
    }
}

namespace FMAlgorithms
{
    struct FMAlgorithm 
        {
            int id;
            std::vector<std::pair<int, int>> connections; 
        };
    
        // An algorithm is:
        // {[number]. { {[from],[to]}, {[from],[to]} } }, where [from] is the first operator and [to] is the second, or 0[audio out].
        // DX Algos from https://www.righto.com/2021/12/yamaha-dx7-chip-reverse-engineering.html
        // DN Algos from https://support.elektron.se/support/solutions/articles/43000566579-algorithms
        inline const std::vector<FMAlgorithm> getClassicAlgorithms()
        {
            return {
                { 1, { {6,6}, {6,5}, {5,4}, {4,3}, {3,0}, {2,1}, {1,0} } },
                { 2, { {6,5}, {5,4}, {4,3}, {3,0}, {2,2}, {2,1}, {1,0} } },
                { 3, { {6,6}, {6,5}, {5,4}, {4,0}, {3,2}, {2,1}, {1,0} } },
                { 4, { {6,5}, {5,4}, {4,6}, {4,0}, {3,2}, {2,1}, {1,0} } },
                { 5, { {6,6}, {6,5}, {5,0}, {4,3}, {3,0}, {2,1}, {1,0} } },
                { 6, { {6,5}, {5,6}, {5,0}, {4,3}, {3,0}, {2,1}, {1,0} } },
                { 7, { {6,6}, {6,5}, {5,3}, {4,3}, {3,0}, {2,1}, {1,0} } },
                { 8, { {6,5}, {5,3}, {3,0}, {4,4}, {4,3}, {2,1}, {1,0} } },
                { 9, { {6,5}, {5,3}, {3,0}, {4,3}, {2,1}, {1,0}, {2,2} } },
                { 10, { {6,4}, {5,4}, {4,0}, {3,3}, {3,2}, {2,1}, {1,0} } },
                { 11, { {6,6}, {6,4}, {5,4}, {4,0}, {3,2}, {2,1}, {1,0} } },
                { 12, { {6,3}, {5,3}, {4,3}, {3,0}, {2,2}, {2,1}, {1,0} } },
                { 13, { {6,6}, {6,3}, {5,3}, {4,3}, {3,0}, {2,1}, {1,0} } },
                { 14, { {6,6}, {6,4}, {4,3}, {3,0}, {5,4}, {2,1}, {1,0} } },
                { 15, { {6,4}, {5,4}, {4,3}, {3,0}, {2,2}, {2,1}, {1,0} } },
                { 16, { {6,6}, {6,5}, {5,1}, {4,3}, {3,1}, {2,1}, {1,0} } },
                { 17, { {6,5}, {5,1}, {4,3}, {3,1}, {2,2}, {2,1}, {1,0} } },
                { 18, { {6,5}, {5,4}, {4,1}, {3,3}, {3,1}, {2,1}, {1,0} } },
                { 19, { {6,6}, {6,5}, {6,4}, {5,0}, {4,0}, {3,2}, {2,1}, {1,0} } },
                { 20, { {6,4}, {4,0}, {5,4}, {2,0}, {3,3}, {3,1}, {1,0} } },
                { 21, { {6,5}, {5,0}, {6,4}, {4,0}, {3,3}, {3,2}, {2,0}, {3,1}, {1,0} } },
                { 22, { {6,6}, {6,5}, {5,0}, {6,4}, {4,0}, {6,3}, {3,0}, {2,1}, {1,0} } },
                { 23, { {6,6}, {6,5}, {6,4}, {5,0}, {4,0}, {3,2}, {2,0}, {1,0} } },
                { 24, { {6,6}, {6,5}, {6,4}, {5,0}, {4,0}, {3,0}, {2,0}, {1,0} } },
                { 25, { {6,6}, {6,5}, {5,0}, {4,0}, {3,0}, {2,0}, {1,0} } },
                { 26, { {6,6}, {6,4}, {5,4}, {4,0}, {3,2}, {2,0}, {1,0} } },
                { 27, { {6,4}, {5,4}, {4,0}, {3,3}, {3,2}, {2,0}, {1,0} } },
                { 28, { {6,0}, {5,5}, {5,4}, {4,3}, {3,0}, {2,1}, {1,0} } },
                { 29, { {6,6}, {6,5}, {5,0}, {4,3}, {3,0}, {2,0}, {1,0} } },
                { 30, { {6,0}, {5,5}, {5,4}, {4,3}, {3,0}, {2,0}, {1,0} } },
                { 31, { {6,6}, {6,5}, {5,0}, {4,0}, {3,0}, {2,0}, {1,0} } },
                { 32, { {6,6}, {6,0}, {5,0}, {4,0}, {3,0}, {2,0}, {1,0} } }, // The end of the DX 6-op algos!
    	    { 33, { {1,1}, {1,2}, {2,0}, {4,3}, {3,2}, {3,0} } }, //DN 4-osc algos start here ...
    	    { 34, { {1,2}, {2,0}, {4,4}, {4,3}, {3,0} } },
    	    { 35, { {1,1}, {1,2}, {1,3}, {1,4}, {2,0}, {3,0}, {4,0} } },
    	    { 36, { {4,4}, {4,3}, {3,1}, {1,0}, {1,2}, {2,0} } },
    	    { 37, { {3,3}, {3,4}, {4,1}, {3,1}, {1,2}, {2,0}, {1,0} } },
    	    { 38, { {1,1}, {1,2}, {1,3}, {4,2}, {4,3}, {2,0}, {3,0} } },
    	    { 39, { {1,1}, {1,2}, {2,0}, {4,3}, {3,0}, {1,0}, {4,0} } },
    	    { 40, { {1,2}, {2,0}, {4,0}, {3,3}, {3,0} } }, // And end here
            };
        }
}
