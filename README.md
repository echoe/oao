# Oops! All Operators! (OAO)

<img src=https://raw.githubusercontent.com/echoe/oao/refs/heads/main/pictures/operators.png width="450" height="300" />

This is a six-operator FM synthesizer/effects unit, where the operators do everything. It's open source, written in C++ using the JUCE framework and a lot of AI help.

Thanks to both Six Sines and to the Korg Opsix for some of the inspiration behind this synth :) but I've sort of just liberally taken from stuff to make something approximating the VST of my dreams.

## Preset Bar

### Presets
There are six buttons to handle presets.
- Init: This initializes the patch to a single Sine wave, with Operator 1's Out set to 1.
- Save: This saves a patch as an .xml file.
- Load: This loads a patch from an .xml file.
- <: Scroll left in preset folder
- ...: Select preset folder. Preset name will be displayed next to this button.
- >: Scroll right in preset folder

### Randomization
There are three randomization buttons. These are after the preset buttons.
You can also go through and manually click on knobs to reset them to the default!
- Rand: This changes all synth variables. It's set to hopefully not go too wild too often. YMMV.
- D: This attempts to make drums for you (no sustain, etc).
- E: This randomizes the effects page only.

### OS
This oversamples the main audio path to provide increased fidelity. Select between 1,2,4, and 8x.

### Poly
This picks the number of voices in the synth. This defauts to 8 but you can go from 1 (mono) to 32.

### Gain
If you're using randomization you may want to turn the gain down as a precaution! Don't go deaf!!! It's easy for things to get too wild. I've tried to moderate that and haven't made all audio from my computer break for a while, but this is a freeware VST made by one developer and AI, so, uh, no guarantees.


## Operator types

### Wave: Sine, Triangle, Saw, Square, Pulse, SquarePWM, White Noise, Pink Noise
- Wave Ratio options: Standard, Sync, Frequency, LFO
-- Standard: Normal ratio.
-- Sync: this tempo syncs to the speed of your DAW.
-- Frequency: this sets the oscillator to a specific hz speed (10 to 16000 Hz, mapped logarhythmically).
-- LFO: This sets the oscillator to normal LFO speeds (0.01 to 16hz).
- Wave Options: Ratio, Detune, Phase, Fold.
-- Ratio: from 0.01 to 16, this controls the pitch of the waveform relative to the note you're playing. You, uh, need this for FM.
-- Detune: You can slightly detune the note if you want, without moving the Ratio by tiny steps.
-- Phase: Changes the phase of the waveform.
-- Fold: Folds the waveform in on itself.

For SquarePWM and Pulse, instead of Phase there is a PWM option there ... follow your PWM dreams.

Additional Notes:

- Ratio goes down to 0.01 so you can have a waveform slow enough to reasonably be used as an LFO. I may add another operator mode for this in the future if this isn't slow enough for some cases, but it's been fine for me. YMMV.
- If you want phase modulation, the phase is there to be modulated! Wow.
- PLEASE NOTE: The gain for all operators is controlled in the Audio Matrix! If you don't hear one, go to the Audio Matrix and turn it up!

### Additive
- Options: Ratio, Tilt, Stretch, Odd/Even
- Ratio: from 0.01 to 16, this controls the pitch of the waveform relative to the note you're playing. You still need this for FM.
- Tilt: Tilts the sound spectrally.
- Stretch: Changes how far away, or close together, your partials are.
- Odd/Even: Emphasizes either odd or even partials.

Additional Notes:
- It's a 32-partial additive oscillator, if you were wondering! I thought about it and it made the most sense to me just for CPU usage reasons. But IDK, maybe it's fine to be 32 or 64? You can change this to however many partials you want and build it yourself, just change 'numPartials' in 'FMOperator.h' and rebuild the synth yourself :)

