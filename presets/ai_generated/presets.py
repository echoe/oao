import xml.etree.ElementTree as ET
import copy
import os

# Base template values from init.xml
def make_base():
    return {
        # Operator defaults
        **{f"ATTACK_{i}": "0.1" for i in range(1,7)},
        **{f"DECAY_{i}": "0.2" for i in range(1,7)},
        **{f"SUSTAIN_{i}": "0.8" for i in range(1,7)},
        **{f"RELEASE_{i}": "0.5" for i in range(1,7)},
        **{f"RATIO_{i}": "1.0" for i in range(1,7)},
        **{f"DETUNE_{i}": "0.0" for i in range(1,7)},
        **{f"PHASE_{i}": "0.0" for i in range(1,7)},
        **{f"FOLD_{i}": "0.0" for i in range(1,7)},
        **{f"MODE_{i}": "0.0" for i in range(1,7)},
        **{f"WAVE_SHAPE_{i}": "0.0" for i in range(1,7)},
        **{f"FILTER_TYPE_{i}": "0.0" for i in range(1,7)},
        **{f"TEMPO_SYNC_{i}": "0.0" for i in range(1,7)},
        **{f"OUT_{i}": "0.0" for i in range(1,7)},
        "OUT_1": "1.0",
        # FM matrix (all zeros)
        **{f"MOD_{s}_{d}": "0.0" for s in range(6) for d in range(6)},
        # Audio routing (all zeros)
        **{f"AUDIO_ROUTE_{s}_{d}": "0.0" for s in range(6) for d in range(6)},
        # Mod matrix slots
        **{f"MOD_SRC_{i}": "0.0" for i in range(1,7)},
        **{f"MOD_TGT_{i}": "0.0" for i in range(1,7)},
        **{f"MOD_AMT_{i}": "0.0" for i in range(1,7)},
        # FX slots
        **{f"FX_TYPE_{i}": "0.0" for i in range(1,4)},
        **{f"FX_MIX_{i}": "1.0" for i in range(1,4)},
        **{f"FX_RATIO_{i}": "1.0" for i in range(1,4)},
        **{f"FX_DETUNE_{i}": "0.0" for i in range(1,4)},
        **{f"FX_PHASE_{i}": "0.0" for i in range(1,4)},
        **{f"FX_FOLD_{i}": "0.0" for i in range(1,4)},
        **{f"FX_SYNC_{i}": "0.0" for i in range(1,4)},
        # Global
        "GAIN_CEIL": "-0.2",
        "WET_DRY": "1.0",
    }

def write_preset(name, params):
    root = ET.Element("Parameters")
    # Sort keys for consistency
    for key in sorted(params.keys()):
        ET.SubElement(root, "PARAM", id=key, value=str(params[key]))
    tree = ET.ElementTree(root)
    ET.indent(tree, space="  ")
    path = f"/yourpath/{name}.xml"
    tree.write(path, xml_declaration=True, encoding="UTF-8")

os.makedirs("/yourpath/", exist_ok=True)

# MODE values: 0=Wave, 1=Additive, 2=Filter, 3=Ext
# WAVE_SHAPE values: 0=Sine, 1=Triangle, 2=Saw, 3=Square, 4=Pulse, 5=SqrPWM, 6=WhiteNoise, 7=PinkNoise
# FILTER_TYPE values: 0=None,1=LP,2=HP,3=BP,4=Comb,5=Granular,6=Formant,7=Tape,8=Bitcrush,
#                     9=APDelay,10=APReverb,11=Compressor,12=Varispeed,13=Scatter,14=RingMod,
#                     15=Chorus,16=Phaser(removed),17=Distortion,18=DJFXDelay,19=HarmonicRes,
#                     20=Ambient,21=OldChorus
# FX_TYPE same as FILTER_TYPE

presets = []

# ─────────────────────────────────────────────
# 1. INIT - Clean sine, single op
# ─────────────────────────────────────────────
p = make_base()
presets.append(("01_Init", p))

# ─────────────────────────────────────────────
# 2. Warm Pad - Sine ops with slow attack, chorus
# ─────────────────────────────────────────────
p = make_base()
for i in range(1,7):
    p[f"ATTACK_{i}"] = "1.2"
    p[f"DECAY_{i}"] = "0.5"
    p[f"SUSTAIN_{i}"] = "0.9"
    p[f"RELEASE_{i}"] = "2.0"
    p[f"WAVE_SHAPE_{i}"] = "0.0"  # sine
p["OUT_1"] = "0.6"; p["OUT_2"] = "0.4"; p["OUT_3"] = "0.3"
p["RATIO_2"] = "2.0"; p["RATIO_3"] = "3.0"
p["DETUNE_2"] = "7.0"; p["DETUNE_3"] = "-5.0"
p["FX_TYPE_1"] = "21.0"  # Old Chorus
p["FX_RATIO_1"] = "3.0"; p["FX_DETUNE_1"] = "25.0"; p["FX_PHASE_1"] = "180.0"; p["FX_FOLD_1"] = "0.6"
p["FX_TYPE_2"] = "10.0"  # AP Reverb
p["FX_RATIO_2"] = "8.0"; p["FX_DETUNE_2"] = "30.0"; p["FX_FOLD_2"] = "0.4"
presets.append(("02_Warm_Pad", p))

# ─────────────────────────────────────────────
# 3. Classic Bell - FM bell with ratio 1:2:3
# ─────────────────────────────────────────────
p = make_base()
p["ATTACK_1"] = "0.001"; p["DECAY_1"] = "2.0"; p["SUSTAIN_1"] = "0.0"; p["RELEASE_1"] = "1.5"
p["ATTACK_2"] = "0.001"; p["DECAY_2"] = "1.5"; p["SUSTAIN_2"] = "0.0"; p["RELEASE_2"] = "1.0"
p["RATIO_1"] = "1.0"; p["RATIO_2"] = "2.0"; p["RATIO_3"] = "3.0"
p["OUT_1"] = "1.0"; p["OUT_2"] = "0.0"; p["OUT_3"] = "0.0"
p["MOD_1_0"] = "5.0"; p["MOD_2_0"] = "2.0"  # Op2->Op1, Op3->Op1
p["MOD_2_1"] = "3.0"  # Op3->Op2
presets.append(("03_Classic_Bell", p))

# ─────────────────────────────────────────────
# 4. Brass Lead - Saw wave, bright filter
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "2.0"  # Saw
p["ATTACK_1"] = "0.02"; p["DECAY_1"] = "0.1"; p["SUSTAIN_1"] = "0.8"; p["RELEASE_1"] = "0.2"
p["DETUNE_1"] = "5.0"
p["WAVE_SHAPE_2"] = "2.0"; p["RATIO_2"] = "1.0"; p["DETUNE_2"] = "-5.0"; p["OUT_2"] = "0.6"
p["ATTACK_2"] = "0.02"; p["DECAY_2"] = "0.1"; p["SUSTAIN_2"] = "0.8"; p["RELEASE_2"] = "0.2"
p["FX_TYPE_1"] = "1.0"  # Lowpass
p["FX_RATIO_1"] = "10.0"; p["FX_DETUNE_1"] = "40.0"; p["FX_PHASE_1"] = "180.0"
presets.append(("04_Brass_Lead", p))

