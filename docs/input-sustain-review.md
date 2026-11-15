# Step 3: input loading, fixed preamp and loaded Sustain

Implemented 2026-11-15 following approval of Step 3. Circuit-consistency and
safety checks pass. Live-guitar acceptance of this revision remains pending;
the positively reviewed [Step 2 build](tone-correction-review.md) is preserved.
Step 4's clipping/recovery calibration has not started.

## Practical behavior

Hi and Lo now represent the drawn 39 kOhm and 390 kOhm paths into Q4's loaded
input. Lo has substantially less drive, rather than a small compensated level
change. Input trim remains available, and Hi is the natural starting point for
dense fuzz. The previous claims that these input paths impose arbitrary 120/48 Hz
high-pass corners have been removed; C1 is 10 uF, and the important guitar-band
coupling response belongs mostly to the later C4/C5 network.

The preamp has fixed gain/bandwidth and a prepared nominal bias/headroom model.
Sustain acts as R24/R23 after it, including C4/C5, R19, and the next stage's load.
Changing Sustain therefore changes attenuation and coupling behavior into the
clipping stages. It no longer changes three internal amplifier gains at once.
Minimum Sustain retains R23's nonzero signal path; it is not a gate or bypass.

Existing parameter IDs, ranges, defaults and choice ordering are unchanged.
The Step 2 tone network, output stage and artwork/UI are unchanged. Existing
settings intentionally produce the revised sound; the user chose this policy.
There is no automatic loudness compensation in the plugin.

## Circuit and device assumptions

The repository schematic remains the source of component values:

- Q4: R13 10 kOhm, R22 1 kOhm, R14 100 kOhm, R9 470 kOhm, C10 470 pF.
- Input: C1 10 uF, R2/R27 39/390 kOhm. An ideal low-impedance input source is
  assumed. R1's 2 MOhm input shunt does not change that ideal source voltage.
- Sustain: C4 470 nF, R24 100 kOhm, R23 1 kOhm, C5 47 nF, R19 6.2 kOhm.
- Nominal Q3 input load: R18 10 kOhm, R21 100 Ohm, R20 100 kOhm, R17 470 kOhm
  with a reduced small-signal transistor model.

