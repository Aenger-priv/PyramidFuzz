# Step 1: preserved baseline and reference measurements

Captured 2026-11-15 on `codex/schematic-baseline`. Production DSP, parameters,
artwork, and JUCE pin are unchanged from `b98a3becd07118bd6d0b7995c1a5a6e5c293a743`.
Step 1 is implemented; review the baseline and proposed limits below before
approving Step 2. No tone correction or other sonic revision has been applied.

This is the historical Step 1 record. The subsequent approved correction is
documented in [the Step 2 review](tone-correction-review.md); baseline archives
and measurements remain untouched.

## Saved-session decision

The user chose to use the revised sound in existing sessions and retain the
original build separately. A legacy DSP mode is therefore not planned. Keep
parameter identifiers, ranges, and choice ordering stable; document intentional
sound changes at each later step. Identical parameter values will not promise
identical sound after those revisions.

## Preserved original

Local bundle: `artifacts/baseline-b98a3be-20261115/` (ignored by Git).

- `preserved/project-source.tar.gz`: original project at `b98a3be`.
- `preserved/juce-source.tar.gz`: JUCE at `501c07674e1ad693085a7e7c398f205c2677f5da`.
- `preserved/au.zip` and `preserved/standalone.zip`: original Debug builds,
  archived before the measurement target was added. Nothing was installed.
- `preserved/preservation.json`: archive SHA-256 checksums and source identities.
- `measurement-source.tar.gz`: working measurement sources used for this run.
- `measurement-executable.zip`: the measurement binary, retained for exact replay.
- `manifest.json`: toolchain, executable/source hashes, exact command, protocol,
  source state, and raw/reference artifact checksums. It records the dirty working
  tree honestly; the archived measurement sources permit reconstruction.
- `repeatability.json`, `io-validation.json`: verification receipts.
- `validation/`: build/test logs and plotting package versions;
  `delivery-index.json` hashes the completed delivery, including figures.
- `baseline-overview.png` / `.svg`: standalone figures from the captured CSVs.

Restore the original sources into a separate directory by extracting the project
archive and extracting the JUCE archive into its `JUCE/` directory. Extract the
original binaries with `ditto -x -k preserved/standalone.zip DESTINATION` or the AU
equivalent. Keep the original AU outside the host's scan directory until choosing
which build to install: both versions have the same plugin identity.

The original binaries are Debug; the offline measurement executable is Release.
Repeatability claims below refer to the same executable and platform, not exact
floating-point equivalence across compilers, architectures, or build settings.

## Circuit variant and modeling contract

The repository image is the circuit reference. C8 remains **22 nF**, following
the existing recorded clarification, despite its contradictory 2.2 uF caption.
This selected variant must remain explicit in subsequent comparisons.

| Circuit role | Drawing references | Intended model responsibility |
| --- | --- | --- |
| Input conditioning | C1 10 uF; R2 39 kOhm / R27 390 kOhm; R14 100 kOhm | Coupling and attenuation into a documented Q4 input impedance; Hi drives harder than Lo. |
| Fixed preamp | Q4; R13 10 kOhm; R22 1 kOhm; R9 470 kOhm; C10 470 pF | Fixed gain, feedback-shaped bandwidth, and headroom; not three separate Sustain gain controls. |
| Sustain divider | C4 470 nF; R24 100 kOhm; R23 1 kOhm; C5 47 nF; R19 6.2 kOhm | Interstage attenuation/loading before Q3; retain the lower-leg resistance at minimum. |
| First clipping stage | Q3; R18 10 kOhm; R21 100 Ohm; R17 470 kOhm; C12 470 pF; C6 47 nF; D3/D4 | Bounded silicon feedback-clipping approximation with corresponding bandwidth/coupling. |
| Selectable clipping | Q2; R11 10 kOhm; R10 100 Ohm; R15 470 kOhm; C11 470 pF; C7 47 nF; D1/D2/D5/D6/D7 | Gain stage remains in Off; silicon versus asymmetric one/two-diode germanium behavior. |
| Passive tone | C9 10 nF; R5 470 kOhm; R8 25 kOhm; C8 22 nF; R25 250 kOhm; C3 47 nF | C9-series/R5-shunt high-pass arm and R8-series/C8-shunt low-pass arm; loaded pot and output coupling. |
| Recovery/output | Q1; R6 10 kOhm; R4 2.2 kOhm; R7 470 kOhm; R3 100 kOhm; C2 10 uF; R26 100 kOhm | Recovery gain/headroom, output coupling, and level control. |

The independent reference is [a component netlist](../tools/reference/tone-network.json)
solved by [a complex AC solver](../tools/tone_reference.py). It imports no plugin
DSP and contains no audio-rate transistor model. Tests check it against closed-form
resistor dividers and RC low/high-pass responses, including bilinear frequency
mapping, and check passive bounds and intended control direction.

Reference assumptions for this topology comparison:

- Source resistance: **10 kOhm**; recovery load: **220 kOhm**, holding the current
  DSP assumptions constant to isolate the wiring error. These are Thevenin/input
  approximations, not measured transistor impedances. In particular, 220 kOhm is
  not the drawn R3 value; recovery input loading needs derivation before claiming
  a fitted circuit model. `loading-sensitivity.csv` varies source/load assumptions.
- Tone pot: 250 kOhm; High rheostat: 0–25 kOhm. Numerical floors are 30 Ohm at
  Tone end stops and 20 Ohm for High, matching the current finite end stops.
- Intended UI: Tone increases dark-to-bright; High increases upper-frequency
  extension by reducing series R8. Reference CSV includes both intended UI mapping
  and existing pot position, so direction changes cannot hide wiring differences.
