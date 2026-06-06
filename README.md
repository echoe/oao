# Oops! All Operators! (OAO)

<img src=https://raw.githubusercontent.com/echoe/oao/refs/heads/main/pictures/operators.png width="450" height="300" />

This is a six-operator FM synthesizer where the operators do everything. It's open source, written in C++ using the JUCE framework and copious amounts of AI help.

Thanks to both Six Sines and to the Korg Opsix for some of the inspiration behind this synth :)

## Preset Bar

### Presets
There are three buttons to handle presets.
- Init: This initializes the patch to a single Sine wave, with Operator 1's Out set to 1.
- Save: This saves a patch as an .xml file.
- Load: This loads a patch from an .xml file.

### Randomization
There are three randomization buttons. These are after 'Init', because frequently after using them, you'll want to go back to an init state.
You can also go through and manually click on knobs to reset them to 'default'!
- R-Synth: This changes all synth variables. It also changes the outs on the Audio Matrix page.
- R-FM: This changes all modulation - all variables on the matrix.
- R-Route: This randomizes all audio matrix routing.

### Oversampling
This oversamples the main audio path to provide increased polyphony. Select between 1,2,4, and 8x.

### Poly
This picks the number of voices in the synth. This defauts to 8 but you can go from 1 (mono) to 32.

### Size
Change the size of the synth window, from 50% to 200%.

### Gain
If you're using randomization you may want to turn the gain down as a precaution! Don't go deaf!!! It's easy for things to get /too wild/. I've tried to moderate that and haven't made all audio from my computer break for a while, but this is a freeware VST made by one developer and AI, so, uh, no guarantees.


## Operator types
### Wave: Sine, Triangle, Saw, Square, Pulse, SquarePWM, White Noise, Pink Noise
- Options: Ratio, Detune, Phase, Fold. You can also tempo sync the operator to a DAW with the checkmark - that's why Ratio changes to Sync Rate there.
- Ratio: from 0.01 to 16, this controls the pitch of the waveform relative to the note you're playing. You, uh, need this for FM.
- Detune: You can slightly detune the note if you want, without moving the Ratio by tiny steps.
- Phase: Changes the phase of the waveform.
- Fold: Folds the waveform in on itself.

For SquarePWM and Pulse, instead of Phase there is a PWM option there. So you can modulate the PWM if you want!

Additional Notes:

- Ratio goes down to 0.01 so you can have a waveform slow enough to reasonably be used as an LFO. I may add another operator mode for this in the future if this isn't slow enough for some cases.
- If you really need PWM you should be able to modulate any of these wave operators targeting the 'Phase' field.
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
#### Comb
- Options: Cutoff, Resonance, Keytrack, Feedback
- This is modeled after the Comb effect on the Korg Opsix as described here: https://www.icemoonprison.com/blog/archives/579
- As such, you can use this for physical modeling.
#### Granular
- Options: Grain size, Damping, Scatter, Feedback
- Because there aren't enough knobs and I couldn't think of a reason you wouldn't want it keytracked, this filter forces keytracking to 100%.
#### Formant
- Options: Vowel, Nasal, Vowel Mod, Drive
- Note: Vowel Mod changes the amount of modulation coming in from the modulation matrix, if you are modulating it. So to hear it simply, you can self-modulate this operator, and then tweak the Vowel Mod, and it will change the sound then.
#### Tape
- Options: Wobble, Age, Saturation, Bias
- For Bias to be heard saturation must be on at least a little bit, the bias biases the saturation.
#### Bitcrush
- Options: Bits (1-16), Rate, Jitter, Noise (added white noise sample)
- You have to have mix set twice to use this! Care!
#### Allpass Delay
- Options: Time, Feedback, Diffusion, Damping
- Time: 0.0 = 10ms, 1.0 = 1000ms
- Damping: one-pole LP in feedback path
- Diffusion 0.0 = clean echo, 1.0 = heavily smeared
- Feedback - adding feedback before return
#### Allpass Reverb
- Options: Size, Decay, Diffusion, Damping
- Size: 0.0 = small room (20ms), 1.0 = large hall (500ms)
- DECAY — feedback amount controls tail length
- POST-DIFFUSION — smear the output for density
- DAMPING in the tank feedback path
#### Compressor
- Options: Threshold, Ratio, Attack, Release
#### Varispeed
- Options: Speed, Acceleration, Depth, Mode
-Speed controls the amount that the speed should vary by.
-Acceleration is how fast the effect changes the speed of the incoming audio.
-Depth controls how much the speed varies around the target speed.
- Mode is 'Direction'. If it's under .45, it's slowing down. if it's over .55, it's speeding up. if it's in the middle, it's not doing either.
#### Scatter
- Options: Pattern, Size, Speed, Depth
#### Ring Mod
- Options: Frequency, Shape, Depth, Feedback
#### Chorus
- Options: Rate, Depth, Spread, Voices
#### Phaser
- Options: Rate, Depth, Stages, Feedback
#### Distortion
- Options: Drive, Flavor, Tone, Degradation
#### Harmonic Resonator
- Options: Root, Scale, Brightness, Depth
Note: The scales are major, minor, pentatonic, whole, and chromatic.

