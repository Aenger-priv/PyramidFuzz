# Architecture

## Plugin and parameters

`PyramidFuzzAudioProcessor` owns the lifecycle, buses and
`AudioProcessorValueTreeState`. It initializes DSP from saved parameters, splits
oversized blocks into prepared chunks, and reports fixed six-sample oversampling latency.
The editor owns the controls and parameter attachments. DSP lives separately in
`src/dsp/`; offline measurements and independent references live in `tools/`.

Parameter IDs, ranges, defaults and choice ordering remain unchanged through
Steps 2–4. Input, Sustain, Tone, High and Volume use continuous smoothing;
Voicing and Clipping use 10 ms morphs. The user chose revised sound for existing
sessions, with original builds archived separately, rather than a legacy DSP
model inside the new plugin.

## Signal path

1. Upsample with JUCE's 2x/4x/8x filtered oversampler; default 4x.
2. Input trim and C1/Hi–Lo source-resistance conditioning.
3. Fixed Q4 preamp response and headroom.
4. C4, Sustain R24/R23, C5/R19 into the nonlinear Q3 input port.
5. Q3 amplifier with R17/C12 and C6/silicon diode feedback.
6. C13/R12 coupling into Q2, with R15/C11 and selectable C7/diode feedback.
7. Passive Tone/High network including C3 into nominal Q1 input loading.
8. Q1 recovery headroom and C2/R26 output coupling.
9. Downsample, apply the common volts-to-digital scale, Volume and quality fade.

All nonlinearities, including recovery, run inside oversampling. No arbitrary
post-clip low-pass or duplicate C3 high-pass is present. The final Volume stage
is linear and does not clamp over-range output.

## DSP blocks

| File/block | Responsibility |
| --- | --- |
| `InputCircuit` | Named input/component values, nominal bias and small-signal calculations, linear source equivalents and double-state filters. No runtime transistor-level bias solve. |
| `InputVoicingStage` | C1 and 39/390 kOhm Hi/Lo paths into Q4's frequency-dependent input load. Both endpoints run continuously for smooth switching. |
| `PreGainStage` | Fixed nominal Q4 feedback response from R13/R22/R9/C10, followed by monotonic bias-derived soft headroom. Output is the polarity-normalized open-circuit source equivalent. |
| `SustainStage` | C4/R24/R23/C5/R19 plus Q4's nominal RC source equivalent. Exposes a Thevenin port; Q3's solved normalized base voltage updates capacitor currents. Its nominal-load wrapper remains available for isolated AC checks. |
| `FeedbackClipper` | Reduced voltage amplifier, resistor/capacitor feedback, series capacitor and nonlinear diode feedback. One bounded bracketed Newton solve per sample; warm starting and bisection fallback; double capacitor state. |
| `ClippingStageA` | Q3 with fixed opposing 1N914-like diodes. |
| `ClippingStageB` | Q2 with smoothly weighted Off/1N4001-like/Ge-like branches. Ge has one forward and two reverse junctions. The same amplifier, R/C feedback and capacitor state serve every mode. |
| `InterstageCoupling` | C13/R12 and a nominal linear Q3 output-impedance equivalent. Receives Q2's actual base voltage to update coupling currents. |
| `ToneStack` | Five-node passive trapezoidal solve: C9/R5 bright arm, series R8/shunt C8 dark arm, R25 wiper and C3. Double states and cached Cholesky factorization. |
| `OutputStage` | Nominal Q1 gain/headroom, C2 output coupling and R26 load. Defines the common 1/3 volts-to-digital output scale. Volume is applied after downsampling. |
| `OversamplingEngine` | JUCE oversamplers, quality selection and integer-latency compensation. |
| `FuzzEngine` / `NonlinearPath` | Prepared buffers and three rate-specific paths, smoothing, per-channel processing and quality fade. |

High operates R8 directly in `ToneStack`. The unused legacy contour block and
its unreferenced biquad/transfer helpers were removed during final cleanup.

## Circuit conventions and remaining approximations

Tone=0 selects the dark arm; Tone=1 selects the bright arm. Increasing High
reduces R8 and extends the dark arm's upper response. The passive controls
interact. C8 is 22 nF, the chosen drawing variant. Pot ends use small nonzero
resistances for a well-conditioned circuit solve.

The Step 4 tone source/load are nominal 4.886 kOhm/45.91 kOhm, derived from Q2/Q1
small-signal KCL. Earlier Step 2–3 captures used 10 kOhm/220 kOhm and remain
unchanged. The independent reference JSON now describes the revised loading.

Input uses 1 digital unit = 1 model volt; output uses 3 model volts = 1 digital
unit before Volume. This is an explicit convention, not a calibrated
pickup/interface/pedal relationship. Natural clipping-mode level differences
are preserved. Only exported listening files receive matching adjustments.

