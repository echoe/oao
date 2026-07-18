import os

NUM_OPS = 10
NUM_FX = 6

OUT_DIR = "./out"
os.makedirs(OUT_DIR, exist_ok=True)

WAVE = {"Sine": 0, "Triangle": 1, "Saw": 2, "Square": 3, "Pulse": 4,
        "SquarePWM": 5, "WhiteNoise": 6, "PinkNoise": 7}

FX = {
    "None": 0, "Lowpass": 1, "Highpass": 2, "Bandpass": 3, "FilterDrive": 4,
    "Comb": 5, "Formant": 6, "Compressor": 7, "EQ3": 8, "OTT": 9, "Lofi": 10,
    "Tape": 11, "Chorus": 12, "OldChorus": 13, "Distortion": 14, "Bitcrush": 15,
    "RingMod": 16, "APReverb": 17, "APDelay": 18, "TimeshiftDelay": 19,
    "ShimmerDelay": 20, "DJFXDelay": 21, "Scatter": 22, "Granular": 23,
    "ColorBass": 24, "SpectralFreeze": 25, "Looper": 26,
}

# --- FX knob helpers: convert a normalized 0..1 "meaning" into the raw param ---
# (verified against PluginProcessor.cpp's per-effect-type normalization math)
def kA(x): return 0.01 + max(0.0, min(1.0, x)) * (16.0 - 0.01)   # ratio param -> normalizedRatio
def kB(x): return max(0.0, min(1.0, x)) * 100.0 - 50.0           # detune param -> (detune+50)/100
def kC(x): return max(0.0, min(1.0, x)) * 360.0                  # phase param -> phase/360
def kD(x): return max(0.0, min(1.0, x))                          # fold param, already 0..1


def default_state():
    p = {}
    for i in range(1, NUM_OPS + 1):
        p[f"MODE_{i}"] = 0.0
        p[f"WAVE_SHAPE_{i}"] = 0.0
        p[f"FILTER_TYPE_{i}"] = 0.0
        p[f"FREQ_MODE_{i}"] = 0.0
        p[f"RATIO_{i}"] = 1.0
        p[f"DETUNE_{i}"] = 0.0
        p[f"PHASE_{i}"] = 0.0
        p[f"FOLD_{i}"] = 0.0
        p[f"ATTACK_{i}"] = 0.1
        p[f"DECAY_{i}"] = 0.2
        p[f"SUSTAIN_{i}"] = 0.8
        p[f"RELEASE_{i}"] = 0.5
        p[f"OUT_{i}"] = 1.0 if i == 1 else 0.0
    for i in range(1, NUM_FX + 1):
        p[f"FX_TYPE_{i}"] = 0.0
        p[f"FX_MIX_{i}"] = 1.0
        p[f"FX_RATIO_{i}"] = 1.0
        p[f"FX_DETUNE_{i}"] = 0.0
        p[f"FX_PHASE_{i}"] = 0.0
        p[f"FX_FOLD_{i}"] = 0.0
        p[f"FX_LFO_WAVE_{i}"] = 0.0
        p[f"FX_LFO_SYNC_{i}"] = 0.0
        p[f"FX_LFO_RATE_{i}"] = 1.0
        p[f"FX_LFO_RATE_SYNC_{i}"] = 5.0
        p[f"FX_LFO_DEPTH_{i}"] = 0.0
        p[f"FX_LFO_TGT_{i}"] = 0.0
        p[f"FX_LFO_DEPTH2_{i}"] = 0.0
        p[f"FX_LFO_TGT2_{i}"] = 0.0
    for s in range(NUM_OPS):
        for d in range(NUM_OPS):
            p[f"MOD_{s}_{d}"] = 0.0
            p[f"AUDIO_ROUTE_{s}_{d}"] = 0.0
    for i in range(1, 6 + 1):
        p[f"MOD_SRC_{i}"] = 0.0
        p[f"MOD_TGT_{i}"] = 0.0
        p[f"MOD_AMT_{i}"] = 0.0
    for i in range(1, 4 + 1):
        p[f"MACRO_VAL_{i}"] = 0.0
        p[f"MACRO_TGT_A_{i}"] = 0.0
        p[f"MACRO_TGT_B_{i}"] = 0.0
    p["GAIN_CEIL"] = -0.2
    return p