# ─────────────────────────────────────────────
# 5. Sub Bass - Sine, low ratio, deep
# ─────────────────────────────────────────────
p = make_base()
p["RATIO_1"] = "0.5"
p["WAVE_SHAPE_1"] = "0.0"  # Sine
p["ATTACK_1"] = "0.01"; p["DECAY_1"] = "0.3"; p["SUSTAIN_1"] = "1.0"; p["RELEASE_1"] = "0.2"
p["FOLD_1"] = "0.15"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "11.0"  # Compressor
p["FX_RATIO_1"] = "5.0"; p["FX_DETUNE_1"] = "-20.0"; p["FX_PHASE_1"] = "100.0"; p["FX_FOLD_1"] = "0.8"
presets.append(("05_Sub_Bass", p))

# ─────────────────────────────────────────────
# 6. Electric Piano - Classic DX7-style
# ─────────────────────────────────────────────
p = make_base()
p["RATIO_1"] = "1.0"; p["RATIO_2"] = "14.0"
p["ATTACK_1"] = "0.001"; p["DECAY_1"] = "1.5"; p["SUSTAIN_1"] = "0.0"; p["RELEASE_1"] = "0.8"
p["ATTACK_2"] = "0.001"; p["DECAY_2"] = "0.8"; p["SUSTAIN_2"] = "0.0"; p["RELEASE_2"] = "0.5"
p["MOD_1_0"] = "3.5"  # Op2 modulates Op1
p["OUT_1"] = "1.0"
presets.append(("06_Electric_Piano", p))

# ─────────────────────────────────────────────
# 7. Additive Strings - Additive mode, tilt dark
# ─────────────────────────────────────────────
p = make_base()
p["MODE_1"] = "1.0"  # Additive
p["DETUNE_1"] = "-20.0"  # Tilt dark
p["PHASE_1"] = "180.0"   # Stretch
p["FOLD_1"] = "0.4"      # Odd/Even
p["ATTACK_1"] = "0.8"; p["DECAY_1"] = "0.3"; p["SUSTAIN_1"] = "0.9"; p["RELEASE_1"] = "1.5"
p["OUT_1"] = "1.0"
p["MODE_2"] = "1.0"; p["DETUNE_2"] = "-15.0"; p["PHASE_2"] = "190.0"; p["FOLD_2"] = "0.45"
p["RATIO_2"] = "2.0"; p["DETUNE_2"] = "3.0"; p["OUT_2"] = "0.5"
p["ATTACK_2"] = "1.0"; p["DECAY_2"] = "0.3"; p["SUSTAIN_2"] = "0.85"; p["RELEASE_2"] = "1.8"
p["FX_TYPE_1"] = "10.0"  # AP Reverb
p["FX_RATIO_1"] = "7.0"; p["FX_DETUNE_1"] = "35.0"; p["FX_FOLD_1"] = "0.5"
presets.append(("07_Additive_Strings", p))

# ─────────────────────────────────────────────
# 8. Growl Bass - Saw + FM, filter modulation
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "2.0"  # Saw
p["RATIO_1"] = "1.0"; p["RATIO_2"] = "1.0"
p["ATTACK_1"] = "0.005"; p["DECAY_1"] = "0.2"; p["SUSTAIN_1"] = "0.7"; p["RELEASE_1"] = "0.15"
p["MOD_1_0"] = "4.0"
p["FOLD_1"] = "0.3"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "4.0"  # Comb
p["FX_RATIO_1"] = "6.0"; p["FX_DETUNE_1"] = "20.0"; p["FX_PHASE_1"] = "90.0"; p["FX_FOLD_1"] = "0.6"
presets.append(("08_Growl_Bass", p))

# ─────────────────────────────────────────────
# 9. Dreamy Pad - Multiple detuned sines, reverb
# ─────────────────────────────────────────────
p = make_base()
for i in range(1,7):
    p[f"WAVE_SHAPE_{i}"] = "0.0"
    p[f"ATTACK_{i}"] = "2.0"
    p[f"DECAY_{i}"] = "1.0"
    p[f"SUSTAIN_{i}"] = "0.8"
    p[f"RELEASE_{i}"] = "3.0"
    p[f"OUT_{i}"] = "0.4"
p["RATIO_1"] = "1.0"; p["RATIO_2"] = "2.0"; p["RATIO_3"] = "3.0"
p["RATIO_4"] = "4.0"; p["RATIO_5"] = "5.0"; p["RATIO_6"] = "6.0"
p["DETUNE_2"] = "5.0"; p["DETUNE_3"] = "-3.0"; p["DETUNE_4"] = "8.0"
p["DETUNE_5"] = "-6.0"; p["DETUNE_6"] = "2.0"
p["FX_TYPE_1"] = "20.0"  # Ambient
p["FX_RATIO_1"] = "12.0"; p["FX_DETUNE_1"] = "40.0"; p["FX_PHASE_1"] = "200.0"; p["FX_FOLD_1"] = "0.7"
presets.append(("09_Dreamy_Pad", p))

# ─────────────────────────────────────────────
# 10. Pluck - Short decay FM
# ─────────────────────────────────────────────
p = make_base()
p["RATIO_1"] = "1.0"; p["RATIO_2"] = "3.5"
p["ATTACK_1"] = "0.001"; p["DECAY_1"] = "0.4"; p["SUSTAIN_1"] = "0.0"; p["RELEASE_1"] = "0.3"
p["ATTACK_2"] = "0.001"; p["DECAY_2"] = "0.2"; p["SUSTAIN_2"] = "0.0"; p["RELEASE_2"] = "0.1"
p["MOD_1_0"] = "6.0"
p["OUT_1"] = "1.0"
presets.append(("10_Pluck", p))

# ─────────────────────────────────────────────
# 11. Lo-Fi Keys - Bitcrush + tape
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "0.0"
p["RATIO_2"] = "7.0"
p["ATTACK_1"] = "0.001"; p["DECAY_1"] = "1.2"; p["SUSTAIN_1"] = "0.2"; p["RELEASE_1"] = "0.6"
p["MOD_1_0"] = "2.5"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "8.0"  # Bitcrush
p["FX_RATIO_1"] = "3.0"; p["FX_DETUNE_1"] = "25.0"; p["FX_PHASE_1"] = "60.0"; p["FX_FOLD_1"] = "0.2"
p["FX_TYPE_2"] = "7.0"  # Tape
p["FX_RATIO_2"] = "2.0"; p["FX_DETUNE_2"] = "35.0"; p["FX_PHASE_2"] = "100.0"; p["FX_FOLD_2"] = "0.5"
presets.append(("11_LoFi_Keys", p))