### Filter:
#### None
- Options: It doesn't do anything
- Use this as a passthrough! 
#### SVF (Lowpass, Highpass, Bandpass)
- Options: Cutoff, Resonance, Keytrack, Feedback
- They're the normal filters of those kinds. The implementation here is JUCE-standard.
#### Filter + Drive
- Options: Cutoff, Resonance, Overdrive, Mode
- Modeled after a popular filter. To the left Mode is lowpass, to the right it's highpass. Gets messy very fast.
#### Comb
- Options: Cutoff, Resonance, Keytrack, Feedback
- This is modeled after the Comb effect on the Korg Opsix as described here: https://www.icemoonprison.com/blog/archives/579
- As such, you can use this for physical modeling.
#### Formant
- Options: Vowel, Nasal, Vowel Mod, Drive
- Note: Vowel Mod changes the amount of modulation coming in from the modulation matrix, if you are modulating it. So to hear it simply, you can self-modulate this operator, and then tweak the Vowel Mod, and it will change the sound then.
#### Compressor
- Options: Threshold, Ratio, Attack, Release
- it's a compressor. Wowee.
#### 3-Bar EQ
- Options: lowGain, medGain, highGain, Gain
- A simple three-bar EQ that can be used as a selective gain addition if/when wanted.
#### OTT
- Options: Depth, Time, Upward, Tone
- Modeled after the famous OTT effect. Make it more in your face!
#### Tape
- Options: Wobble, Age, Saturation, Bias
- Wobble: pitch shifting
- Age: one pole lowpass for frequency loss
- saturation: saturates the signal
- bias: biases the saturation. midway is nothing, left is harsh and bright, right is dull and soft.
- For Bias to be heard saturation must be on at least a little bit, the bias biases the saturation.
#### Chorus
- Options: Rate (speed chorus moves), Depth (size of chorus), Spread (in the stereo field), Voices (1-8)
- This is a modern chorus.
#### Old Chorus
- Options: Rate (triangle LFO rate), Depth(1.7-16ms), Mode (Left: 1-voice, right: 2-voice), Warmth (subtle smear from prev sample)
- This is modeled after a popular old chorus, and can sound more controlled and straightforwards.
#### Distortion
- Options: Drive, Flavor, Tone, Degradation
- Drive: move sound
- Flavor: morphs between distortion types. 0.0-0.25: Soft clip, 0.25-0.5 Hard clip, 0.5-0.75 Foldback, 0.75-1.0: Digital (sign function / bit mangle).
- Tone: post-distortion onepole filter
- Degradation: adds various sample-distruction effects.
#### Bitcrush
- Options: Bits (1-16), Rate (of bitcrushing), Jitter (added to output), Noise (added white noise sample in 'cracks' of output)
#### Ring Mod
- Options: Frequency (that rings out), Shape (of modulation), Depth (of modulation), Feedback (from modulation)
- Modulate everything. Ring everything.
#### Allpass Reverb
- Options: Size, Decay, Diffusion, Damping
- Size: 0.0 = small room (20ms), 1.0 = large hall (500ms)
- Decay — feedback amount controls tail length
- Diffusion — smear the output for density
- Damping - one-pole LP in the feedback path
- A simple reverb built around allpass filters.
#### Allpass Delay
- Options: Time, Feedback, Diffusion, Damping
- Time: 0.0 = 10ms, 1.0 = 1000ms
- Damping: one-pole LP in feedback path
- Diffusion 0.0 = clean echo, 1.0 = heavily smeared
- Feedback - adding feedback before return
- A delay built around allpass filters.
#### Time-Control Delay
- Options: Time, Feedback, Damping, Drive
- A delay modeled after a famous delay, that quickly feeds back into itself.
#### Ambient Shimmer
-Options: Time, Feedback, Shimmer, Diffusion
- Note: This ambient shimmer delay has a 16 second (!!!) possible time. Shimmer is a one-octave-up duplication of the sound and the knob acts as a mix knob for that.
#### DJFX Delay
- Options: Buffer, Speed, Loop On, Drift
- Modeled after a famous DJ-style delay.
- Without the loop on this functions as a varispeed that 'catches up' to the audio going through the playhead every Buffer seconds.
#### Scatter
- Options: Pattern, Size, Speed, Depth
- Pattern 0.0-0.25: Stutter (repeats short segments)
- Pattern 0.25-0.5: Reverse (play segment backwards)
- Pattern 0.5-0.75: Skip (jump forward in buffer)
- Pattern 0.75-1.0: Loop (loop a segment)
- Size: from 10ms to 500ms, control the size of the sections you're scattering around
- Speed: from half speed to double speed, change the speed that you're scattering samples
- Depth: blend scattered with dry
#### Granular
- Options: Grain size, Damping, Scatter, Feedback
- Because there aren't enough knobs and I couldn't think of a reason you wouldn't want it keytracked, this filter forces keytracking to 100%.
#### Color Bass
- Options: Drive, Shimmer [Shimmer Pitch], Tone, Decay
- Make your sound go (glittery noises)!
#### Spectral Freeze
-Options: Freeze, Blend, Pitch, Blur
Take the incoming audio. Pause it. Twist it and reverse it.