def op(p, i, wave="Sine", ratio=1.0, detune=0.0, phase=0.0, fold=0.0,
       a=0.1, d=0.2, s=0.8, r=0.5, out=None):
    p[f"WAVE_SHAPE_{i}"] = float(WAVE[wave])
    p[f"RATIO_{i}"] = float(ratio)
    p[f"DETUNE_{i}"] = float(detune)
    p[f"PHASE_{i}"] = float(phase)
    p[f"FOLD_{i}"] = float(fold)
    p[f"ATTACK_{i}"] = float(a)
    p[f"DECAY_{i}"] = float(d)
    p[f"SUSTAIN_{i}"] = float(s)
    p[f"RELEASE_{i}"] = float(r)
    if out is not None:
        p[f"OUT_{i}"] = float(out)


def fm(p, src, dst, amount):
    """src modulates dst's phase. 1-indexed operator numbers in, 0-indexed param names out."""
    p[f"MOD_{src-1}_{dst-1}"] = float(amount)


def fxslot(p, slot, type_name, mix=1.0, A=0.5, B=0.5, C=0.5, D=0.5):
    p[f"FX_TYPE_{slot}"] = float(FX[type_name])
    p[f"FX_MIX_{slot}"] = float(mix)
    p[f"FX_RATIO_{slot}"] = kA(A)
    p[f"FX_DETUNE_{slot}"] = kB(B)
    p[f"FX_PHASE_{slot}"] = kC(C)
    p[f"FX_FOLD_{slot}"] = kD(D)


def write_preset(name, p):
    lines = ['<?xml version="1.0" encoding="UTF-8"?>', '', '<Parameters>']
    for key in sorted(p.keys()):
        lines.append(f'  <PARAM id="{key}" value="{p[key]}"/>')
    lines.append('  <SampleData/>')
    lines.append('</Parameters>')
    lines.append('')
    path = os.path.join(OUT_DIR, name + ".xml")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


presets = []  # list of (name, state)


def add(name, builder_fn):
    p = default_state()
    builder_fn(p)
    presets.append((name, p))


# ============================================================================
# BASS (8)
# ============================================================================

def bass_deep_sine(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.001, d=0.15, s=0.9, r=0.2, out=1.0)
    op(p, 2, wave="Sine", ratio=1.0, a=0.001, d=0.2, s=0.0, r=0.1, out=0.0)
    fm(p, 2, 1, 0.6)
    fxslot(p, 1, "Compressor", A=0.3, B=0.6, C=0.05, D=0.6)
    fxslot(p, 2, "Lowpass", A=0.35, B=0.15, C=0, D=0)
add("Deep Sine Bass", bass_deep_sine)


def bass_reese(p):
    op(p, 1, wave="Saw", ratio=1.0, detune=-6, a=0.001, d=0.3, s=0.85, r=0.3, out=1.0)
    op(p, 2, wave="Saw", ratio=1.0, detune=6, a=0.001, d=0.3, s=0.85, r=0.3, out=1.0)
    fxslot(p, 1, "Lowpass", A=0.3, B=0.25, C=0, D=0)
    fxslot(p, 2, "Chorus", A=0.2, B=0.3, C=0.4, D=0.3)
    fxslot(p, 3, "Distortion", mix=0.25, A=0.35, B=0.2, C=0.5, D=0.1)
add("Reese Growl", bass_reese)


def bass_fm_punch(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.001, d=0.1, s=0.4, r=0.15, out=1.0)
    op(p, 2, wave="Sine", ratio=2.0, a=0.001, d=0.08, s=0.0, r=0.05, out=0.0)
    fm(p, 2, 1, 2.4)
    fxslot(p, 1, "FilterDrive", A=0.32, B=0.3, C=0.4, D=0.0)
    fxslot(p, 2, "Compressor", A=0.25, B=0.7, C=0.02, D=0.3)
add("FM Punch Bass", bass_fm_punch)


def bass_sub_wobble(p):
    op(p, 1, wave="Sine", ratio=0.5, a=0.005, d=0.2, s=1.0, r=0.4, out=1.0)
    op(p, 2, wave="Triangle", ratio=1.0, a=0.01, d=0.2, s=0.6, r=0.3, out=0.5)
    fxslot(p, 1, "Lowpass", A=0.22, B=0.2, C=0, D=0)
    # FX LFO wobbles the lowpass cutoff (Knob A of FX slot 1)
    p["FX_LFO_WAVE_1"] = float(WAVE["Sine"]) if False else 0.0
    p["FX_LFO_RATE_1"] = 3.0
    p["FX_LFO_DEPTH_1"] = 0.35
    p["FX_LFO_TGT_1"] = 32.0  # FX 1 Knob A (fxtargets(): None=1, FX1 KnobA=2... wait see note below
add("Sub Wobble", bass_sub_wobble)


