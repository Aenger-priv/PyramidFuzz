# Step 2: passive tone-network correction

Implemented 2026-11-15 following approval of Step 2. Numerical validation passes;
the user also reports good sound and smooth control sweeps with a Les Paul with
humbuckers through the corrected standalone. Step 3 has not started. The
[original baseline](baseline-review.md) remains preserved.

## Practical change

The low-pass arm now has **R8 in series from the collector and C8 to ground**.
Previously C8 was in series and R8 went to ground, creating a second high-pass
arm and an unintended near-mute setting. C8's capacitor history now follows its
voltage to ground. The C9/R5 high-pass arm and C3 output coupling remain present.

Tone=0 selects the darker arm; Tone=1 selects the brighter arm. High reduces R8
to raise the low-pass corner. This is a loaded passive network: High's effect
depends on Tone and is not a universal treble boost at every frequency/setting.

Only `src/dsp/ToneStack.h` changes production DSP. Input/preamp, Sustain,
clipping, oversampling, output-stage code, parameter defaults/IDs/ranges/order,
artwork, and UI are unchanged. Existing saved settings intentionally sound
different, following the user's decision to retain the original build separately
instead of adding a legacy engine. There is no automatic loudness compensation.

## Retained circuit assumptions

C8 stays 22 nF; Tone is 250 kOhm; R8 is 0–25 kOhm with the existing 20 Ohm
numerical floor; Tone end stops retain their 30 Ohm floor. Source resistance
stays 10 kOhm and recovery load stays 220 kOhm. The independent Step 1 netlist
and AC solver are unchanged.

Keeping the loading assumptions isolates topology and mapping from a simultaneous
gain recalibration. They are not measured transistor impedances: in particular,
220 kOhm is not the schematic's 100 kOhm R3. Revisit recovery loading alongside
the surrounding-stage calibration; this change does not establish hardware fidelity.

## Numerical evidence

Local capture: `artifacts/tone-step2-20261115/` (ignored by Git). The full capture
repeats the Step 1 protocol: 75 tone curves / 7,500 points, 117 gain cases, eight
stage taps, harmonics and 12 synthetic audio renders. Capture manifests record
the toolchain, executable and source hashes. The fixed-control tap adapter still
matches production `FuzzEngine` exactly in these runs.

| Check | Acceptance limit | Measured |
| --- | --- | --- |
| Magnitude error, 30 Hz–10 kHz, 75 grid settings | <=0.1 dB | **0.00272 dB** maximum |
| Wrapped phase error in that band where reference >-80 dB | <=1 degree | **0.00880 degrees** maximum |
| Final 1,024 samples of the 0.1-amplitude impulse | <1e-6 peak | **1.76e-20** maximum |
| Independent reference comparison points in acceptance band | All measured points in band | **6,300** |
| Identical-channel difference during tone-control sweeps | <=1e-6 | **0** |

The comparison uses the intended dark-to-bright UI mapping and the reference's
trapezoidal frequency mapping. The original baseline fails the same checker,
with a maximum magnitude mismatch of 69.27 dB in the acceptance band.

At Tone=1, 48 kHz, and the measured 999.7559 Hz bin:

| High | Original gain | Corrected gain |
| --- | --- | --- |
| 0 | -3.592 dB | -3.622 dB |
| 1 | -55.942 dB | -5.004 dB |

The former 52.35 dB drop during that High sweep is now 1.38 dB. The retained
small change follows passive loading; no gain correction masks it.

All six taps before Tone are identical to the original baseline in all 117 gain
cases, confirming the change starts in the tone block. The output-stage input
and resulting output naturally change with the corrected response.

## Regression and dynamic checks

- C++ regressions check dark-to-bright spectral tilt, High's extension at the
  dark endpoint, and retained signal at the former near-mute corner, at all three
  sample rates.
- A CTest integration check measures the production tone solver and compares it
  against the independent Python AC reference. `--tone-only` avoids generating
  the full gain/audio matrix during this test.
- Tone-only dynamic checks use a <=0.1-amplitude 110 Hz + 5 kHz input, block-boundary
  targets, the engine's 30 ms smoothing convention, both channels, and control
  sweeps through endpoints/intermediate settings at 44.1/48/96 kHz.
- Conservative dynamic safety guards were set at 0.15 peak and 0.05 maximum
  adjacent-sample change. Observed maxima were 0.06890 and 0.01440 respectively;
  final-tail peak was below 2.20e-20 after the input faded to silence.
- Debug AU, standalone and all test targets build successfully; CTest passes
  **3/3** entries. These checks do not establish inaudible automation or complete
  GarageBand behavior; live host validation remains a later gate.

## Listening review

`comparison/` contains original/corrected pairs for Off, Silicon and Germanium,
using Lo, 4x and the same default controls: Input 0 dB, Sustain 6.8, Tone .34,
High .22, Volume -11 dB. Both versions use the identical synthetic dry file.

- `*-raw.wav`: copied unnormalized float captures, preserving natural level changes.
- `*-rms-matched.wav`: separate PCM24 listening exports matched over the first
  four seconds, attenuating the louder version only. Matching is not in the DSP.
- `listening-settings.json`: source hashes and exact comparison gains. Corrected
  exports were attenuated by 0.701 dB (Off), 0.522 dB (Silicon), and 0.554 dB
  (Germanium). Exported RMS differences are below 0.0001 dB; no file clips.
- `tone-correction.png` / `.svg`: original-versus-corrected endpoint and corrected
  Tone sweep against the independent reference. The plot was visually inspected.

The examples use default controls, not a recording of someone sweeping the knobs.
They are synthetic plucks, not real guitar or a measured pedal. A captured real
DI through both versions would make listening comparisons reproducible; none
has been captured here. No subjective improvement is claimed from plots alone.

Initial user listening feedback on the supplied comparison: the corrected version
sounds smoother and less distorted. The measured clipping-stage statistics are
unchanged; the downstream tone response reshapes their harmonics.

The user subsequently tested the corrected standalone with an actual Les Paul
with humbuckers and reported that it sounds good and the knobs sweep nicely.
This is a positive live-guitar check on that setup, not a recorded DI A/B or a
hardware-pedal comparison. Retain the Step 2 build as the comparison point for
later changes. Approval to begin Step 3 is still separate.

## Reproduce and inspect

```bash
cmake --build build --config Debug --target \
  PyramidFuzzDspTests PyramidFuzzMeasurements PyramidFuzz_AU PyramidFuzz_Standalone
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release --target PyramidFuzzMeasurements
python3 tools/capture_measurements.py \
  --executable build/PyramidFuzzMeasurements_artefacts/Release/PyramidFuzzMeasurements \
  --output artifacts/NEW_TONE_CAPTURE
python3 tools/validate_tone_capture.py \
  --csv artifacts/NEW_TONE_CAPTURE/raw/tone.csv \
  --report artifacts/NEW_TONE_CAPTURE/tone-validation.json
build/measurements-venv/bin/python tools/compare_tone_revision.py \
  --baseline artifacts/baseline-b98a3be-20261115 \
  --revised artifacts/NEW_TONE_CAPTURE
```

The plotting environment is described in the baseline review. Use a fresh capture
directory; tools refuse to overwrite prior results. `scope-and-audio-validation.json`
records the upstream equality and listening-export checks. `delivery-index.json`
hashes the completed local delivery. Host binaries have not been installed or
uploaded, and the revision has not been pushed.