# ─────────────────────────────────────────────
# 12. Organ - Additive, Hammond-style
# ─────────────────────────────────────────────
p = make_base()
for i in range(1,7):
    p[f"MODE_{i}"] = "1.0"  # Additive
    p[f"ATTACK_{i}"] = "0.005"
    p[f"DECAY_{i}"] = "0.1"
    p[f"SUSTAIN_{i}"] = "1.0"
    p[f"RELEASE_{i}"] = "0.02"
p["RATIO_1"] = "1.0"; p["RATIO_2"] = "2.0"; p["RATIO_3"] = "3.0"
p["RATIO_4"] = "4.0"; p["RATIO_5"] = "5.0"; p["RATIO_6"] = "8.0"
p["OUT_1"] = "0.8"; p["OUT_2"] = "0.7"; p["OUT_3"] = "0.5"
p["OUT_4"] = "0.4"; p["OUT_5"] = "0.3"; p["OUT_6"] = "0.2"
p["DETUNE_1"] = "8.0"  # Tilt in additive = brightness
p["FX_TYPE_1"] = "21.0"  # Old Chorus
p["FX_RATIO_1"] = "2.0"; p["FX_FOLD_1"] = "0.7"
presets.append(("12_Organ", p))

# ─────────────────────────────────────────────
# 13. Metallic Bells - Inharmonic FM
# ─────────────────────────────────────────────
p = make_base()
p["RATIO_1"] = "1.0"; p["RATIO_2"] = "2.756"; p["RATIO_3"] = "5.404"
p["ATTACK_1"] = "0.001"; p["DECAY_1"] = "3.0"; p["SUSTAIN_1"] = "0.0"; p["RELEASE_1"] = "2.0"
p["ATTACK_2"] = "0.001"; p["DECAY_2"] = "2.0"; p["SUSTAIN_2"] = "0.0"; p["RELEASE_2"] = "1.5"
p["ATTACK_3"] = "0.001"; p["DECAY_3"] = "1.5"; p["SUSTAIN_3"] = "0.0"; p["RELEASE_3"] = "1.0"
p["MOD_1_0"] = "4.0"; p["MOD_2_1"] = "3.0"; p["MOD_2_0"] = "2.0"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "10.0"  # AP Reverb
p["FX_RATIO_1"] = "9.0"; p["FX_DETUNE_1"] = "38.0"; p["FX_FOLD_1"] = "0.6"
presets.append(("13_Metallic_Bells", p))

# ─────────────────────────────────────────────
# 14. Granular Texture - Granular filter on noise
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "6.0"  # White noise
p["MODE_1"] = "0.0"
p["ATTACK_1"] = "0.5"; p["DECAY_1"] = "1.0"; p["SUSTAIN_1"] = "0.8"; p["RELEASE_1"] = "2.0"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "5.0"  # Granular
p["FX_RATIO_1"] = "6.0"; p["FX_DETUNE_1"] = "20.0"; p["FX_PHASE_1"] = "180.0"; p["FX_FOLD_1"] = "0.5"
p["FX_TYPE_2"] = "10.0"  # AP Reverb
p["FX_RATIO_2"] = "10.0"; p["FX_DETUNE_2"] = "40.0"; p["FX_FOLD_2"] = "0.7"
presets.append(("14_Granular_Texture", p))

# ─────────────────────────────────────────────
# 15. Punchy Lead - Square, short envelope
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "3.0"  # Square
p["RATIO_2"] = "2.0"; p["MOD_1_0"] = "2.0"
p["ATTACK_1"] = "0.003"; p["DECAY_1"] = "0.15"; p["SUSTAIN_1"] = "0.6"; p["RELEASE_1"] = "0.1"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "1.0"  # Lowpass
p["FX_RATIO_1"] = "8.0"; p["FX_DETUNE_1"] = "35.0"; p["FX_PHASE_1"] = "120.0"
presets.append(("15_Punchy_Lead", p))

# ─────────────────────────────────────────────
# 16. Ambient Shimmer - Ambient delay, high shimmer
# ─────────────────────────────────────────────
p = make_base()
p["MODE_1"] = "1.0"  # Additive
p["DETUNE_1"] = "10.0"; p["PHASE_1"] = "200.0"; p["FOLD_1"] = "0.5"
p["ATTACK_1"] = "1.5"; p["DECAY_1"] = "1.0"; p["SUSTAIN_1"] = "0.9"; p["RELEASE_1"] = "4.0"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "20.0"  # Ambient
p["FX_RATIO_1"] = "14.0"; p["FX_DETUNE_1"] = "45.0"; p["FX_PHASE_1"] = "270.0"; p["FX_FOLD_1"] = "0.8"
p["FX_TYPE_2"] = "19.0"  # Harmonic Resonator
p["FX_RATIO_2"] = "4.0"; p["FX_DETUNE_2"] = "30.0"; p["FX_PHASE_2"] = "120.0"; p["FX_FOLD_2"] = "0.7"
presets.append(("16_Ambient_Shimmer", p))

# ─────────────────────────────────────────────
# 17. Wobble Bass - FM bass with scatter
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "2.0"  # Saw
p["RATIO_2"] = "0.5"
p["MOD_1_0"] = "5.0"
p["ATTACK_1"] = "0.005"; p["DECAY_1"] = "0.3"; p["SUSTAIN_1"] = "0.8"; p["RELEASE_1"] = "0.2"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "13.0"  # Scatter
p["FX_RATIO_1"] = "4.0"; p["FX_DETUNE_1"] = "25.0"; p["FX_PHASE_1"] = "90.0"; p["FX_FOLD_1"] = "0.6"
p["FX_TYPE_2"] = "4.0"  # Comb
p["FX_RATIO_2"] = "5.0"; p["FX_DETUNE_2"] = "30.0"; p["FX_FOLD_2"] = "0.5"
presets.append(("17_Wobble_Bass", p))

# ─────────────────────────────────────────────
# 18. Glitch Pad - Scatter + bitcrush
# ─────────────────────────────────────────────
p = make_base()
p["MODE_1"] = "1.0"  # Additive
p["DETUNE_1"] = "5.0"; p["FOLD_1"] = "0.6"
p["ATTACK_1"] = "0.3"; p["DECAY_1"] = "0.5"; p["SUSTAIN_1"] = "0.7"; p["RELEASE_1"] = "1.0"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "13.0"  # Scatter
p["FX_RATIO_1"] = "7.0"; p["FX_DETUNE_1"] = "30.0"; p["FX_PHASE_1"] = "45.0"; p["FX_FOLD_1"] = "0.7"
p["FX_TYPE_2"] = "8.0"  # Bitcrush
p["FX_RATIO_2"] = "2.0"; p["FX_DETUNE_2"] = "40.0"; p["FX_PHASE_2"] = "30.0"; p["FX_FOLD_2"] = "0.3"
presets.append(("18_Glitch_Pad", p))