def bass_acid(p):
    op(p, 1, wave="Square", ratio=1.0, a=0.001, d=0.15, s=0.6, r=0.1, out=1.0)
    fxslot(p, 1, "Lowpass", A=0.28, B=0.55, C=0, D=0)
    fxslot(p, 2, "Distortion", mix=0.4, A=0.3, B=0.25, C=0.55, D=0.15)
    p["FX_LFO_RATE_1"] = 2.0
    p["FX_LFO_DEPTH_1"] = 0.25
add("Acid Squelch", bass_acid)


def bass_wide_octave(p):
    op(p, 1, wave="Saw", ratio=1.0, a=0.001, d=0.2, s=0.9, r=0.2, out=0.9)
    op(p, 2, wave="Saw", ratio=2.0, a=0.001, d=0.2, s=0.7, r=0.2, out=0.4)
    fxslot(p, 1, "Lowpass", A=0.3, B=0.2, C=0, D=0)
    fxslot(p, 2, "EQ3", A=0.6, B=0.5, C=0.4, D=0.7)
add("Wide Octave Bass", bass_wide_octave)


def bass_pluck(p):
    op(p, 1, wave="Triangle", ratio=1.0, a=0.001, d=0.12, s=0.0, r=0.08, out=1.0)
    op(p, 2, wave="Sine", ratio=3.0, a=0.001, d=0.05, s=0.0, r=0.05, out=0.0)
    fm(p, 2, 1, 0.9)
    fxslot(p, 1, "Highpass", A=0.05, B=0.1, C=0, D=0)
    fxslot(p, 2, "APDelay", mix=0.15, A=0.15, B=0.2, C=0.3, D=0.4)
add("Plucky Sub Bass", bass_pluck)


def bass_dirty(p):
    op(p, 1, wave="Square", ratio=1.0, detune=0, a=0.001, d=0.18, s=0.8, r=0.15, out=1.0)
    op(p, 2, wave="Square", ratio=1.0, detune=-4, a=0.001, d=0.18, s=0.8, r=0.15, out=0.6)
    fxslot(p, 1, "Bitcrush", A=0.55, B=0.35, C=0.1, D=0.1)
    fxslot(p, 2, "Lowpass", A=0.32, B=0.3, C=0, D=0)
    fxslot(p, 3, "Compressor", A=0.3, B=0.65, C=0.05, D=0.4)
add("Dirty Crunch Bass", bass_dirty)


# ============================================================================
# PADS (8)
# ============================================================================

def pad_warm_strings(p):
    op(p, 1, wave="Saw", ratio=1.0, detune=-5, a=1.2, d=0.8, s=0.85, r=1.6, out=0.7)
    op(p, 2, wave="Saw", ratio=1.0, detune=5, a=1.2, d=0.8, s=0.85, r=1.6, out=0.7)
    op(p, 3, wave="Triangle", ratio=2.0, a=1.4, d=0.9, s=0.6, r=1.8, out=0.3)
    fxslot(p, 1, "Chorus", A=0.15, B=0.4, C=0.5, D=0.6)
    fxslot(p, 2, "APReverb", mix=0.35, A=0.6, B=0.7, C=0.5, D=0.4)
add("Warm Strings Pad", pad_warm_strings)


def pad_glass(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.8, d=1.0, s=0.7, r=2.0, out=1.0)
    op(p, 2, wave="Sine", ratio=4.01, a=1.0, d=1.0, s=0.3, r=2.0, out=0.0)
    fm(p, 2, 1, 0.5)
    fxslot(p, 1, "ShimmerDelay", mix=0.3, A=0.4, B=0.35, C=0.6, D=0.5)
    fxslot(p, 2, "APReverb", mix=0.4, A=0.7, B=0.75, C=0.5, D=0.4)
add("Glass Pad", pad_glass)


def pad_choir(p):
    op(p, 1, wave="Saw", ratio=1.0, a=0.9, d=0.6, s=0.9, r=1.5, out=0.8)
    op(p, 2, wave="Saw", ratio=1.0, detune=8, a=0.9, d=0.6, s=0.9, r=1.5, out=0.5)
    fxslot(p, 1, "Formant", A=0.3, B=0.5, C=0.4, D=0.3)
    fxslot(p, 2, "Chorus", A=0.2, B=0.3, C=0.5, D=0.5)
    fxslot(p, 3, "APReverb", mix=0.3, A=0.65, B=0.6, C=0.5, D=0.4)
add("Ghost Choir Pad", pad_choir)