Device beta, Vbe, thermal voltage and diode Is/N/series-continuation values are
nominal assumptions. All four transistors remain silicon; the material switch
selects only Q2's feedback diodes. Diode curves are not fitted to measured parts.
Voltage-amplifier soft headroom substitutes for full transistor nonlinearity.
The model omits temperature, junction charge, reverse breakdown, noise and
parameter spread.

Input/Sustain uses an actual nonlinear Q3 base port, but output-source equivalents
remain nominal and unilateral. They do not close the complete bidirectional
large-signal circuit. Q3's output equivalent assumes a 30 kOhm source; Q1's
output equivalent assumes a 50 kOhm tone-wiper source. Source impedances do not
track every control position or diode conduction. See the
[clipping/recovery review](clipping-recovery-review.md) for all component values,
reference equations and the switch-drawing ambiguity.

## Processing changes

The host-rate smoother values are held for each group of oversampled samples.
There is no allocation in the ordinary fixed-quality sample loop. Quality
changes use a block-boundary state reset with a 6 ms fade down/up. The reset
includes tone/recovery/C2 because they run at the selected rate. This briefly
interrupts the wet signal.

All three quality paths are prepared before processing. A quality change resets
and selects an existing path with the fade; it does not re-prepare stages or
notify host latency. Two padding samples align 2x's native four-sample delay
with the six-sample delay of 4x/8x. Mono-to-stereo runs one circuit path before
output duplication. State loading validates the complete eight-parameter tree
before replacing it; the processor advertises a conservative ten-second tail.

Native host bypass uses a preallocated six-sample dry delay, preserving the
reported latency and mono-to-stereo expansion. A 5 ms linear wet/dry transition
smooths toggles; an initially bypassed instance starts dry immediately. The wet
engine keeps running so its filters and quality state stay warm. This bypass
does not save DSP CPU, and adds no APVTS parameter. Tests cover all three bus
layouts, independent stereo channels, exact dry delay and allocation-free
callbacks. Hosts that suspend an instance can still interrupt its processing.

The diode evaluator and warm solve preserve the declared law with fewer
transcendental evaluations. Processor tests cover routing/state/chunking and
measure zero C++ heap allocations during the exercised quality changes. See the
[processing review](processing-quality-review.md) for scope, local CPU results,
alias estimates and the explicitly unmet extreme-settings targets.

## Measurements and retained revisions

`tools/Measurements.cpp` uses production stages in a fixed-control tap adapter;
every render is checked sample by sample against `FuzzEngine`. Ten taps record
`dry`, `input_voicing`, `pre_gain`, `sustain` (solved normalized Q3 base), `clip_a`, `clip_b`,
`tone`, `recovery`, `downsampled`, and `output`. Dry/downsampled/output are host
rate; the other taps are oversampled. Stage values inside the model are volts,
while final output includes digital scaling and Volume.

Python references independently assemble DC/AC KCL and solve nonlinear feedback
by bisection. CTest covers DSP behavior, reference sanity, the tone grid at base
and oversampled rates, input/Sustain AC, feedback traces, and long DC decay.
Large captures, archives, figures and listening WAVs are ignored under
`artifacts/`. Capture manifests freeze source hashes, toolchain and raw data.

- [Step 1](baseline-review.md): original `b98a3be` baseline and session policy.
- [Step 2](tone-correction-review.md): passive topology/direction correction;
  positively auditioned with Les Paul humbuckers.
- [Step 3](input-sustain-review.md): input loading, fixed preamp and Sustain placement.
- [Step 4](clipping-recovery-review.md): feedback clipping, coupling and recovery.

Steps 5–7 are implemented. The [delivery record](delivery-validation.md) separates
passing automated/native checks from remaining manual acceptance and the known
CPU/alias stress exceptions.

## Control presentation and meters

Parameter text formatting uses dB, a 0–10 Sustain amount and Tone/High
percentages, while stored values remain unchanged. Slider attachments retain
default reset; controls add explicit focus order, fine Shift-drag, help and
mode-specific footer text. Host bypass is retained as the complete-effect bypass.

`PeakMeter` holds sample-timed 400 ms exponential release and a one-second
overload indicator, publishing lock-free atomic level/flag snapshots to the
editor. Meters display −60..0 dBFS bars and unbounded numeric peaks. Input is
before trim and output after Volume. They do not affect audio. See the
[UI review](ui-playability-review.md) for previews and interaction status.

## App icon

`assets/generated/pyramid-fuzz-icon.png` supplies the black/gold pyramid-and-eye
artwork. Its adjacent JSON records the generation prompt and style reference.
CMake's `ICON_BIG` lets JUCE package `Icon.icns` and the bundle icon entry.
Rebuilding requires no image-generation service or network access.
