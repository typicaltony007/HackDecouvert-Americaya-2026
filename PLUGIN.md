# SynthMath

A beginner-friendly software synthesizer built with JUCE 9 and CMake. It builds as a VST3 plugin and as a Standalone app.

## Features

- **Starts as a plain sine wave.** A new instance plays a single sine oscillator, so you can build your sound up from the simplest starting point.
- **2 oscillators (A + B), 8-voice polyphony.** Each oscillator has a waveform, level, octave, semitone and fine-tune control. Oscillator B starts at level 0.
  - The waveforms are **Sine, Saw, Square, Triangle** and **Custom**. Saw and Square use PolyBLEP to reduce aliasing.
  - **Custom** plays one cycle of a math formula of `x`, where `x` goes from 0 to 2π. The formula box only appears when Custom is selected. Examples:
    - `sin(x) + 0.5*sin(2*x) + 0.25*sin(3*x)`
    - `tanh(4*sin(x))`
    - `pow(sin(x), 3) + 0.2*saw(3*x)`

    Available functions: `sin cos tan abs min max pow sqrt exp log tanh sign floor frac saw square tri noise`, plus the constants `pi` and `e`. Press Enter or **Apply** to hear it. The formula is saved with the plugin state.
- **Effects as a node chain.** The signal flows from upstream to downstream: **AUDIO IN → effect nodes → AUDIO OUT**.
  - Click **+** to add an effect. The menu shows a one-line description of each one.
  - **Drag** a node left or right to change the processing order. The audio follows the new order.
  - Click a node to edit it below the chain. Its dot switches it on/off, and **×** removes it.
  - The chain order is saved with your project.
  - Available effects:
    - Distortion (Soft Clip, Overdrive, Hard Clip, Tube, Foldback)
    - Bitcrush
    - Filter (low-pass / high-pass / band-pass)
    - 3-band EQ
    - Chorus
    - Compressor
    - Delay
    - Reverb
  - The EQ has a **frequency visualiser**. It shows each band's curve, the combined curve and a live spectrum of the output. Drag the **L / M / H** handles to set a band's frequency and gain. Scroll on **M** to change its Q, and double-click a handle to reset its gain.
- **Modulation screen** with 5 tabs. Each slot is an **Envelope** (ADSR) or an **LFO** (Sine, Triangle, Saw, Square, Sample & Hold). You can route it to Amp, Pitch, Osc A or B Pitch, Osc A or B Level, or Pan. Slot 1 defaults to *Envelope → Amp*.
- **Play from your computer keyboard.** The labels on the on-screen keyboard show which key plays which note:

  | Key  | A | W  | S | E  | D | F | T  | G | Y  | H | U  | J |
  |------|---|----|---|----|---|---|----|---|----|---|----|---|
  | Note | C | C# | D | D# | E | F | F# | G | G# | A | A# | B |

  All twelve notes are in octave 3: **A = C3**, which is MIDI note 60 (middle C), as labelled on the on-screen keyboard. To shift the row, change `ComputerKeys::baseNote` in `Source/UI/ComputerKeyboard.h`.
  - Typing in the formula box doesn't play notes. Press Enter or Escape, or click anywhere else, to go back to playing.
  - Inside a DAW, the plugin window needs keyboard focus, so click it first. Some DAWs keep certain keys for themselves. The Standalone app always works.
- **Clean, high-contrast UI.** It uses warm-black panels with light beige text. Knobs show their name, and switch to the current value while you hover or drag. Double-click a knob to reset it.

## Building with Visual Studio 2026

You need:
- Visual Studio 2026 with the **Desktop development with C++** workload. This includes CMake 4.2+, which is needed for the `Visual Studio 18 2026` generator.
- JUCE. By default the build uses the local checkout at `../../juce-9.0.5-windows/JUCE`, which is `D:\Hackathon\juce-9.0.5-windows\JUCE`. To use another copy, pass `-DJUCE_DIR=<path>`. If neither is found, CMake downloads JUCE 9.0.5 from GitHub.

### Option A: command line
Run this from a *Developer PowerShell for VS 2026*:
```
cmake --preset vs2026
cmake --build --preset vs2026-release
```
Or just run `build.bat` (or `build.bat Debug`).

### Option B: generate a .sln
```
cmake --preset vs2026
```
Then open `build/vs2026/SynthMath.sln`. `SynthMath_Standalone` is the startup project.

### Option C: open the folder in VS
In Visual Studio, use **File → Open → Folder**. VS reads `CMakePresets.json` automatically.

### Build output
- VST3: `build/vs2026/SynthMath_artefacts/<Config>/VST3/SynthMath.vst3`
- Standalone: `build/vs2026/SynthMath_artefacts/<Config>/Standalone/SynthMath.exe`

To install the VST3 automatically, configure with `-DSYNTH_COPY_PLUGIN_AFTER_BUILD=ON`. This copies the plugin to `C:\Program Files\Common Files\VST3` and needs an elevated prompt. You can also copy the `.vst3` folder there by hand.

If you installed an earlier build under another name (`Synthesizer Hackathon.vst3` or `SynthEasy.vst3`), delete it. It's the same plugin, so your DAW would otherwise list it twice.

## Project layout
```
CMakeLists.txt          JUCE plugin target (VST3 + Standalone, synth, MIDI in, stereo out)
CMakePresets.json       VS 2026 (and VS 2022 fallback) presets
Source/
  PluginProcessor.*     Audio processor: synth (audio in) -> effect chain -> volume (audio out)
  PluginEditor.*        Top-level UI layout, focus handling for computer-keyboard playing
  Parameters.*          All parameter IDs, choice lists, effect info and the APVTS layout
  DSP/
    Oscillator.h        PolyBLEP saw/square, sine, triangle, custom-wavetable oscillator
    CustomWaveTable.*   Formula -> wavetable (lock-free double buffer)
    ModSource.h         Per-voice envelope / LFO
    SynthVoice.*        Voice: 2 oscillators + 5 mod sources + routing
    EffectsChain.*      All 8 effects, processed in the user's chain order
    SpectrumTap.h       Lock-free output buffer feeding the analyser
  UI/
    SynthLookAndFeel.*  Warm, high-contrast theme: knobs, switches, tabs, menus, keyboard
    Controls.h          Parameter-bound Knob / Choice / PowerSwitch, Section panel
    OscillatorPanel.*   Oscillator UI + waveform preview + formula editor
    FxChainView.*       Node chain: add / reorder / bypass / remove effects
    EffectsPanel.*      Effects tab: node chain + controls of the selected effect
    EqVisualiser.*      EQ response curves + spectrum analyser + draggable band handles
    ModulationPanel.*   5-tab envelope/LFO screen
    ComputerKeyboard.h  A..J key-to-note input + labelled on-screen keyboard
Legacy_Projucer_Template/  The original Projucer GUI-app template (not used by the CMake build)
```