def pad_analog_slow(p):
    op(p, 1, wave="Square", ratio=1.0, detune=-4, a=1.5, d=1.0, s=0.8, r=2.2, out=0.6)
    op(p, 2, wave="Saw", ratio=1.0, detune=4, a=1.5, d=1.0, s=0.8, r=2.2, out=0.6)
    fxslot(p, 1, "Lowpass", A=0.35, B=0.2, C=0, D=0)
    fxslot(p, 2, "OldChorus", A=0.25, B=0.4, C=0.3, D=0.5)
    fxslot(p, 3, "APReverb", mix=0.25, A=0.55, B=0.6, C=0.5, D=0.4)
add("Slow Analog Swell", pad_analog_slow)


def pad_fm_evolving(p):
    op(p, 1, wave="Sine", ratio=1.0, a=1.0, d=1.2, s=0.6, r=2.0, out=1.0)
    op(p, 2, wave="Sine", ratio=1.5, a=1.5, d=1.0, s=0.4, r=2.0, out=0.0)
    fm(p, 2, 1, 0.0)
    # macro 1 sweeps the FM index live (Op2 -> Op1 matrix cell) alongside a filter
    p["MACRO_TGT_A_1"] = 47.0  # Op 2 -> Op 1 matrix cell (see note below on indices)
    p["MACRO_TGT_B_1"] = 32.0  # FX 1 Knob A
    fxslot(p, 1, "Lowpass", A=0.5, B=0.2, C=0, D=0)
    fxslot(p, 2, "APReverb", mix=0.3, A=0.6, B=0.6, C=0.5, D=0.4)
add("Evolving FM Pad", pad_fm_evolving)


def pad_dark_drone(p):
    op(p, 1, wave="Triangle", ratio=1.0, a=2.0, d=1.0, s=0.9, r=3.0, out=0.8)
    op(p, 2, wave="Triangle", ratio=2.005, a=2.0, d=1.0, s=0.9, r=3.0, out=0.4)
    fxslot(p, 1, "Lowpass", A=0.2, B=0.15, C=0, D=0)
    fxslot(p, 2, "APReverb", mix=0.5, A=0.8, B=0.85, C=0.5, D=0.6)
add("Dark Drone Pad", pad_dark_drone)


def pad_music_box(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.01, d=1.5, s=0.0, r=1.0, out=1.0)
    op(p, 2, wave="Sine", ratio=3.5, a=0.01, d=1.2, s=0.0, r=0.8, out=0.0)
    fm(p, 2, 1, 0.7)
    fxslot(p, 1, "APDelay", mix=0.3, A=0.25, B=0.35, C=0.4, D=0.3)
    fxslot(p, 2, "APReverb", mix=0.35, A=0.6, B=0.6, C=0.5, D=0.4)
add("Music Box Pad", pad_music_box)


def pad_vox_wave(p):
    op(p, 1, wave="Square", ratio=1.0, a=0.6, d=0.5, s=0.85, r=1.2, out=0.9)
    fxslot(p, 1, "Formant", A=0.45, B=0.55, C=0.5, D=0.35)
    fxslot(p, 2, "Chorus", A=0.2, B=0.3, C=0.5, D=0.4)
    fxslot(p, 3, "APReverb", mix=0.3, A=0.55, B=0.6, C=0.5, D=0.4)
add("Vox Wave Pad", pad_vox_wave)


# ============================================================================
# KEYS / PLUCKS (8)
# ============================================================================

def keys_ep_classic(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.001, d=1.0, s=0.3, r=0.8, out=1.0)
    op(p, 2, wave="Sine", ratio=2.0, a=0.001, d=0.6, s=0.0, r=0.4, out=0.0)
    fm(p, 2, 1, 1.1)
    fxslot(p, 1, "Tape", A=0.3, B=0.2, C=0.3, D=0.2)
    fxslot(p, 2, "APDelay", mix=0.2, A=0.2, B=0.25, C=0.3, D=0.3)
add("Classic EP", keys_ep_classic)


def keys_bell_ep(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.001, d=1.2, s=0.2, r=1.0, out=1.0)
    op(p, 2, wave="Sine", ratio=3.98, a=0.001, d=0.7, s=0.0, r=0.5, out=0.0)
    fm(p, 2, 1, 0.9)
    fxslot(p, 1, "Chorus", A=0.15, B=0.25, C=0.4, D=0.3)
    fxslot(p, 2, "APReverb", mix=0.25, A=0.45, B=0.5, C=0.5, D=0.3)
add("Bell EP", keys_bell_ep)


def keys_wurly(p):
    op(p, 1, wave="Triangle", ratio=1.0, a=0.001, d=0.9, s=0.35, r=0.7, out=1.0)
    op(p, 2, wave="Square", ratio=1.0, a=0.001, d=0.4, s=0.0, r=0.3, out=0.15)
    fxslot(p, 1, "OldChorus", A=0.25, B=0.3, C=0.35, D=0.4)
    fxslot(p, 2, "FilterDrive", A=0.5, B=0.25, C=0.15, D=0.0)
