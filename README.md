# StereoHarasser — vintage degrader + reverb (VST3 / AU)

Developer: **blackghanistan sound solutions** · Framework: **JUCE 8 (C++17)** · 64-bit only

## Build

```bash
git init && git submodule add https://github.com/juce-framework/JUCE.git JUCE
git -C JUCE checkout 8.0.8            # any 8.0.x tag

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

* macOS: `StereoHarasser.vst3` and `StereoHarasser.component` (AU, universal arm64 + x86_64) are
  copied to `~/Library/Audio/Plug-Ins/` after the build.
* Windows: the build produces the bundle folder
  `build\blg_artefacts\Release\VST3\StereoHarasser.vst3` (x64). Copy the whole
  `StereoHarasser.vst3` folder to `C:\Program Files\Common Files\VST3\` (or configure with
  `-DBLG_COPY_AFTER_BUILD=ON` in an administrator shell to do it automatically). Requires Visual
  Studio 2022 with the "Desktop development with C++" workload: `cmake -B build -A x64`
  (CMake auto-detects the installed Visual Studio version).

### Build without installing anything (GitHub Actions)

Push this folder to a GitHub repository. `.github/workflows/build.yml` builds the plugin for
Windows x64 on every push (or run it manually from the Actions tab). Download the
`blg-vst3-windows-x64` artifact from the finished run — it contains the `StereoHarasser.vst3`
folder.

### FL Studio

Copy `StereoHarasser.vst3` to `C:\Program Files\Common Files\VST3\`, then in FL Studio open
Options → Manage plugins → "Find installed plugins" (with "Verify plugins" on). StereoHarasser
appears in the plugin database as an effect; load it in a mixer insert slot.

## Validate

```bash
auval -v aufx Blg1 Bkgh                                          # macOS, AU
pluginval --strictness-level 5 build/blg_artefacts/Release/VST3/StereoHarasser.vst3
```

## Signal chain

`Turbo → Sample rate + Bit depth → reconstruction low-pass → Tilt → Spectral feedback → Reverb (pre-delay + Freeverb) → Stereo (M/S)`

| Control | ID | Range | What it does |
|---|---|---|---|
| Sample rate | `sampleRate` | 1 – 48 kHz | sample & hold down-sampling + low-pass at 0.45 × rate |
| Bit depth | `bits` | 8 / 12 / 16 / 24 | quantiser (24 = off) |
| Turbo | `turbo` | 0 – 100 % | tanh drive up to +23 dB, slight bias (even harmonics), auto-level, DC blocker |
| Tilt | `tilt` | ±12 dB | spectral tilt around 650 Hz (+ = brighter, − = warmer/bassier) |
| Reverb Dry | `revDry` | 0 – 100 % | equal-power dry / reverb crossfade (100 % = dry only) |
| Reverb size | `revSize` | 0 – 100 % | room size |
| Width | `revWidth` | 0 – 100 % | stereo spread of the reverb tail |
| Depth | `revDepth` | 0 – 100 % | distance: pre-delay 0 – 80 ms + darker damping |
| Stereo | `stereo` | 0 – 200 % | mid/side width of the whole output (100 % = neutral) |
| Feedback Amount | `fbAmount` | 0 – 100 % | how much of the previous frame's output spectrum is fed back into the next one |
| Feedback Tilt | `fbTilt` | −1 … +1 | leans the feedback towards the low (−) or high (+) end of the spectrum (±12 dB) |
| Feedback Shift | `fbShift` | −8 … +8 bins | moves the fed-back spectrum sideways by this many bins on every pass, so repeats spiral down (−) or up (+) |

## Presets

Factory presets are compiled in (`PresetManager.cpp`), all built around stereo-width treatments
for bass:

| Category | Presets |
|---|---|
| Bass Mono | Sub Anchor, Mono Punch, Tight Low End — narrow/mono-focused low end |
| Bass Wide | Wide Sub, Stereo Swell, 808 Spread, Bass Bloom — wide stereo bass |
| Bass Vintage | Analog Warmth, Tape Bass, Cassette Low End, Lo-Fi Sub — degraded/warm bass |
| Bass Space | Bass Hall, Deep Chamber, Sub Room — bass with a sense of room |
| Bass Feedback | Sub Smear, Ghost Bass — bass run through the spectral feedback stage |

User presets are XML files `<Category>/<Name>.blgpreset` in:

* macOS `~/Library/Audio/Presets/blackghanistan/blg/`
* Windows `%APPDATA%\blackghanistan\blg\`
* Linux `~/.config/blackghanistan/blg/`

## Real-time safety

* `processBlock` allocates nothing; all buffers are created in `prepareToPlay`.
* Parameters are read through atomics (`getRawParameterValue`) and smoothed per sample.
* The spectral feedback stage works on 1024-sample frames, so the plugin reports a fixed latency of
  1023 samples (about 21 ms at 48 kHz) to the host. It is the same whatever the settings, so the
  host only has to compensate once; with Amount at 0 the stage is a transparent pure delay.
* All file I/O and dialogs run on the message thread only.

## Layout & visual style

The UI is drawn on a fixed 960 × 600 canvas (opens 20% larger than that base design size by
default) and scaled with a component transform, so the window can be resized freely
(640 – 1920 px wide) with the 8:5 aspect ratio locked. The window width is stored in the plugin
state.

The look is a dark horror/gore finish: a grungy near-black backdrop with grime streaks and
wobbling blood drips (`Source/UI/BackPanel.h`), and bone-coloured knobs whose pointer is a
faceted, 3D-shaded spike dripping blood tendrils rather than a plain line (`Source/UI/Theme.h`).
There is no logo tile in the top-right corner; the wordmark text owns that corner instead.

The bottom row has three panels:

* **Feedback** (bottom-left, `Source/UI/FeedbackPanel.h` plus the `Fader` widget in
  `Source/UI/Widgets.h`) is modelled on the Spectral Effects panel of the MAIM plugin: a
  **Tilt** fader (vertical), a **Shift** fader (horizontal), an **Amount** knob, and a live
  before/after spectrum graph in the middle — the bone-coloured line is the signal entering the
  spectral stage, the blood-red fill is what leaves it. The DSP is `Source/DSP/SpectralFeedback.h`:
  an overlapping-FFT stage (1024-point, 75 % overlap) in which every frame's spectrum is mixed with
  the previous frame's output spectrum, optionally tilted and bin-shifted. A one-sided energy
  limiter keeps the wet level within about 3x the input's, so high Amount settings cannot run away
  in volume, while tails still ring out freely after the input stops. MAIM's own spectral section
  is built on its MP3 encoder internals, which can't be reproduced literally, so this is an
  equivalent spectral-domain feedback rather than a copy.
* **Preset browser** (bottom-centre) — category / preset lists.
* **Output** (bottom-right, `Source/UI/OutputWaveScene.h`) draws the plugin's actual output
  waveform as a crisp line over a dark, slowly churning, bubbling blood backdrop; the backdrop is
  kept low-contrast so the real waveform trace stays the clear focus. It has no controls.

The Output panel and the Feedback panel's graph read from lock-free ring buffers
(`Source/DSP/ScopeBuffer.h`) on `BlgAudioProcessor`: `outputScope` (the final signal),
`spectralInScope` and `spectralOutScope` (either side of the spectral stage). `processBlock` pushes
samples into them (real-time safe — no locks, no allocation), and each display's
`juce::Timer`-driven repaint reads the latest window from the message thread; the graph's FFT runs
there too, never on the audio thread.

## License note

JUCE is dual-licensed (AGPLv3 / commercial). Closed-source distribution of StereoHarasser requires
a JUCE commercial licence.