# ─────────────────────────────────────────────
# 19. Choir - Formant filter, additive source
# ─────────────────────────────────────────────
p = make_base()
p["MODE_1"] = "1.0"  # Additive
p["DETUNE_1"] = "-10.0"; p["PHASE_1"] = "180.0"; p["FOLD_1"] = "0.5"
p["ATTACK_1"] = "0.5"; p["DECAY_1"] = "0.5"; p["SUSTAIN_1"] = "0.9"; p["RELEASE_1"] = "1.5"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "6.0"  # Formant
p["FX_RATIO_1"] = "5.0"; p["FX_DETUNE_1"] = "30.0"; p["FX_PHASE_1"] = "120.0"; p["FX_FOLD_1"] = "0.4"
p["FX_TYPE_2"] = "10.0"  # AP Reverb
p["FX_RATIO_2"] = "9.0"; p["FX_DETUNE_2"] = "38.0"; p["FX_FOLD_2"] = "0.5"
presets.append(("19_Choir", p))

# ─────────────────────────────────────────────
# 20. Ring Mod Stab - Short attack, ring mod FX
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "0.0"
p["RATIO_2"] = "5.0"; p["MOD_1_0"] = "3.0"
p["ATTACK_1"] = "0.002"; p["DECAY_1"] = "0.3"; p["SUSTAIN_1"] = "0.4"; p["RELEASE_1"] = "0.2"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "14.0"  # Ring Mod
p["FX_RATIO_1"] = "8.0"; p["FX_DETUNE_1"] = "25.0"; p["FX_PHASE_1"] = "180.0"; p["FX_FOLD_1"] = "0.7"
presets.append(("20_RingMod_Stab", p))

# ─────────────────────────────────────────────
# 21. Tape Warmth - Tape FX on simple saw
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "2.0"  # Saw
p["WAVE_SHAPE_2"] = "2.0"; p["RATIO_2"] = "1.0"; p["DETUNE_2"] = "-8.0"; p["OUT_2"] = "0.6"
p["ATTACK_1"] = "0.01"; p["DECAY_1"] = "0.2"; p["SUSTAIN_1"] = "0.85"; p["RELEASE_1"] = "0.4"
p["ATTACK_2"] = "0.01"; p["DECAY_2"] = "0.2"; p["SUSTAIN_2"] = "0.85"; p["RELEASE_2"] = "0.4"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "7.0"  # Tape
p["FX_RATIO_1"] = "4.0"; p["FX_DETUNE_1"] = "40.0"; p["FX_PHASE_1"] = "150.0"; p["FX_FOLD_1"] = "0.5"
p["FX_TYPE_2"] = "21.0"  # Old Chorus
p["FX_RATIO_2"] = "2.0"; p["FX_FOLD_2"] = "0.6"
presets.append(("21_Tape_Warmth", p))

# ─────────────────────────────────────────────
# 22. Hard Sync Lead - Triangle, self-modulation
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "1.0"  # Triangle
p["RATIO_2"] = "1.0"
p["MOD_0_0"] = "3.0"  # Self FM
p["ATTACK_1"] = "0.005"; p["DECAY_1"] = "0.1"; p["SUSTAIN_1"] = "0.7"; p["RELEASE_1"] = "0.1"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "17.0"  # Distortion
p["FX_RATIO_1"] = "6.0"; p["FX_DETUNE_1"] = "30.0"; p["FX_PHASE_1"] = "180.0"; p["FX_FOLD_1"] = "0.3"
presets.append(("22_Hard_Sync_Lead", p))

# ─────────────────────────────────────────────
# 23. Ethnic Pluck - Inharmonic, fast decay
# ─────────────────────────────────────────────
p = make_base()
p["RATIO_1"] = "1.0"; p["RATIO_2"] = "3.0"; p["RATIO_3"] = "7.0"
p["ATTACK_1"] = "0.001"; p["DECAY_1"] = "0.8"; p["SUSTAIN_1"] = "0.0"; p["RELEASE_1"] = "0.5"
p["MOD_1_0"] = "5.5"; p["MOD_2_0"] = "2.0"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "9.0"  # AP Delay
p["FX_RATIO_1"] = "3.0"; p["FX_DETUNE_1"] = "20.0"; p["FX_PHASE_1"] = "60.0"; p["FX_FOLD_1"] = "0.3"
presets.append(("23_Ethnic_Pluck", p))

# ─────────────────────────────────────────────
# 24. PWM Pad - Pulse wave with slow LFO feel
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "4.0"  # Pulse
p["PHASE_1"] = "120.0"  # PWM duty cycle
p["WAVE_SHAPE_2"] = "4.0"; p["PHASE_2"] = "200.0"; p["RATIO_2"] = "1.0"; p["DETUNE_2"] = "6.0"; p["OUT_2"] = "0.7"
p["ATTACK_1"] = "0.3"; p["DECAY_1"] = "0.4"; p["SUSTAIN_1"] = "0.85"; p["RELEASE_1"] = "1.2"
p["ATTACK_2"] = "0.3"; p["DECAY_2"] = "0.4"; p["SUSTAIN_2"] = "0.85"; p["RELEASE_2"] = "1.2"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "21.0"  # Old Chorus
p["FX_RATIO_1"] = "3.0"; p["FX_DETUNE_1"] = "28.0"; p["FX_PHASE_1"] = "270.0"; p["FX_FOLD_1"] = "0.65"
presets.append(("24_PWM_Pad", p))

# ─────────────────────────────────────────────
# 25. Vocal Synth - Formant morphing
# ─────────────────────────────────────────────
p = make_base()
p["MODE_1"] = "2.0"; p["FILTER_TYPE_1"] = "6.0"  # Formant filter
p["RATIO_1"] = "8.0"  # High cutoff = bright vowel
p["DETUNE_1"] = "20.0"  # High resonance
p["PHASE_1"] = "180.0"  # Vowel mod
p["FOLD_1"] = "0.3"  # Drive
p["ATTACK_1"] = "0.05"; p["DECAY_1"] = "0.2"; p["SUSTAIN_1"] = "0.8"; p["RELEASE_1"] = "0.3"
# Feed a saw into it via audio matrix
p["WAVE_SHAPE_2"] = "2.0"; p["OUT_2"] = "0.0"; p["RATIO_2"] = "1.0"
p["ATTACK_2"] = "0.005"; p["DECAY_2"] = "0.1"; p["SUSTAIN_2"] = "1.0"; p["RELEASE_2"] = "0.1"
p["AUDIO_ROUTE_1_0"] = "0.8"
p["OUT_1"] = "1.0"
presets.append(("25_Vocal_Synth", p))