### More?
The process to add more effects is very easy:
-Add necessary state variables to SynthFilter.h's private section, and any variables it needs to reset to the reset() call.
-Add a processing function to SynthFilter.h that actually takes input and processes the input into the effect result.
-Update Constants.h to add the effect in the FilterChoices array and in the case settings.
-In FMOperator.h, process the knobs to be correct given their start positions (which are from the default Wave 'Operators' page, these knobs are just relabeled with every change), and then pass them to the processing function.
-In the processBlock effects loop in PluginProcessor.cpp, so the Effects page can work, add the case there. This is pretty much the same as FMOperator.h but with some differences that are pretty self-evident looking at the 16 (!) current examples.

### Ext. In
It's an external in! You can route external sound into the synth if you want now, and it will go through the modulation matrix like everything else (and will be affected by the filters). It is only open if midi data is going into the plugin though.
-Options: Gain, Tilt (quick EQ), Mod (controls how much the external in is affected by audio modulation), Fold(just like the normal Wave oscillator)

## Modulation Matrix
- A 6 by 6 matrix letting you modulate any operator with any other operator. On the right of this, there is a 6-slot mod matrix, letting you modulate anything in the synth (including effects, but effects are global only) with one of the six operators.
- This is a frequency modulation matrix - you are modulating the frequency of anything you target (including targeting a filter) with the wave of whatever you are sending in.
- NOTE FOR THE 6-SLOT MOD MATRIX: The targets are labeled with the name of the initial knobs. However, they should affect whatever you have enabled within that Operator.
- - So for instance, if you affect Operator 2's "Ratio" with Operator 6, and Operator 2 is set to a Comb Filter, you will actually modulate its Grain Size. ... In theory. That's how it's set up.

## Audio Matrix
- A 6 by 6 matrix letting you route audio.
- You can route audio from one operator into another with this, and you can control which operators are sending out to be heard.
- The default 'Init' state just has Operator 1 audible. You can check this when you open the synth up by opening up this page and looking on the far right.

## Effects
- Use any three of the filters that you want! They always run in serial but you can order them in any way so it doesn't really matter. You can also target them in the extra modulation matrix :)

## Other Features:
- You can sync an operator to DAW tempo and use it as an LFO. Do you want more LFOs? Why have LFOs when you can just have more operators??
- You can pass audio into an operator with the audio routing matrix, so you can use an operator as a filter. Do you want a filter? Why have filters when you can just have more operators??
- You can modulate any operator with any other operator.
- Three simple, global-only effects to soften up the FM; Delay, Reverb, and Chorus. If you want better effects, that's what effects in a DAW are for! The effects are not the point!
- Preset support! ... It's in the form of writing and reading xml files, but it does work!
- Randomization of each page, if you really can't think of anything.
- An Init button to avoid the randomization and get back to a normal state when it becomes too much.
- A gain function to attempt to control the noisiness of FM (optimistic).
- A soft clipper on the end of the chain and at every step of the audio routing page.
- Full color theming with five (wow) zones to select and eight preset themes

# In progress
Text scaling. I'm working on it, the buttons also do not scale with plugin size

Pictures of the synth in use are provided in the pictures/ folder. There are also build scripts in the main folder.
This is mostly AI-coded, but I am actively using the synth myself and have read over all of the code and think I understand it (though how much can one understand code they haven't written themselves?). Your mileage, as always, may vary.

Happy synthing!