- Continuous AC response and trapezoidal-equivalent AC are stored separately.
  The latter uses `s = j * 2 * Fs * tan(pi * f / Fs)` for comparisons to the
  discrete tone solver; near-Nyquist analog warping is not treated as a coding bug.
- DSP samples remain dimensionless. For future block derivation use a provisional
  convention of **one DSP unit = 1 V AC about bias** (0 dBFS peak = 1 V peak).
  This is a modeling coordinate only, not an interface or pedal calibration, and
  it changes no DSP constants. Physical drive/headroom claims await calibration.

## Protocol and measured baseline

Host defaults are Input 0 dB, Sustain 6.8/10, Tone .34, High .22, Volume -11 dB,
Lo, Silicon, 4x. These deliberately follow the host parameter layout, whose
defaults differ from the standalone `FuzzParameters` struct defaults.

- Tone: 75 impulse responses across 44.1/48/96 kHz and a 5x5 Tone/High grid.
  Each uses a 0.1-amplitude impulse, 65,536 samples, and measured FFT-bin frequencies.
  The CSV has 7,500 response points. The final 1,024 samples check settling.
- Gain/stages: 117 cases; the main 48 kHz/4x matrix covers both voicings, all modes,
  Sustain 0/.68/1, and input peaks -48/-36/-24/-12/-6 dBFS. A further 27 cases
  cover all sample rates, qualities, and modes at Lo/.68/-24 dBFS.
- Each sine case settles for 8,192 samples, then measures 8,192 samples. The
  fundamental is bin 19 (111.328125 Hz at 48 kHz). Harmonics 1–32 use coherent
  rectangular-window bins; this is not an alias-energy or noise measurement.
- Eight taps record RMS, peak, signed mean/DC estimate, and sample count: dry,
  voiced input, preamp, both clipping stages, downsampled signal, tone, output.
  Nonlinear tap counts use the oversampled rate. A fixed-parameter adapter exposes
  the production stages; its final samples must agree with `FuzzEngine` within
  1e-6 on every render. Maximum observed error was **0**.
- Listening material: one deterministic synthetic pluck sequence at -18 dBFS
  peak, rendered into 12 mode/voicing/quality combinations, plus its dry file.
  Files are unnormalized float32 WAVs. A 20 ms fade ends the dry signal at 4 s,
  followed by 1 s of silence; output decay is recorded in 50 ms windows.
- No actual guitar DI was available. Optional `--di` imports mono/stereo WAV/AIFF
  without resampling or normalization, retains source rate, and appends 1 s of
  silence. A stereo PCM fixture verified source channels, level, sample rate and
  silence retention; that fixture is not presented as a guitar performance.

At Tone=1, 48 kHz, and the FFT bin at **999.7559 Hz**, raising High from 0 to 1
changes gain from **-3.5920 dB to -55.9419 dB**, a **52.3499 dB drop**. The
reference comparison confirms a topology discrepancy, independently of listening.

For the -24 dBFS-peak sine, Lo/4x/default controls, output RMS is -24.699 dBFS
(Off), -22.417 dBFS (Silicon), and -23.528 dBFS (Germanium). These are baseline
observations, not proposed loudness-equalization targets.

## Validation and proposed next gate

Completed:

- Debug AU, standalone, and existing DSP tests build successfully.
- CTest passes 2/2 entries: existing DSP checks and the independent reference's
  three analytic/behavioral test methods. Upstream SDK deprecation warnings remain.
- A second full capture produced **bit-identical copies of all 19 raw files**
  on the same machine/executable, including all 13 WAVs.
- Archive integrity and SHA-256 checks pass. The CLI refuses to overwrite existing
  measurements. Production source/assets/JUCE diffs against `b98a3be` are empty.
- The standalone plots were rendered and visually inspected.

Proposed Step 2 acceptance limits, for review before implementing that step:

1. Corrected tone response within **0.1 dB** of the independent trapezoidal AC
   reference over **30 Hz–10 kHz** across the 75 grid settings. Compare phase
   within **1 degree** where reference gain exceeds -80 dB. Rebaseline the
   reference explicitly if approved source/load assumptions change.
2. Include endpoints and explicit dark-to-bright/High-direction regressions.
   Preserve legitimate passive loss; do not impose a flat loudness response.
3. Finite output and settled impulse tail below **1e-6** for the documented
   0.1 impulse. Check smooth control sweeps separately; a stationary frequency
   match does not approve automation behavior.
4. Compare the same dry performance through original and corrected versions,
   both at raw output levels and with recorded listening-only gain compensation.
   A real DI listening review remains pending; synthetic plots cannot approve feel.

CPU, aliasing, hardware matching, live automation, and GarageBand session behavior
are not approved by this capture. They remain in the later planned steps.

## Reproduce

```bash
cmake -S . -B build -G Xcode -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Release --target PyramidFuzzMeasurements
python3 tools/capture_measurements.py \
  --executable build/PyramidFuzzMeasurements_artefacts/Release/PyramidFuzzMeasurements \
  --output artifacts/NEW_CAPTURE \
  --baseline-ref b98a3be
# Add --di /absolute/path/to/dry.wav for a real performance.
python3 -m venv build/measurements-venv
build/measurements-venv/bin/python -m pip install -r tools/requirements-measurements.txt
build/measurements-venv/bin/python tools/plot_measurements.py artifacts/NEW_CAPTURE
ctest --test-dir build -C Debug --output-on-failure
```

For later intentional DSP revisions, omit `--baseline-ref b98a3be`; the manifest
still records working sources and hashes. Use a new directory for each capture.
Recreating the original baseline requires restoring its preserved source and
measurement snapshot; the baseline guard deliberately rejects corrected DSP.
Preservation archives are made separately; the capture command does not archive
or install host binaries. Large binaries/renders remain local, outside Git.