Nominal parameters are **beta=500 for Q4**, **beta=600 for Q3**, **Vbe=.65 V**,
**thermal voltage=.02585 V**, a **9 V supply**, and a **.2 V saturation margin**
for the preamp headroom limit. These are chosen modeling assumptions, not
measurements of the user's pedal or a selected transistor lot. The manufacturer
datasheets establish high-gain device families and their variation:
[MPSA18](https://www.onsemi.cn/download/data-sheet/pdf/mpsa18-d.pdf),
[2N5088/2N5089](https://www.onsemi.com/pdf/datasheet/2n5088-d.pdf).
Datasheet limits under specified test conditions do not calibrate this model's
actual operating point, Vbe, or large-signal waveform.

The existing provisional coordinate remains one DSP sample unit per volt of AC
about bias. This is not a measured relationship between the guitar/interface and
digital full scale. The plugin cannot recreate pickup loading that happened
before the audio interface; no humbucker impedance or inductance was invented.

## DSP implementation

`InputCircuit.h` holds component values and preparation-time operating-point
calculations. For beta B, collector/base-feedback/base/emitter resistances
Rc/Rf/Rb/Re, define `K=1+(Rc+Rf)/Rb`. The base current is:

```text
Ib = (Vcc - K*Vbe) / (Rc*(B+1) + Rf + K*(B+1)*Re)
```

The reduced hybrid-pi model includes emitter degeneration and collector-to-base
feedback. The independent Python reference solves three DC equations directly,
rather than copying this closed-form implementation, and checks base/collector KCL.

For nominal Q4, Ic is 0.312 mA, Ve is 0.312 V, Vb is 0.962 V, and Vc is 5.779 V.
Its base-driven collector gain is approximately -9.005 before loading from the
input source. Q4's nominal feedback-loaded input resistance is about 30.18 kOhm.
The 39/390 kOhm input paths then produce source-to-preamp gains around 3.93/0.647
in the midband limit, a roughly 15.7 dB drive difference.

`InputVoicingStage` calculates the Q4 base response including C1 and R9/C10
feedback loading. Both endpoint filters run continuously, and their outputs
morph over the existing 10 ms voicing ramp. `PreGainStage` applies the matching
collector/base transfer at the same oversampled rate. The signal uses the
engine's polarity-normalized convention; physical inversion remains in the
feedback calculations. Double filter state preserves C1's very low pole even
at 768 kHz internal sample rate.

The preamp's former quadratic skew and arbitrary filters are replaced by a
fixed, monotonic `headroom*tanh(signal/headroom)` limit, with unity slope near
zero. Its nominal positive/negative headroom is about 5.267/3.221 V in that sign
convention. It cannot fold a large negative input toward the positive rail.
This is a prepared linear feedback response followed by a soft headroom limit,
not a nonlinear transistor equation solver.

### Loading and Sustain

A constant collector source resistance initially produced about 3.13 dB error
at 10 kHz against the coupled small-signal circuit. It was replaced before
delivery with a frequency-dependent Thevenin RC equivalent derived from Q4's
R9/C10 feedback. In the guitar band it has the form:

```text
Zout(s) = Rinf + (R0 - Rinf) / (1 + s*tau)
```

The runtime implementation uses a series resistor plus a parallel RC. Its
midband resistance is about 6.600 kOhm in Hi and 4.536 kOhm in Lo; it falls with
frequency as the feedback network dictates. C1 is treated as a short only for
this source-impedance reduction, while remaining explicit in the input transfer.
That very-low-frequency approximation is checked against the fully coupled
reference over the acceptance band rather than being hidden.

`SustainStage` combines that equivalent with C4, the pot's upper leg, R23 plus
the lower leg, C5/R19, and a nominal Q3 base load of **6.413 kOhm**. Three
capacitor states and a cached 2x2 trapezoidal solve retain capacitor memory during
smoothed control movement. There is no singular zero-resistance endpoint:
R23 and the source/load network remain finite. Equivalent-source parameters
interpolate during the existing Hi/Lo morph; physical endpoint models are tested
independently. The reference uses a negligible 1 mOhm regularization for its zero
upper pot leg, with no corresponding artificial resistance needed in the DSP.

The first clipping stage's arbitrary 92 Hz pre-high-pass is removed because
C4/C5/R19 coupling is now explicit. Stage A's drive is fixed at its former
full-Sustain value, 18x. Stage B's per-mode drives are similarly fixed at their
former full-Sustain values: Off 2.34x, Silicon 6.4x, Germanium 5.4x. Their existing
nonlinear transfers and remaining filters stay in place for Step 4 calibration;
these fixed drive numbers are **not** claimed to be derived transistor gains.

Input conditioning, preamp, Sustain and both clipping stages all run inside the
selected oversampling domain. Parameter ramps remain generated at host rate and
held per oversampling group. Scratch/state allocation occurs in preparation;
Sustain's factorization is reused when smoothed control values are unchanged.

## Independent validation

Local bundle: `artifacts/input-step3-20261115/`. It contains source/executable
identity and settings, raw CSV/WAV data, independent validation reports, figures,
build/test logs and retained binaries. Large artifacts remain local and ignored.

`tools/input_reference.py` uses DC KCL and a separate complex AC nodal solver
with a controlled current source. It includes a fully coupled nominal input/Q4/
Sustain circuit, so the production factorization is checked against the coupled
network as well as against its individual blocks. The nominal Q3 load remains a
resistance in both models; this is not an end-to-end transistor simulation.

| Check | Limit | Result |
| --- | --- | --- |
| Input/preamp and Sustain block magnitude agreement | <=0.1 dB | **0.000969 dB** maximum |
| Block phase agreement | <=1 degree | **0.000072 degrees** maximum |
| Combined production block responses vs coupled nominal circuit | <=0.1 dB | **0.002483 dB** maximum |
| Combined phase agreement | <=1 degree | **0.014648 degrees** maximum |
| Physical response cases | Complete defined matrix | **348**, spanning 44.1/48/96 kHz and 2x/4x/8x |
| Existing tone-network response | Step 2 limits | Still passes; tone DSP unchanged |
| Fixed-parameter tap adapter vs production engine | <=1e-6 error | **0**, across the full capture |

The main 4x grid uses 20/50/100/440/1000/3000/5000/10000 Hz, both voicings and
Sustain 0/.25/.68/1. At 2x/8x the additional rate/endpoint checks use
20/1000/10000 Hz and Sustain 0/1. Probes have 1e-4 amplitude, .5 seconds settling
and .2 seconds coherent measurement. Reference frequency mapping matches the
trapezoidal discretization. These small-signal checks do not establish large-signal
hardware accuracy.

Five complete-engine hot-input/control-sweep cases cover all three sample rates
at 4x and 48 kHz at 2x/8x. They use .4-peak input, up to +18 dB Input, changes
across Sustain endpoints/intermediate values and Hi/Lo, and a faded stop.
Maximum observed output peak is **0.148554**, adjacent-sample change **0.006696**,
stereo difference **0**, and final tail peak **2.06e-11**. Bounds are .9/.15/1e-6/
1e-4 respectively. This is a transient safety check, not proof of inaudible host
automation. C++ regressions also check preamp monotonicity at 88.2/192/768 kHz.

Debug AU, standalone and test targets build successfully. **CTest passes 4/4**
groups, including seven analytic reference test methods and both production
response integration checks. New DSP compiler warnings were removed; the
measurement writer's pre-existing deprecated API and JUCE linker warnings remain.

The full capture retains 117 gain cases, 3,744 harmonic rows, 75 tone curves and
12 synthetic renders plus the dry file. There are now **nine** stage taps:
dry, input-base equivalent, open-circuit preamp equivalent, loaded Sustain/Q3
input, Clip A, Clip B, downsampled, Tone and output. Input/preamp/Sustain taps
are at the oversampled rate; dry/downsampled/Tone/output are at host rate.

All 22 raw files are bit-identical across the initial and warning-cleanup
captures. Isolated tone CSV data is bit-identical to Step 2. The preamp-equivalent
statistics are invariant across Sustain for identical input/voicing/rate, as
required by the new placement. `repeatability-and-scope.json` records these
checks, preserved Step 2 checksums and the six exported listening-pair checks.

## Listening and remaining limits

`comparison/` pairs the positively reviewed **Step 2** capture with **Step 3**,
using identical dry synthetic audio and default parameter values, separately for
Hi/Lo and all three clipping modes. Files named `original` in this bundle refer
to Step 2, not the earlier uncorrected tone model. Raw float files preserve the
level change; separate PCM24 files are RMS matched by attenuation for listening.
Exact gains and input/source hashes are recorded in `listening-settings.json`.
No compensation enters the plugin. These are synthetic plucks, not a real DI.

Audition with the Les Paul again: compare Hi/Lo, minimum/mid/full Sustain, low
strings and guitar-volume cleanup. Expect Lo to be considerably quieter/cleaner
than the old approximation. Start with Hi for the dense fuzz comparison; do not
judge the model solely by equal numerical settings or by the louder version.
The current app must be quit and reopened to load the rebuilt executable.

Q3 loading is still a nominal small-signal resistance and omits its C12/diode
large-signal interaction. Q4's nonlinear feedback, Early effect, temperature,
device spread, pickup/interface transfer and physical voltage calibration are
not modeled in full. Clipping/recovery calibration and the remaining C13/C3
attributions belong to Step 4. Tone-stack source/recovery load assumptions remain
those approved for Step 2. No real DI recording, physical-pedal match, CPU/alias
qualification or GarageBand acceptance is claimed here.

## Reproduce

```bash
cmake --build build --config Debug --target \
  PyramidFuzzDspTests PyramidFuzzMeasurements PyramidFuzz_AU PyramidFuzz_Standalone
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release --target PyramidFuzzMeasurements
python3 tools/capture_measurements.py \
  --executable build/PyramidFuzzMeasurements_artefacts/Release/PyramidFuzzMeasurements \
  --output artifacts/NEW_INPUT_CAPTURE
python3 tools/validate_input_capture.py \
  --capture artifacts/NEW_INPUT_CAPTURE/raw --report artifacts/NEW_INPUT_CAPTURE/input-validation.json
build/measurements-venv/bin/python tools/compare_tone_revision.py \
  --baseline artifacts/tone-step2-20261115 --revised artifacts/NEW_INPUT_CAPTURE \
  --audio-only --include-hi
build/measurements-venv/bin/python tools/plot_input_revision.py \
  --baseline artifacts/tone-step2-20261115 --revised artifacts/NEW_INPUT_CAPTURE
```

Use fresh output directories. `--input-only` is available on the measurement
executable for focused circuit checks. The original Step 1 and Step 2 archives
remain unchanged. No AU installation or remote push has been performed.