add("Wurly Keys", keys_wurly)


def pluck_glass(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.001, d=0.4, s=0.0, r=0.3, out=1.0)
    op(p, 2, wave="Sine", ratio=5.0, a=0.001, d=0.2, s=0.0, r=0.15, out=0.0)
    fm(p, 2, 1, 1.6)
    fxslot(p, 1, "APDelay", mix=0.25, A=0.2, B=0.3, C=0.4, D=0.4)
    fxslot(p, 2, "APReverb", mix=0.2, A=0.4, B=0.45, C=0.5, D=0.3)
add("Glass Pluck", pluck_glass)


def pluck_koto(p):
    op(p, 1, wave="Triangle", ratio=1.0, a=0.001, d=0.5, s=0.0, r=0.4, out=1.0)
    op(p, 2, wave="Sine", ratio=2.76, a=0.001, d=0.3, s=0.0, r=0.2, out=0.0)
    fm(p, 2, 1, 1.3)
    fxslot(p, 1, "Highpass", A=0.08, B=0.1, C=0, D=0)
    fxslot(p, 2, "APDelay", mix=0.3, A=0.22, B=0.3, C=0.35, D=0.35)
add("Koto Pluck", pluck_koto)


def keys_organ(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.005, d=0.05, s=1.0, r=0.15, out=1.0)
    op(p, 2, wave="Sine", ratio=2.0, a=0.005, d=0.05, s=1.0, r=0.15, out=0.5)
    op(p, 3, wave="Sine", ratio=3.0, a=0.005, d=0.05, s=1.0, r=0.15, out=0.25)
    fxslot(p, 1, "OldChorus", A=0.2, B=0.35, C=0.3, D=0.4)
    fxslot(p, 2, "EQ3", A=0.55, B=0.5, C=0.45, D=0.6)
add("Drawbar Organ", keys_organ)


def pluck_marimba(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.001, d=0.35, s=0.0, r=0.2, out=1.0)
    op(p, 2, wave="Sine", ratio=4.0, a=0.001, d=0.1, s=0.0, r=0.08, out=0.0)
    fm(p, 2, 1, 0.6)
    fxslot(p, 1, "Lowpass", A=0.5, B=0.15, C=0, D=0)
    fxslot(p, 2, "APReverb", mix=0.15, A=0.35, B=0.4, C=0.5, D=0.3)
add("Marimba Pluck", pluck_marimba)


def keys_toy_piano(p):
    op(p, 1, wave="Triangle", ratio=1.0, a=0.001, d=0.6, s=0.1, r=0.4, out=1.0)
    op(p, 2, wave="Sine", ratio=6.0, a=0.001, d=0.15, s=0.0, r=0.1, out=0.0)
    fm(p, 2, 1, 0.5)
    fxslot(p, 1, "Lofi", A=0.3, B=0.6, C=0.2, D=0.3)
    fxslot(p, 2, "APDelay", mix=0.15, A=0.18, B=0.2, C=0.3, D=0.25)
add("Toy Piano", keys_toy_piano)


# ============================================================================
# LEADS (8)
# ============================================================================

def lead_hard_saw(p):
    op(p, 1, wave="Saw", ratio=1.0, detune=-7, a=0.005, d=0.15, s=0.9, r=0.2, out=0.7)
    op(p, 2, wave="Saw", ratio=1.0, detune=7, a=0.005, d=0.15, s=0.9, r=0.2, out=0.7)
    fxslot(p, 1, "Lowpass", A=0.55, B=0.3, C=0, D=0)
    fxslot(p, 2, "Distortion", mix=0.3, A=0.3, B=0.25, C=0.5, D=0.15)
    fxslot(p, 3, "APDelay", mix=0.2, A=0.18, B=0.25, C=0.3, D=0.3)
add("Hard Saw Lead", lead_hard_saw)


def lead_fm_screamer(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.005, d=0.1, s=0.8, r=0.15, out=1.0)
    op(p, 2, wave="Sine", ratio=2.0, a=0.005, d=0.15, s=0.6, r=0.15, out=0.0)
    fm(p, 2, 1, 3.2)
    fxslot(p, 1, "Distortion", mix=0.35, A=0.4, B=0.3, C=0.5, D=0.2)
    fxslot(p, 2, "APDelay", mix=0.25, A=0.2, B=0.3, C=0.35, D=0.3)
add("FM Screamer Lead", lead_fm_screamer)