# ─────────────────────────────────────────────
# 26. Deep Space - Long ambient, harmonic resonator
# ─────────────────────────────────────────────
p = make_base()
p["MODE_1"] = "1.0"  # Additive
p["DETUNE_1"] = "0.0"; p["PHASE_1"] = "270.0"; p["FOLD_1"] = "0.5"
p["ATTACK_1"] = "3.0"; p["DECAY_1"] = "2.0"; p["SUSTAIN_1"] = "0.7"; p["RELEASE_1"] = "5.0"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "19.0"  # Harmonic Resonator
p["FX_RATIO_1"] = "6.0"; p["FX_DETUNE_1"] = "35.0"; p["FX_PHASE_1"] = "240.0"; p["FX_FOLD_1"] = "0.8"
p["FX_TYPE_2"] = "20.0"  # Ambient
p["FX_RATIO_2"] = "15.0"; p["FX_DETUNE_2"] = "45.0"; p["FX_PHASE_2"] = "300.0"; p["FX_FOLD_2"] = "0.9"
presets.append(("26_Deep_Space", p))

# ─────────────────────────────────────────────
# 27. Comb Lead - Comb filter, plucky
# ─────────────────────────────────────────────
p = make_base()
p["MODE_1"] = "2.0"; p["FILTER_TYPE_1"] = "4.0"  # Comb
p["RATIO_1"] = "5.0"; p["DETUNE_1"] = "10.0"; p["PHASE_1"] = "180.0"; p["FOLD_1"] = "0.7"
p["ATTACK_1"] = "0.001"; p["DECAY_1"] = "0.5"; p["SUSTAIN_1"] = "0.3"; p["RELEASE_1"] = "0.3"
p["WAVE_SHAPE_2"] = "2.0"; p["OUT_2"] = "0.0"
p["ATTACK_2"] = "0.001"; p["DECAY_2"] = "0.1"; p["SUSTAIN_2"] = "1.0"; p["RELEASE_2"] = "0.1"
p["AUDIO_ROUTE_1_0"] = "1.0"
p["OUT_1"] = "1.0"
presets.append(("27_Comb_Lead", p))

# ─────────────────────────────────────────────
# 28. Jungle Bass - Distortion + scatter
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "2.0"  # Saw
p["RATIO_2"] = "0.5"; p["MOD_1_0"] = "6.0"
p["ATTACK_1"] = "0.005"; p["DECAY_1"] = "0.15"; p["SUSTAIN_1"] = "0.7"; p["RELEASE_1"] = "0.1"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "17.0"  # Distortion
p["FX_RATIO_1"] = "9.0"; p["FX_DETUNE_1"] = "30.0"; p["FX_PHASE_1"] = "200.0"; p["FX_FOLD_1"] = "0.4"
p["FX_TYPE_2"] = "13.0"  # Scatter
p["FX_RATIO_2"] = "3.0"; p["FX_DETUNE_2"] = "25.0"; p["FX_PHASE_2"] = "30.0"; p["FX_FOLD_2"] = "0.5"
presets.append(("28_Jungle_Bass", p))

# ─────────────────────────────────────────────
# 29. Glass Marimba - Bell-like, HP filter
# ─────────────────────────────────────────────
p = make_base()
p["RATIO_1"] = "1.0"; p["RATIO_2"] = "4.0"; p["RATIO_3"] = "9.0"
p["ATTACK_1"] = "0.001"; p["DECAY_1"] = "1.0"; p["SUSTAIN_1"] = "0.0"; p["RELEASE_1"] = "0.8"
p["ATTACK_2"] = "0.001"; p["DECAY_2"] = "0.6"; p["SUSTAIN_2"] = "0.0"; p["RELEASE_2"] = "0.5"
p["MOD_1_0"] = "3.0"; p["MOD_2_1"] = "4.0"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "2.0"  # HP
p["FX_RATIO_1"] = "7.0"; p["FX_DETUNE_1"] = "20.0"; p["FX_PHASE_1"] = "90.0"
p["FX_TYPE_2"] = "10.0"  # AP Reverb
p["FX_RATIO_2"] = "8.0"; p["FX_FOLD_2"] = "0.4"
presets.append(("29_Glass_Marimba", p))

# ─────────────────────────────────────────────
# 30. Techno Kick - Short, punchy, filtered noise
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "6.0"  # White noise
p["ATTACK_1"] = "0.001"; p["DECAY_1"] = "0.08"; p["SUSTAIN_1"] = "0.0"; p["RELEASE_1"] = "0.05"
p["OUT_1"] = "1.0"
p["WAVE_SHAPE_2"] = "0.0"; p["RATIO_2"] = "0.5"; p["OUT_2"] = "0.8"
p["ATTACK_2"] = "0.001"; p["DECAY_2"] = "0.15"; p["SUSTAIN_2"] = "0.0"; p["RELEASE_2"] = "0.1"
p["FX_TYPE_1"] = "1.0"  # LP
p["FX_RATIO_1"] = "5.0"; p["FX_DETUNE_1"] = "10.0"; p["FX_PHASE_1"] = "60.0"
p["FX_TYPE_2"] = "11.0"  # Compressor
p["FX_RATIO_2"] = "8.0"; p["FX_DETUNE_2"] = "-10.0"; p["FX_PHASE_2"] = "80.0"; p["FX_FOLD_2"] = "0.9"
presets.append(("30_Techno_Kick", p))

# ─────────────────────────────────────────────
# 31. Reverse Pad - Long attack, ambient tail
# ─────────────────────────────────────────────
p = make_base()
p["MODE_1"] = "1.0"  # Additive
p["DETUNE_1"] = "-5.0"; p["PHASE_1"] = "160.0"; p["FOLD_1"] = "0.45"
p["ATTACK_1"] = "4.0"; p["DECAY_1"] = "1.5"; p["SUSTAIN_1"] = "1.0"; p["RELEASE_1"] = "0.3"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "12.0"  # Varispeed
p["FX_RATIO_1"] = "3.0"; p["FX_DETUNE_1"] = "15.0"; p["FX_PHASE_1"] = "90.0"; p["FX_FOLD_1"] = "0.3"
p["FX_TYPE_2"] = "10.0"  # AP Reverb
p["FX_RATIO_2"] = "12.0"; p["FX_DETUNE_2"] = "40.0"; p["FX_FOLD_2"] = "0.6"
presets.append(("31_Reverse_Pad", p))

# ─────────────────────────────────────────────
# 32. Chiptune Lead - Square, short, no release
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "3.0"  # Square
p["ATTACK_1"] = "0.001"; p["DECAY_1"] = "0.05"; p["SUSTAIN_1"] = "1.0"; p["RELEASE_1"] = "0.02"
p["OUT_1"] = "1.0"
p["RATIO_2"] = "2.0"; p["WAVE_SHAPE_2"] = "3.0"; p["OUT_2"] = "0.3"
p["ATTACK_2"] = "0.001"; p["DECAY_2"] = "0.05"; p["SUSTAIN_2"] = "1.0"; p["RELEASE_2"] = "0.02"
p["FX_TYPE_1"] = "8.0"  # Bitcrush
p["FX_RATIO_1"] = "2.0"; p["FX_DETUNE_1"] = "45.0"; p["FX_PHASE_1"] = "10.0"; p["FX_FOLD_1"] = "0.1"
presets.append(("32_Chiptune_Lead", p))