#### More?
The process to add more effects is very easy:
-Add necessary state variables to SynthFilter.h's private section, and any variables it needs to reset to the reset() call.
-Add a processing function to SynthFilter.h that actually takes input and processes the input into the effect result.
-Update Constants.h to add the effect in the FilterChoices array and in the case settings.
-In FMOperator.h, process the knobs to be correct given their start positions (which are from the default Wave 'Operators' page, these knobs are just relabeled with every change), and then pass them to the processing function.
-In the processBlock effects loop in PluginProcessor.cpp, so the Effects page can work, add the case there. This is pretty much the same as FMOperator.h but with some differences that are pretty self-evident looking at the 24 (!) current examples.
-There is also an FX-Specific plugin that now builds, where you can use the effects by themselves.

### Sample
- Import an audio file, and modify it with the effects/route it just like everything else. Initially I wanted to pass audio through the plugin but that's a giant pain for multiple reasons and JUCE has this built in so it's very easy to implement!
- You can import a sample of any size (as long as your RAM can handle it ...).
- Samples are automatically assumed to be pitched at c4 - midi notes above or below c4 pitch the sample up or down respectfully.
- Sample playback: Oneshot, Loop, Pingpong, Scatter
- Sample options: Speed, Sample Start, Loop (>.5 means yes loop, <.5 means no loop), Fold
- Note: When you save a preset, the preset saves a copy of the sample in the xml. Long samples will make very large presets.

## Modulation Matrix
- A 6 by 6 matrix letting you modulate any operator with any other operator. On the right of this, there is a 6-slot mod matrix, letting you modulate anything in the synth (including effects, but effects are global only) with one of the six operators.
- This is a frequency modulation matrix - you are modulating the frequency of anything you target (including targeting a filter) with the wave of whatever you are sending in.
- NOTE FOR THE 6-SLOT MOD MATRIX: The targets are labeled with the slot they are in (Slot A, Slot B, Slot C, Slot D). This correlates to the knobs left to right - by default, for the Wave operator type, this will affect Ratio, Detune, Phase (or PWM), and Fold. But it will transparently affect that knob, with whatever you have in that slot.

## Audio Matrix
- A 6 by 6 matrix letting you route audio.
- You can route audio from one operator into another with this, and you can control which operators are sending out to be heard.
- The default 'Init' state just has Operator 1 audible. You can check this when you open the synth up by opening up this page and looking on the far right.

## Effects
- Use any three of the effects that you want at the end of the chain. They always run in serial. You can target them in the extra modulation matrix

## Settings
A settings page for two things:
### Size
- Set the size of the plugin window from 50% to 200%. This doesn't work perfectly but it's something!
### Theme
Select your color theme from one of eight preset choices, or make your own with fice color presets! All pages will pick up the new colors and refresh.


## Other Features:
- You can sync an operator to DAW tempo and use it as an LFO. Do you want more LFOs? Why have LFOs when you can just have more operators??
- You can pass audio into an operator with the audio routing matrix, so you can use an operator as a filter. Do you want a filter? Why have filters when you can just have more operators??
- You can modulate any operator with any other operator.
- You can route a track into this synth, then have a midi track playing on the same track and just take the input, and use any of the effects in it on ... anything.

Pictures of the synth in use are provided in the pictures/ folder, and I've switched between themes for each picture so you can get an idea of the theme options. There are also build scripts in the main folder, and built vst3 and standalone builds for all three OS's in the release folder.
This is mostly AI-coded, but I am actively using the synth myself and have read over all of the code and think I understand it (though how much can one understand code they haven't written themselves?). Your mileage, as always, may vary.

Happy synthing!