def lead_square_chip(p):
    op(p, 1, wave="Square", ratio=1.0, a=0.001, d=0.1, s=0.85, r=0.1, out=1.0)
    fxslot(p, 1, "Bitcrush", A=0.4, B=0.3, C=0.15, D=0.1)
    fxslot(p, 2, "APDelay", mix=0.2, A=0.15, B=0.2, C=0.25, D=0.25)
add("Chiptune Square Lead", lead_square_chip)


def lead_supersaw(p):
    op(p, 1, wave="Saw", ratio=1.0, detune=-10, a=0.01, d=0.2, s=0.9, r=0.3, out=0.5)
    op(p, 2, wave="Saw", ratio=1.0, detune=0, a=0.01, d=0.2, s=0.9, r=0.3, out=0.5)
    op(p, 3, wave="Saw", ratio=1.0, detune=10, a=0.01, d=0.2, s=0.9, r=0.3, out=0.5)
    fxslot(p, 1, "Lowpass", A=0.6, B=0.3, C=0, D=0)
    fxslot(p, 2, "Chorus", A=0.2, B=0.3, C=0.5, D=0.5)
    fxslot(p, 3, "APDelay", mix=0.2, A=0.2, B=0.25, C=0.3, D=0.3)
add("Supersaw Lead", lead_supersaw)


def lead_pwm_synth(p):
    op(p, 1, wave="SquarePWM", ratio=1.0, a=0.01, d=0.2, s=0.85, r=0.3, out=1.0)
    fxslot(p, 1, "Chorus", A=0.25, B=0.35, C=0.5, D=0.4)
    fxslot(p, 2, "Lowpass", A=0.6, B=0.25, C=0, D=0)
add("PWM Synth Lead", lead_pwm_synth)


def lead_metallic_edge(p):
    op(p, 1, wave="Saw", ratio=1.0, a=0.01, d=0.15, s=0.8, r=0.2, out=1.0)
    op(p, 2, wave="Sine", ratio=3.14, a=0.01, d=0.15, s=0.4, r=0.2, out=0.0)
    fm(p, 2, 1, 1.4)
    fxslot(p, 1, "Highpass", A=0.15, B=0.2, C=0, D=0)
    fxslot(p, 2, "APDelay", mix=0.3, A=0.22, B=0.3, C=0.35, D=0.3)
add("Metallic Edge Lead", lead_metallic_edge)


def lead_soft_flute(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.08, d=0.2, s=0.85, r=0.4, out=1.0)
    op(p, 2, wave="Sine", ratio=2.0, a=0.1, d=0.2, s=0.3, r=0.4, out=0.0)
    fm(p, 2, 1, 0.4)
    fxslot(p, 1, "APReverb", mix=0.2, A=0.4, B=0.4, C=0.5, D=0.3)
add("Soft Flute Lead", lead_soft_flute)


def lead_feedback_growl(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.005, d=0.1, s=0.8, r=0.2, out=1.0)
    fm(p, 1, 1, 1.8)  # self feedback for growl/noise character
    fxslot(p, 1, "Lowpass", A=0.4, B=0.35, C=0, D=0)
    fxslot(p, 2, "Distortion", mix=0.2, A=0.25, B=0.2, C=0.4, D=0.1)
add("Feedback Growl Lead", lead_feedback_growl)


# ============================================================================
# BELLS / METALLIC (6)
# ============================================================================

def bell_classic_fm(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.001, d=2.0, s=0.0, r=1.5, out=1.0)
    op(p, 2, wave="Sine", ratio=3.5, a=0.001, d=1.2, s=0.0, r=1.0, out=0.0)
    fm(p, 2, 1, 2.0)
    fxslot(p, 1, "APReverb", mix=0.35, A=0.6, B=0.6, C=0.5, D=0.4)
add("Classic FM Bell", bell_classic_fm)


def bell_tubular(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.001, d=3.0, s=0.0, r=2.0, out=1.0)
    op(p, 2, wave="Sine", ratio=2.41, a=0.001, d=2.0, s=0.0, r=1.5, out=0.0)
    fm(p, 2, 1, 1.1)
    fxslot(p, 1, "Comb", A=0.5, B=0.4, C=0, D=0.7)
    fxslot(p, 2, "APReverb", mix=0.4, A=0.7, B=0.7, C=0.5, D=0.5)
add("Tubular Bell", bell_tubular)


def bell_music_gong(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.001, d=4.0, s=0.0, r=3.0, out=1.0)
    op(p, 2, wave="Sine", ratio=1.732, a=0.001, d=3.0, s=0.0, r=2.0, out=0.0)
    op(p, 3, wave="Sine", ratio=4.24, a=0.001, d=2.0, s=0.0, r=1.5, out=0.0)
    fm(p, 2, 1, 1.5)
    fm(p, 3, 1, 0.8)
    fxslot(p, 1, "APReverb", mix=0.5, A=0.8, B=0.8, C=0.5, D=0.6)