# ─────────────────────────────────────────────
# 33. Evolving Texture - Varispeed + scatter
# ─────────────────────────────────────────────
p = make_base()
p["MODE_1"] = "1.0"  # Additive
p["DETUNE_1"] = "15.0"; p["PHASE_1"] = "220.0"; p["FOLD_1"] = "0.55"
p["ATTACK_1"] = "1.0"; p["DECAY_1"] = "2.0"; p["SUSTAIN_1"] = "0.7"; p["RELEASE_1"] = "3.0"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "12.0"  # Varispeed
p["FX_RATIO_1"] = "5.0"; p["FX_DETUNE_1"] = "25.0"; p["FX_PHASE_1"] = "180.0"; p["FX_FOLD_1"] = "0.7"
p["FX_TYPE_2"] = "13.0"  # Scatter
p["FX_RATIO_2"] = "6.0"; p["FX_DETUNE_2"] = "20.0"; p["FX_PHASE_2"] = "120.0"; p["FX_FOLD_2"] = "0.4"
p["FX_TYPE_3"] = "20.0"  # Ambient
p["FX_RATIO_3"] = "11.0"; p["FX_DETUNE_3"] = "42.0"; p["FX_FOLD_3"] = "0.6"
presets.append(("33_Evolving_Texture", p))

# ─────────────────────────────────────────────
# 34. Acid Bass - Self-mod saw, comb filter
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "2.0"  # Saw
p["MOD_0_0"] = "4.0"  # Self FM
p["ATTACK_1"] = "0.005"; p["DECAY_1"] = "0.2"; p["SUSTAIN_1"] = "0.6"; p["RELEASE_1"] = "0.1"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "4.0"  # Comb
p["FX_RATIO_1"] = "7.0"; p["FX_DETUNE_1"] = "30.0"; p["FX_PHASE_1"] = "120.0"; p["FX_FOLD_1"] = "0.75"
presets.append(("34_Acid_Bass", p))

# ─────────────────────────────────────────────
# 35. Majestic Strings - 6 ops detuned
# ─────────────────────────────────────────────
p = make_base()
for i in range(1,7):
    p[f"WAVE_SHAPE_{i}"] = "2.0"  # Saw
    p[f"ATTACK_{i}"] = "0.4"
    p[f"DECAY_{i}"] = "0.3"
    p[f"SUSTAIN_{i}"] = "0.9"
    p[f"RELEASE_{i}"] = "1.2"
    p[f"OUT_{i}"] = "0.5"
p["RATIO_1"] = "1.0"; p["RATIO_2"] = "1.0"; p["RATIO_3"] = "2.0"
p["RATIO_4"] = "2.0"; p["RATIO_5"] = "3.0"; p["RATIO_6"] = "3.0"
p["DETUNE_1"] = "8.0"; p["DETUNE_2"] = "-8.0"; p["DETUNE_3"] = "5.0"
p["DETUNE_4"] = "-5.0"; p["DETUNE_5"] = "3.0"; p["DETUNE_6"] = "-3.0"
p["FX_TYPE_1"] = "21.0"  # Old Chorus
p["FX_RATIO_1"] = "2.0"; p["FX_FOLD_1"] = "0.55"
p["FX_TYPE_2"] = "10.0"  # AP Reverb
p["FX_RATIO_2"] = "10.0"; p["FX_DETUNE_2"] = "40.0"; p["FX_FOLD_2"] = "0.5"
presets.append(("35_Majestic_Strings", p))

# ─────────────────────────────────────────────
# 36. Tape Echo - Tape + AP delay
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "0.0"  # Sine
p["RATIO_2"] = "3.0"; p["MOD_1_0"] = "4.0"
p["ATTACK_1"] = "0.01"; p["DECAY_1"] = "0.5"; p["SUSTAIN_1"] = "0.6"; p["RELEASE_1"] = "0.8"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "7.0"  # Tape
p["FX_RATIO_1"] = "3.0"; p["FX_DETUNE_1"] = "35.0"; p["FX_PHASE_1"] = "120.0"; p["FX_FOLD_1"] = "0.45"
p["FX_TYPE_2"] = "9.0"  # AP Delay
p["FX_RATIO_2"] = "6.0"; p["FX_DETUNE_2"] = "25.0"; p["FX_PHASE_2"] = "90.0"; p["FX_FOLD_2"] = "0.4"
presets.append(("36_Tape_Echo", p))

# ─────────────────────────────────────────────
# 37. Crystal Arp - High ratio, short, bright
# ─────────────────────────────────────────────
p = make_base()
p["RATIO_1"] = "1.0"; p["RATIO_2"] = "8.0"; p["RATIO_3"] = "16.0"
p["ATTACK_1"] = "0.001"; p["DECAY_1"] = "0.3"; p["SUSTAIN_1"] = "0.0"; p["RELEASE_1"] = "0.2"
p["MOD_1_0"] = "2.0"; p["MOD_2_1"] = "3.0"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "10.0"  # AP Reverb
p["FX_RATIO_1"] = "7.0"; p["FX_DETUNE_1"] = "32.0"; p["FX_FOLD_1"] = "0.35"
presets.append(("37_Crystal_Arp", p))

# ─────────────────────────────────────────────
# 38. Sitar - Inharmonic FM + comb
# ─────────────────────────────────────────────
p = make_base()
p["RATIO_1"] = "1.0"; p["RATIO_2"] = "0.999"; p["RATIO_3"] = "2.756"
p["ATTACK_1"] = "0.001"; p["DECAY_1"] = "1.5"; p["SUSTAIN_1"] = "0.1"; p["RELEASE_1"] = "0.8"
p["MOD_1_0"] = "7.0"; p["MOD_2_0"] = "3.0"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "4.0"  # Comb
p["FX_RATIO_1"] = "4.0"; p["FX_DETUNE_1"] = "15.0"; p["FX_PHASE_1"] = "60.0"; p["FX_FOLD_1"] = "0.4"
presets.append(("38_Sitar", p))

# ─────────────────────────────────────────────
# 39. Noise Sweep - Filtered noise, LP sweep feel
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "7.0"  # Pink noise
p["ATTACK_1"] = "0.1"; p["DECAY_1"] = "2.0"; p["SUSTAIN_1"] = "0.5"; p["RELEASE_1"] = "1.5"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "1.0"  # LP
p["FX_RATIO_1"] = "6.0"; p["FX_DETUNE_1"] = "35.0"; p["FX_PHASE_1"] = "180.0"
p["FX_TYPE_2"] = "10.0"  # AP Reverb
p["FX_RATIO_2"] = "11.0"; p["FX_FOLD_2"] = "0.6"
presets.append(("39_Noise_Sweep", p))

