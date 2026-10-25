# Pyramid Fuzz

Pyramid Fuzz is a JUCE guitar fuzz for macOS, inspired by a modified Ram's Head Big Muff circuit. It builds as an Audio Unit for hosts such as GarageBand and as a standalone app. The DSP uses schematic-guided gain and clipping stages and a passive tone-network model; it is not a full transistor-level simulation.

## Controls

- **Input** adjusts the signal level entering the fuzz.
- **Sustain** adjusts the gain and compression of the fuzz stages.
- **Tone** sweeps the passive tone network from darker to brighter; **High** adjusts its high-frequency branch.
- **Volume** sets the output level.
- **Voicing** selects the tighter Hi input or the fuller, lower-drive Lo input.
- **Clipping** selects Off, Silicon, or Germanium for the second clipping stage. Off retains the gain stage with a more open response.
- **Oversampling** selects 2x, 4x, or 8x processing of the nonlinear stages. The default is 4x. Changing it briefly fades the signal while the processing is reset.

The editor also displays input and output levels. The plugin supports mono and stereo input/output, including mono input to stereo output.

## Build and run on macOS

Install Xcode and CMake 3.22 or newer. Clone the repository with its JUCE submodule, or initialize it in an existing checkout:

```bash
git submodule update --init --recursive
```

From the project directory, configure and build the standalone app and Audio Unit:

```bash
cmake -S . -B build -G Xcode -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --target PyramidFuzz_Standalone PyramidFuzz_AU
```

Launch the standalone app:

```bash
open "build/PyramidFuzz_artefacts/Debug/Standalone/Pyramid Fuzz.app"
```

The Audio Unit is built at `build/PyramidFuzz_artefacts/Debug/AU/Pyramid Fuzz.component`. The build does not install it automatically. To use it in GarageBand, copy the component to `~/Library/Audio/Plug-Ins/Components`, then restart GarageBand if needed. `auval -a` lists registered Audio Units.

The build also supports an installed JUCE CMake package when the `JUCE/` submodule is absent; point `CMAKE_PREFIX_PATH` at the package's installation prefix.

## Tests

CMake testing is enabled by default. Build and run the offline DSP checks with:

```bash
cmake --build build --config Debug --target PyramidFuzzDspTests
ctest --test-dir build -C Debug --output-on-failure
```

The checks cover clipping-knee continuity, finite output, stereo consistency, and signal flow at 44.1, 48, and 96 kHz across the clipping and oversampling modes. Listening against a reference pedal and validation in GarageBand remain useful steps for assessing sound and host behavior.

## Project layout

- `src/PluginProcessor.*` and `src/PluginEditor.*`: plugin integration, parameters, and interface.
- `src/dsp/`: fuzz stages, tone network, and oversampling.
- `tests/`: offline DSP checks.
- [docs/architecture.md](docs/architecture.md): signal path and implementation details.
- [docs/schematic-analysis.md](docs/schematic-analysis.md): notes on the source schematic.