add("Gong Wash", bell_music_gong)


def metallic_clangor(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.001, d=1.5, s=0.0, r=1.0, out=1.0)
    op(p, 2, wave="Sine", ratio=5.13, a=0.001, d=1.0, s=0.0, r=0.7, out=0.0)
    fm(p, 2, 1, 2.8)
    fxslot(p, 1, "Highpass", A=0.15, B=0.15, C=0, D=0)
    fxslot(p, 2, "APReverb", mix=0.3, A=0.5, B=0.5, C=0.5, D=0.4)
add("Metallic Clangor", metallic_clangor)


def bell_soft_celesta(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.001, d=0.9, s=0.0, r=0.7, out=1.0)
    op(p, 2, wave="Sine", ratio=4.0, a=0.001, d=0.5, s=0.0, r=0.3, out=0.0)
    fm(p, 2, 1, 0.6)
    fxslot(p, 1, "Chorus", A=0.1, B=0.2, C=0.4, D=0.2)
    fxslot(p, 2, "APReverb", mix=0.3, A=0.5, B=0.5, C=0.5, D=0.35)
add("Soft Celesta", bell_soft_celesta)


def bell_dark_temple(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.001, d=3.5, s=0.0, r=2.5, out=1.0)
    op(p, 2, wave="Sine", ratio=2.003, a=0.001, d=2.5, s=0.0, r=1.8, out=0.0)
    fm(p, 2, 1, 1.7)
    fxslot(p, 1, "Lowpass", A=0.4, B=0.2, C=0, D=0)
    fxslot(p, 2, "APReverb", mix=0.55, A=0.85, B=0.85, C=0.5, D=0.6)
add("Dark Temple Bell", bell_dark_temple)


# ============================================================================
# BRASS / ORGAN / VOX (6)
# ============================================================================

def brass_section(p):
    op(p, 1, wave="Saw", ratio=1.0, a=0.05, d=0.2, s=0.9, r=0.3, out=1.0)
    op(p, 2, wave="Sine", ratio=1.0, a=0.05, d=0.2, s=0.6, r=0.3, out=0.0)
    fm(p, 2, 1, 0.6)
    fxslot(p, 1, "EQ3", A=0.6, B=0.55, C=0.5, D=0.6)
    fxslot(p, 2, "Distortion", mix=0.15, A=0.2, B=0.2, C=0.4, D=0.1)
    fxslot(p, 3, "APReverb", mix=0.2, A=0.45, B=0.5, C=0.5, D=0.3)
add("Brass Section", brass_section)


def organ_church(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.02, d=0.05, s=1.0, r=0.3, out=0.8)
    op(p, 2, wave="Sine", ratio=2.0, a=0.02, d=0.05, s=1.0, r=0.3, out=0.4)
    op(p, 3, wave="Sine", ratio=3.0, a=0.02, d=0.05, s=1.0, r=0.3, out=0.25)
    op(p, 4, wave="Sine", ratio=4.0, a=0.02, d=0.05, s=1.0, r=0.3, out=0.15)
    fxslot(p, 1, "APReverb", mix=0.45, A=0.75, B=0.75, C=0.5, D=0.5)
add("Church Organ", organ_church)


def vox_ooh(p):
    op(p, 1, wave="Saw", ratio=1.0, a=0.1, d=0.3, s=0.85, r=0.5, out=1.0)
    fxslot(p, 1, "Formant", A=0.15, B=0.5, C=0.3, D=0.25)
    fxslot(p, 2, "Chorus", A=0.15, B=0.25, C=0.4, D=0.3)
    fxslot(p, 3, "APReverb", mix=0.2, A=0.4, B=0.45, C=0.5, D=0.3)
add("Vox Ooh", vox_ooh)


def vox_aah_lead(p):
    op(p, 1, wave="Square", ratio=1.0, a=0.08, d=0.3, s=0.85, r=0.5, out=1.0)
    fxslot(p, 1, "Formant", A=0.55, B=0.5, C=0.35, D=0.3)
    fxslot(p, 2, "APDelay", mix=0.2, A=0.2, B=0.25, C=0.3, D=0.3)
add("Vox Aah Lead", vox_aah_lead)