# ─────────────────────────────────────────────
# 40. Thick Supersaw - 6 detuned saws
# ─────────────────────────────────────────────
p = make_base()
for i in range(1,7):
    p[f"WAVE_SHAPE_{i}"] = "2.0"
    p[f"RATIO_{i}"] = "1.0"
    p[f"ATTACK_{i}"] = "0.05"
    p[f"DECAY_{i}"] = "0.1"
    p[f"SUSTAIN_{i}"] = "1.0"
    p[f"RELEASE_{i}"] = "0.3"
    p[f"OUT_{i}"] = "0.45"
detunes = [14, -14, 9, -9, 4, -4]
for i, d in enumerate(detunes, 1):
    p[f"DETUNE_{i}"] = str(d)
p["FX_TYPE_1"] = "1.0"  # LP
p["FX_RATIO_1"] = "11.0"; p["FX_DETUNE_1"] = "38.0"; p["FX_PHASE_1"] = "180.0"
p["FX_TYPE_2"] = "21.0"  # Old Chorus
p["FX_RATIO_2"] = "3.0"; p["FX_FOLD_2"] = "0.6"
presets.append(("40_Thick_Supersaw", p))

# ─────────────────────────────────────────────
# 41. Haunted Keys - Varispeed + formant
# ─────────────────────────────────────────────
p = make_base()
p["RATIO_1"] = "1.0"; p["RATIO_2"] = "5.0"
p["ATTACK_1"] = "0.005"; p["DECAY_1"] = "1.5"; p["SUSTAIN_1"] = "0.2"; p["RELEASE_1"] = "1.0"
p["MOD_1_0"] = "3.0"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "6.0"  # Formant
p["FX_RATIO_1"] = "4.0"; p["FX_DETUNE_1"] = "25.0"; p["FX_PHASE_1"] = "90.0"; p["FX_FOLD_1"] = "0.35"
p["FX_TYPE_2"] = "12.0"  # Varispeed
p["FX_RATIO_2"] = "4.0"; p["FX_DETUNE_2"] = "20.0"; p["FX_PHASE_2"] = "30.0"; p["FX_FOLD_2"] = "0.2"
presets.append(("41_Haunted_Keys", p))

# ─────────────────────────────────────────────
# 42. Soft Flute - Sine, additive, breathy
# ─────────────────────────────────────────────
p = make_base()
p["MODE_1"] = "1.0"  # Additive
p["DETUNE_1"] = "-30.0"  # Dark tilt
p["PHASE_1"] = "180.0"
p["FOLD_1"] = "0.0"  # Odd harmonics only
p["ATTACK_1"] = "0.1"; p["DECAY_1"] = "0.2"; p["SUSTAIN_1"] = "0.9"; p["RELEASE_1"] = "0.4"
p["OUT_1"] = "1.0"
p["WAVE_SHAPE_2"] = "7.0"  # Pink noise for breath
p["OUT_2"] = "0.15"; p["ATTACK_2"] = "0.05"; p["DECAY_2"] = "0.1"; p["SUSTAIN_2"] = "0.6"; p["RELEASE_2"] = "0.2"
presets.append(("42_Soft_Flute", p))

# ─────────────────────────────────────────────
# 43. Space Chord - Harmonic resonator, long
# ─────────────────────────────────────────────
p = make_base()
p["MODE_1"] = "1.0"
p["DETUNE_1"] = "5.0"; p["PHASE_1"] = "200.0"; p["FOLD_1"] = "0.5"
p["ATTACK_1"] = "2.0"; p["DECAY_1"] = "1.0"; p["SUSTAIN_1"] = "0.85"; p["RELEASE_1"] = "4.0"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "19.0"  # Harmonic Resonator
p["FX_RATIO_1"] = "8.0"; p["FX_DETUNE_1"] = "38.0"; p["FX_PHASE_1"] = "200.0"; p["FX_FOLD_1"] = "0.9"
p["FX_TYPE_2"] = "20.0"  # Ambient
p["FX_RATIO_2"] = "13.0"; p["FX_DETUNE_2"] = "43.0"; p["FX_PHASE_2"] = "280.0"; p["FX_FOLD_2"] = "0.85"
presets.append(("43_Space_Chord", p))

# ─────────────────────────────────────────────
# 44. Dirty Reese - FM bass, distorted
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "2.0"
p["RATIO_2"] = "1.0"; p["DETUNE_2"] = "3.0"
p["MOD_1_0"] = "6.0"; p["MOD_1_1"] = "2.0"
p["ATTACK_1"] = "0.01"; p["DECAY_1"] = "0.2"; p["SUSTAIN_1"] = "0.8"; p["RELEASE_1"] = "0.15"
p["OUT_1"] = "0.8"; p["OUT_2"] = "0.0"
p["FX_TYPE_1"] = "17.0"  # Distortion
p["FX_RATIO_1"] = "7.0"; p["FX_DETUNE_1"] = "28.0"; p["FX_PHASE_1"] = "160.0"; p["FX_FOLD_1"] = "0.5"
p["FX_TYPE_2"] = "1.0"  # LP
p["FX_RATIO_2"] = "6.0"; p["FX_DETUNE_2"] = "35.0"
presets.append(("44_Dirty_Reese", p))

# ─────────────────────────────────────────────
# 45. Juno Pad - Old chorus, classic 80s
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "2.0"  # Saw
p["WAVE_SHAPE_2"] = "2.0"; p["DETUNE_2"] = "10.0"; p["OUT_2"] = "0.7"
p["WAVE_SHAPE_3"] = "3.0"; p["OUT_3"] = "0.4"  # Square layer
p["ATTACK_1"] = "0.05"; p["DECAY_1"] = "0.2"; p["SUSTAIN_1"] = "0.9"; p["RELEASE_1"] = "0.6"
p["ATTACK_2"] = "0.05"; p["DECAY_2"] = "0.2"; p["SUSTAIN_2"] = "0.9"; p["RELEASE_2"] = "0.6"
p["ATTACK_3"] = "0.05"; p["DECAY_3"] = "0.2"; p["SUSTAIN_3"] = "0.9"; p["RELEASE_3"] = "0.6"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "21.0"  # Old Chorus
p["FX_RATIO_1"] = "2.0"; p["FX_DETUNE_1"] = "30.0"; p["FX_PHASE_1"] = "270.0"; p["FX_FOLD_1"] = "0.7"
p["FX_TYPE_2"] = "10.0"  # AP Reverb
p["FX_RATIO_2"] = "8.0"; p["FX_FOLD_2"] = "0.45"
presets.append(("45_Juno_Pad", p))

