# PyramidFuzz

PyramidFuzz is a JUCE-based macOS plugin project targeting AU/GarageBand first. The repository now contains a first working DSP core: modular fuzz stages, selectable clipping modes, a Muff-style tone approximation, and selectable 2x / 4x / 8x oversampling with 4x as the default.

## Current status

- JUCE + CMake plugin scaffold
- AU and Standalone targets
- `AudioProcessorValueTreeState` parameter layout
- First-pass DSP engine with modular stages
- 4x oversampling enabled by default, with 2x / 8x options exposed
- Basic editor with main controls wired to parameters
- Architecture and implementation plan docs

## Project structure

- `CMakeLists.txt`: top-level JUCE/CMake build
- `src/PluginProcessor.*`: plugin shell, bus layout, parameter state, DSP integration
- `src/PluginEditor.*`: initial GUI scaffold and parameter attachments
- `src/dsp/*`: DSP blocks, oversampling engine, and fuzz signal path
- `docs/architecture.md`: DSP architecture, oversampling placement, and refinement notes

## JUCE setup

The build supports either of these approaches:

1. Place JUCE as a `JUCE/` subdirectory in the repository.
2. Provide JUCE through CMake package discovery, for example with `-DCMAKE_PREFIX_PATH=/path/to/JUCE`.

JUCE is not currently vendored in this repository. The CMake entry point is already set up to support either approach cleanly.

## Add JUCE on macOS

Prerequisites:

1. Install full Xcode from the Mac App Store.
2. Ensure the active developer directory points at Xcode, not only Command Line Tools:

```bash
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
```

3. Confirm Xcode is active:

```bash
xcodebuild -version
```

### Option A: vendor JUCE inside the repo

1. From the project root, add JUCE as a sibling project directory named `JUCE`.
2. Example:

```bash
git clone https://github.com/juce-framework/JUCE.git JUCE
```

3. Confirm this file exists:

```bash
ls JUCE/CMakeLists.txt
```

4. Configure the project:

```bash
cmake -S . -B build -G Xcode -DCMAKE_BUILD_TYPE=Debug
```

5. Open the generated Xcode project:

```bash
open build/PyramidFuzz.xcodeproj
```

### Option B: use an existing JUCE checkout via `CMAKE_PREFIX_PATH`

1. Clone JUCE anywhere on your Mac, for example:

```bash
git clone https://github.com/juce-framework/JUCE.git ~/SDKs/JUCE
```

2. Configure PyramidFuzz against that checkout:

```bash
cmake -S . -B build -G Xcode -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=~/SDKs/JUCE
```

3. Open the Xcode project:

```bash
open build/PyramidFuzz.xcodeproj
```

### AU notes for macOS / GarageBand

1. Build the `PyramidFuzz_AU` target in Xcode.
2. If plugin validation or discovery is stale, restart GarageBand after the AU build finishes.
3. If needed, inspect AU registration with:

```bash
auval -a
```

4. GarageBand typically discovers user Audio Units from:

```text
~/Library/Audio/Plug-Ins/Components
```

## Configure

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/path/to/JUCE
```

If JUCE is vendored into `JUCE/`, `CMAKE_PREFIX_PATH` is not required.

## Build

```bash
cmake --build build
```

To generate an Xcode project explicitly on macOS:

```bash
cmake -S . -B build -G Xcode -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/path/to/JUCE
cmake --build build --config Debug
```

## Current DSP implementation

- `Input`: pre-gain trim before the fuzz path
- `Voicing`: crossfades between a tighter Hi input filter set and a fuller Lo input filter set
- `PreGainStage`: coupling-style filtering plus mild asymmetric saturation
- `ClippingStageA`: dense compressed fuzz stage with strong sustain contribution
- `ClippingStageB`: Off / Silicon / Germanium mode with distinct clip curves
- `Tone`: Big Muff-style dark/bright branch blend with a center-position mid scoop
- `High`: post-tone presence contour for bite without excessive fizz
- `Volume`: final recovery gain with a little output glue saturation

## Next steps

1. Tune the internal gain staging and filter corner frequencies by ear against target Muff references.
2. Improve tone-stack realism with a closer passive-network fit.
3. Add level metering and output calibration in the UI.
4. Validate AU behavior in GarageBand on macOS.
5. Add regression tests or offline response checks for the DSP blocks.