def brass_synth_stab(p):
    op(p, 1, wave="Saw", ratio=1.0, a=0.001, d=0.25, s=0.7, r=0.2, out=1.0)
    op(p, 2, wave="Saw", ratio=1.0, detune=-5, a=0.001, d=0.25, s=0.7, r=0.2, out=0.6)
    fxslot(p, 1, "FilterDrive", A=0.55, B=0.3, C=0.2, D=0.0)
    fxslot(p, 2, "EQ3", A=0.55, B=0.5, C=0.5, D=0.6)
add("Synth Stab Brass", brass_synth_stab)


def organ_gritty(p):
    op(p, 1, wave="Square", ratio=1.0, a=0.01, d=0.1, s=1.0, r=0.2, out=0.8)
    op(p, 2, wave="Square", ratio=3.0, a=0.01, d=0.1, s=1.0, r=0.2, out=0.3)
    fxslot(p, 1, "Distortion", mix=0.3, A=0.25, B=0.2, C=0.4, D=0.15)
    fxslot(p, 2, "OldChorus", A=0.2, B=0.3, C=0.3, D=0.4)
add("Gritty Rock Organ", organ_gritty)


# ============================================================================
# FX / WEIRD / AMBIENT / DRUMS (6)
# ============================================================================

def fx_alien_texture(p):
    op(p, 1, wave="Triangle", ratio=1.0, a=1.5, d=1.0, s=0.7, r=2.5, out=1.0)
    op(p, 2, wave="Sine", ratio=1.5, a=2.0, d=1.0, s=0.5, r=2.5, out=0.0)
    fm(p, 2, 1, 1.2)
    fxslot(p, 1, "SpectralFreeze", A=0.6, B=0.4, C=0.5, D=0.6)
    fxslot(p, 2, "ShimmerDelay", mix=0.4, A=0.4, B=0.35, C=0.6, D=0.5)
    fxslot(p, 3, "APReverb", mix=0.4, A=0.7, B=0.7, C=0.5, D=0.5)
add("Alien Texture", fx_alien_texture)


def fx_granular_cloud(p):
    op(p, 1, wave="Triangle", ratio=1.0, a=2.0, d=1.0, s=0.8, r=3.0, out=1.0)
    fxslot(p, 1, "Granular", A=0.4, B=0.5, C=0.45, D=0.5)
    fxslot(p, 2, "APReverb", mix=0.45, A=0.75, B=0.75, C=0.5, D=0.5)
add("Granular Cloud", fx_granular_cloud)


def fx_ring_mod_alarm(p):
    op(p, 1, wave="Square", ratio=1.0, a=0.01, d=0.3, s=0.9, r=0.3, out=1.0)
    fxslot(p, 1, "RingMod", A=0.3, B=0.5, C=0.4, D=0.3)
    fxslot(p, 2, "APDelay", mix=0.3, A=0.2, B=0.3, C=0.3, D=0.3)
add("Ring Mod Alarm", fx_ring_mod_alarm)


def fx_scatter_glitch(p):
    op(p, 1, wave="Saw", ratio=1.0, a=0.01, d=0.2, s=0.8, r=0.3, out=1.0)
    fxslot(p, 1, "Scatter", A=0.5, B=0.4, C=0.45, D=0.5)
    fxslot(p, 2, "Bitcrush", A=0.35, B=0.3, C=0.15, D=0.1)
add("Scatter Glitch", fx_scatter_glitch)


def ambient_shimmer_wash(p):
    op(p, 1, wave="Sine", ratio=1.0, a=2.5, d=1.5, s=0.8, r=4.0, out=0.8)
    op(p, 2, wave="Sine", ratio=2.0, a=3.0, d=1.5, s=0.6, r=4.0, out=0.4)
    fxslot(p, 1, "ShimmerDelay", mix=0.45, A=0.5, B=0.4, C=0.65, D=0.5)
    fxslot(p, 2, "APReverb", mix=0.55, A=0.85, B=0.85, C=0.5, D=0.6)
add("Ambient Shimmer Wash", ambient_shimmer_wash)


def drum_synth_kick(p):
    op(p, 1, wave="Sine", ratio=1.0, a=0.001, d=0.3, s=0.0, r=0.2, out=1.0)
    op(p, 2, wave="Sine", ratio=0.5, a=0.001, d=0.08, s=0.0, r=0.05, out=0.0)
    fm(p, 2, 1, 1.4)
    fxslot(p, 1, "FilterDrive", A=0.3, B=0.4, C=0.5, D=0.0)
    fxslot(p, 2, "Compressor", A=0.25, B=0.75, C=0.02, D=0.3)
add("Synth Kick", drum_synth_kick)


for name, state in presets:
    write_preset(name, state)

print(f"Wrote {len(presets)} presets to {OUT_DIR}")
