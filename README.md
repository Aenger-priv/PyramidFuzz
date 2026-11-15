# Pyramid Fuzz

A JUCE guitar fuzz for macOS/GarageBand, inspired by a modified Ram's Head Big Muff. Builds as an Audio Unit and a standalone app. The signal path uses nominal circuit-guided gain, diode feedback and passive tone models, with oversampling. It is not a measured-pedal calibration or full transistor simulation.

## Controls

- **Input:** input trim in dB.
- **Sustain:** loaded interstage drive, on a 0–10 scale.
- **Tone:** dark to bright; displayed as a percentage.
- **High:** extends the passive tone network's upper response; interacts with Tone.
- **Volume:** output level in dB.
- **Voicing:** Hi gives more drive; Lo is cleaner and quieter.
- **Diode clipping:** Off opens only the second diode branch; the amplifier and first clipping stage remain active. Silicon is firmer; germanium uses asymmetric one/two-junction branches and clips lower. Natural level differences are preserved.
- **Oversampling:** 2x / 4x / 8x. Default 4x. Changes briefly fade the signal; host latency remains six samples.

Double-click a knob to reset, Shift-drag for fine adjustment, or click its value to type. Tab/arrows operate focused controls. Meters show peak dBFS with timed decay and a one-second overload hold. Input is metered before trim; output after Volume. Use the host bypass for the complete effect.

Use 4x for normal playing. Try 8x for exposed bright/high-drive sounds when CPU allows. The extreme 96 kHz/8x/stereo/32-frame combination remains CPU-heavy. Synthetic stress tests also retain measurable alias energy at 4x; see the [quality review](docs/processing-quality-review.md) for the actual results and limits.

## Build and run

Requires Xcode, CMake 3.22+, and the pinned JUCE submodule:

```bash
git submodule update --init --recursive
cmake -S . -B build -G Xcode
cmake --build build --config Release --target PyramidFuzz_Standalone PyramidFuzz_AU
open "build/PyramidFuzz_artefacts/Release/Standalone/Pyramid Fuzz.app"
```

Quit an existing standalone copy before reopening a rebuilt app. Release is recommended for playing; Debug remains available for development.

The AU is `build/PyramidFuzz_artefacts/Release/AU/Pyramid Fuzz.component`. Builds do not install it automatically. To use it in GarageBand, copy it to `~/Library/Audio/Plug-Ins/Components`, restart the host, then validate registration with:

```bash
auval -v aufx PhFz AnDr
```

The deployment floor is explicitly macOS 12.0. A universal build can be generated separately:

```bash
cmake -S . -B build/validated-release -G Xcode "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
cmake --build build/validated-release --config Release --target PyramidFuzz_Standalone PyramidFuzz_AU
```

Build compatibility and runtime validation are separate: see the delivery review for tested OS/architecture/host results. An installed JUCE CMake package is also supported when the submodule is absent, via `CMAKE_PREFIX_PATH`.

## Tests and evidence

```bash
cmake --build build --config Release --target PyramidFuzzDspTests PyramidFuzzProcessorTests PyramidFuzzMeasurements
ctest --test-dir build -C Release --output-on-failure
```

Six test groups cover DSP behavior, processor state/routing/chunking, meter timing and value formats, independent circuit references, tone/input responses and clipping feedback. Optional measurement tools capture stage levels, harmonics, decay, synthetic/DI audio, CPU and alias estimates. Large captures and original builds stay local under ignored `artifacts/`.

Existing sessions retain parameter values and use the revised sound. Earlier builds are preserved separately; no legacy sound model is embedded.

- [Revision plan](docs/implementation-review-plan.md)
- [Original baseline](docs/baseline-review.md), [tone](docs/tone-correction-review.md), [input/Sustain](docs/input-sustain-review.md), [clipping/recovery](docs/clipping-recovery-review.md)
- [Processing/quality](docs/processing-quality-review.md), [UI/playability](docs/ui-playability-review.md), [delivery validation](docs/delivery-validation.md)

## Layout

- `src/PluginProcessor.*`: host integration and parameters; `src/PluginEditor.*`: controls and artwork.
- `src/dsp/`: circuit blocks, quality paths, smoothing and meters.
- `tests/`, `tools/`: regression tests, independent references, captures and plots.
- `assets/generated/`: panel, knobs and app icon, with generation metadata. Normal builds use these local assets and need no generation service.
- `scripts/generate_pedal_art.py`: optional artwork helper; its default prompt is retained under `assets/prompts/`. It is outside the normal build.
- [Architecture](docs/architecture.md) and [schematic mapping](docs/schematic-analysis.md).
