# Oops! All Oscillators! (OAO)

This is a six-operator FM synthesizer where the operators do everything. It's open source, written in C++ using the JUCE framework and copious amounts of AI help.

## Operator types
### Wave: Sine, Triangle, Saw, Square, White Noise
- Options: Ratio, Detune, Phase, Fold. You can also tempo sync the operator to a DAW with the checkmark.
- Ratio goes down to 0.01 so you can have an oscillator slow enough to be used as an LFO. I may add another mode for this in the future.
- PLEASE NOTE: The gain for all operators is controlled in the Audio Matrix!

### Additive: 
- Options: Ratio, Tilt, Stretch, Odd/Even
- It's an actual additive oscillator in your FM oscillator! Additive is cool.

### Filter: Lowpass, Highpass, Bandpass, Comb, Granular.
- Options: Cutoff, Resonance, Keytrack, Feedback
- Granular options: Grain size, Damping, Scatter, Feedback

## Modulation Matrix
- A 6 by 6 matrix letting you modulate any operator by any other operator. On the right of this, there is a 6-slot mod matrix, letting you modulate anything in the synth (including effects) with one of the six operators.

## Audio Matrix
- A 6 by 6 matrix letting you route audio within the synths.
- You can route audio from one operator into another with this, and you can control which operators are sending out (by default you only hear operator 1).

## Effects
- Basic effects taken straight from default juce effects:
### Chorus
-Mix, Rate, and Depth.
### Delay
-Mix, Time, and Feedback,
### Reverb
-Mix and Room.

## General Features:
- You can sync an operator to DAW tempo and use it as an LFO. Do you want more LFOs? Why have LFOs when you can just have more oscillators??
- You can pass audio into an operator with the audio routing matrix, so you can use an operator as a filter. Do you want a filter? Why have filters when you can just have more operators?
- You can modulate any operator with any other operator.
- Three simple, global-only effects to soften up the FM; Delay, Reverb, and Chorus. If you want better effects, get your own lol.
- Preset support! ... It's in the form of writing and reading xml files, but it does work!
- Randomization of each page, if you really can't think of anything.
- An Init button to avoid the randomization and get back to a normal state when it becomes too much.
- A gain function to attempt to control the noisiness of FM (optimistic).
- A soft clipper on the end of the chain and at every step of the audio routing page.

Pictures of the synth in use are provided in the pictures/ folder. There are also build scripts in the main folder.
This is mostly AI-coded, but I'm actively using the synth myself! So. It will evolve as it evolves, but I am happy with it for now. If you want to expand it, please free to take the code and run with it - that's why it's here!! Or if you want you can open a bug report.