# ─────────────────────────────────────────────
# 46. Laser Zap - Very fast FM decay
# ─────────────────────────────────────────────
p = make_base()
p["RATIO_1"] = "1.0"; p["RATIO_2"] = "16.0"
p["ATTACK_1"] = "0.001"; p["DECAY_1"] = "0.08"; p["SUSTAIN_1"] = "0.0"; p["RELEASE_1"] = "0.05"
p["ATTACK_2"] = "0.001"; p["DECAY_2"] = "0.04"; p["SUSTAIN_2"] = "0.0"; p["RELEASE_2"] = "0.02"
p["MOD_1_0"] = "9.0"
p["OUT_1"] = "1.0"
presets.append(("46_Laser_Zap", p))

# ─────────────────────────────────────────────
# 47. Modular Chaos - High FM feedback, all ops
# ─────────────────────────────────────────────
p = make_base()
for i in range(6):
    for j in range(6):
        if i != j:
            p[f"MOD_{i}_{j}"] = "1.5"
for i in range(1,4):
    p[f"OUT_{i}"] = "0.5"
p["ATTACK_1"] = "0.01"; p["DECAY_1"] = "0.5"; p["SUSTAIN_1"] = "0.6"; p["RELEASE_1"] = "0.5"
p["RATIO_1"] = "1.0"; p["RATIO_2"] = "2.0"; p["RATIO_3"] = "3.0"
p["RATIO_4"] = "5.0"; p["RATIO_5"] = "7.0"; p["RATIO_6"] = "11.0"
p["FX_TYPE_1"] = "11.0"  # Compressor to tame it
p["FX_RATIO_1"] = "6.0"; p["FX_DETUNE_1"] = "-15.0"; p["FX_PHASE_1"] = "120.0"; p["FX_FOLD_1"] = "0.8"
presets.append(("47_Modular_Chaos", p))

# ─────────────────────────────────────────────
# 48. Cinematic Hit - Full orchestra swell
# ─────────────────────────────────────────────
p = make_base()
p["MODE_1"] = "1.0"; p["DETUNE_1"] = "-5.0"; p["PHASE_1"] = "190.0"; p["FOLD_1"] = "0.4"
p["WAVE_SHAPE_2"] = "2.0"; p["RATIO_2"] = "1.0"; p["DETUNE_2"] = "7.0"; p["OUT_2"] = "0.6"
p["WAVE_SHAPE_3"] = "2.0"; p["RATIO_3"] = "2.0"; p["DETUNE_3"] = "-6.0"; p["OUT_3"] = "0.5"
p["WAVE_SHAPE_4"] = "7.0"; p["OUT_4"] = "0.2"  # Pink noise
for i in range(1,5):
    p[f"ATTACK_{i}"] = "1.5"
    p[f"DECAY_{i}"] = "0.5"
    p[f"SUSTAIN_{i}"] = "0.9"
    p[f"RELEASE_{i}"] = "3.0"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "10.0"; p["FX_RATIO_1"] = "11.0"; p["FX_DETUNE_1"] = "42.0"; p["FX_FOLD_1"] = "0.6"
p["FX_TYPE_2"] = "19.0"; p["FX_RATIO_2"] = "7.0"; p["FX_DETUNE_2"] = "38.0"; p["FX_PHASE_2"] = "180.0"; p["FX_FOLD_2"] = "0.75"
presets.append(("48_Cinematic_Hit", p))

# ─────────────────────────────────────────────
# 49. Talking Bass - Formant + FM bass
# ─────────────────────────────────────────────
p = make_base()
p["WAVE_SHAPE_1"] = "2.0"
p["RATIO_2"] = "2.0"; p["MOD_1_0"] = "4.0"
p["ATTACK_1"] = "0.01"; p["DECAY_1"] = "0.3"; p["SUSTAIN_1"] = "0.7"; p["RELEASE_1"] = "0.2"
p["OUT_1"] = "1.0"
p["FX_TYPE_1"] = "6.0"  # Formant
p["FX_RATIO_1"] = "3.0"; p["FX_DETUNE_1"] = "25.0"; p["FX_PHASE_1"] = "200.0"; p["FX_FOLD_1"] = "0.4"
p["FX_TYPE_2"] = "4.0"  # Comb
p["FX_RATIO_2"] = "5.0"; p["FX_DETUNE_2"] = "20.0"; p["FX_FOLD_2"] = "0.45"
presets.append(("49_Talking_Bass", p))

# ─────────────────────────────────────────────
# 50. Full Orchestra - All operators, complex routing
# ─────────────────────────────────────────────
p = make_base()
# Strings layer
p["WAVE_SHAPE_1"] = "2.0"; p["DETUNE_1"] = "6.0"; p["OUT_1"] = "0.7"
p["ATTACK_1"] = "0.8"; p["DECAY_1"] = "0.4"; p["SUSTAIN_1"] = "0.9"; p["RELEASE_1"] = "1.5"
# Brass layer
p["WAVE_SHAPE_2"] = "2.0"; p["RATIO_2"] = "1.0"; p["DETUNE_2"] = "-6.0"; p["OUT_2"] = "0.6"
p["ATTACK_2"] = "0.02"; p["DECAY_2"] = "0.1"; p["SUSTAIN_2"] = "0.8"; p["RELEASE_2"] = "0.3"
# Piano layer
p["RATIO_3"] = "1.0"; p["RATIO_4"] = "14.0"; p["MOD_3_2"] = "3.0"; p["OUT_3"] = "0.5"
p["ATTACK_3"] = "0.001"; p["DECAY_3"] = "1.5"; p["SUSTAIN_3"] = "0.0"; p["RELEASE_3"] = "0.8"
# Bass layer
p["WAVE_SHAPE_5"] = "2.0"; p["RATIO_5"] = "0.5"; p["OUT_5"] = "0.8"
p["ATTACK_5"] = "0.01"; p["DECAY_5"] = "0.3"; p["SUSTAIN_5"] = "1.0"; p["RELEASE_5"] = "0.2"
# Percussion
p["WAVE_SHAPE_6"] = "6.0"; p["OUT_6"] = "0.3"
p["ATTACK_6"] = "0.001"; p["DECAY_6"] = "0.1"; p["SUSTAIN_6"] = "0.0"; p["RELEASE_6"] = "0.05"
p["FX_TYPE_1"] = "10.0"; p["FX_RATIO_1"] = "10.0"; p["FX_DETUNE_1"] = "40.0"; p["FX_FOLD_1"] = "0.5"
p["FX_TYPE_2"] = "21.0"; p["FX_RATIO_2"] = "2.0"; p["FX_FOLD_2"] = "0.5"
p["FX_TYPE_3"] = "11.0"; p["FX_RATIO_3"] = "5.0"; p["FX_DETUNE_3"] = "-10.0"; p["FX_PHASE_3"] = "100.0"; p["FX_FOLD_3"] = "0.7"
presets.append(("50_Full_Orchestra", p))

# Write all presets
for name, params in presets:
    write_preset(name, params)

print(f"Generated {len(presets)} presets")
