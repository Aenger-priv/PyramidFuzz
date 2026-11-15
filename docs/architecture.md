# Architecture

## Current implementation

The repository is now set up as a JUCE audio plugin shell with a first-pass fuzz engine:

- `PyramidFuzzAudioProcessor`
  - Owns bus layout and plugin lifecycle
  - Owns `AudioProcessorValueTreeState`
  - Exposes the parameter layout required by the product brief
  - Pulls parameter values into a modular DSP engine each block
  - Reports integer oversampling latency back to the host
  - Initializes the DSP from the current parameter state and splits oversized host blocks into preallocated chunks

- `PyramidFuzzAudioProcessorEditor`
  - Owns control widgets and parameter attachments
  - Presents the minimum stompbox-style UI shell for the core controls

## DSP blocks

DSP is organized under `src/dsp/` as discrete blocks:

- `InputVoicingStage`
  - Applies input trim and the C1 / 39 kOhm or 390 kOhm network into nominal Q4 input loading
  - Includes R14 and feedback-dependent loading from R9/C10 and the reduced Q4 model
  - Runs both physical endpoints for a smooth Hi/Lo morph; shares the oversampled rate with the preamp

- `PreGainStage`
  - Fixed Q4 collector/base response from R13/R22/R9/C10 and a prepared nominal operating point
  - Uses a monotonic soft limit with asymmetric headroom derived from the nominal bias and 9 V supply
  - Does not depend on Sustain; its output is the polarity-normalized open-circuit source equivalent

- `SustainStage`
  - Models C4, R24/R23, C5/R19 and the nominal Q3 input load after Q4
  - Uses a frequency-dependent RC source equivalent for Q4, including C10 feedback loading
  - Three capacitor states and a cached 2x2 trapezoidal solve; endpoints remain nonzero because R23 is 1 kOhm

- `InputCircuit`
  - Named circuit values, nominal bias/small-signal calculations at preparation, and double-state linear filters
  - Beta/Vbe and small-signal load approximations are documented in the Step 3 review

- `ClippingStageA`
  - First major fuzz generator
  - Uses pre/post filtering and a dense soft-to-hard transfer with cubic weighting for sustain and compression
  - Fixed 18x internal drive; its former arbitrary 92 Hz coupling filter is replaced by `SustainStage`

- `ClippingStageB`
  - Second major clipping stage
  - Per-mode internal drive is fixed; the Sustain divider changes its incoming signal instead
  - Uses a continuous-slope diode knee to avoid extra high-frequency content at the clipping threshold
  - Offers three continuously morphable internal responses:
    - Off: lightly limited, more open
    - Silicon: harder knee, brighter ceiling, tighter attack
    - Germanium: lower threshold, softer knee, slight asymmetry

- `ToneStack`
  - Solves a five-node passive network by trapezoidal nodal analysis
  - Uses 0.022 uF C8 and 25 kOhm R8, with assumed 10 kOhm source and 220 kOhm recovery loading
  - Step 2 corrects R8 to series resistance and C8 to ground, preserving the C9/R5 high-pass branch and C3 output coupling
  - Tone runs from dark to bright; increasing High reduces R8 and raises the low-pass corner. Passive source loading means its effect depends on Tone
  - The independent AC grid check covers 75 settings at 44.1/48/96 kHz; the source/load assumptions remain approximations

- `HighControlFilter`
  - Retained as a reusable contour block, but bypassed in the main path
  - `High` changes the modeled series R8 resistance in `ToneStack`

- `OutputStage`
  - Applies recovery gain and a small amount of final glue saturation before volume scaling

- `OversamplingEngine`
  - Wraps JUCE oversamplers for 2x / 4x / 8x operation
  - 4x is the default working mode for v1; integer-latency compensation matches the reported host latency

- `FuzzEngine`
  - Coordinates parameter smoothing, oversampling selection, stage ordering, and channel-safe processing

## Parameter model

The processor exposes the v1 user-facing parameters:

- `Input`
- `Sustain`
- `Tone`
- `High`
- `Volume`
- `Voicing`
- `Clipping Mode`
- `Oversampling`

Implementation notes:

- `Input`, `Sustain`, `Tone`, `High`, and `Volume` are smoothed in the DSP engine.
- `Voicing` and `Clipping Mode` are handled as smoothed morph values rather than hard edge switches, which reduces clicks when changed.
- `Oversampling` changes at block boundaries and re-prepares the nonlinear stages for the new oversampled rate. The wet path fades down and back up over 6 ms around the state reset, preventing an audible discontinuity when the quality setting changes.

## Signal path

Current processing order:

1. Upsample into the nonlinear domain
2. Input voicing and trim
3. Fixed PreGainStage
4. SustainStage with C4/C5 coupling and loading
5. Fixed-gain ClippingStageA
6. Fixed-gain ClippingStageB with selectable diode response
7. Downsample back to the host sample rate
8. ToneStack
9. OutputStage

When the oversampling mode changes, the output stage is briefly muted while the nonlinear filters and oversampler are reconfigured, then returned with a 6 ms linear fade. This is deliberately a small quality-switch interruption rather than a click or burst of stale filter state.

## Oversampling placement

Oversampling wraps the input/preamp/Sustain network and the main nonlinear stages:

- `InputVoicingStage`
- `PreGainStage`
- `SustainStage`
- `ClippingStageA`
- `ClippingStageB`

This keeps the strongest alias generators inside the oversampled path while leaving the later tonal shaping at base rate to control CPU use.

## Current approximations

- The tone stack solves a simplified passive network with approximated source and load impedances; it is not a complete transistor-level circuit simulation.
- The clipping stages are tuned transfer functions rather than literal diode equations.
- Filter controls that are implemented as path blends are smoothed musically, but the design is still intended for refinement by ear.
- The Step 3 input model uses nominal beta/Vbe, a low-impedance source, linear feedback shaping with a separate preamp headroom limit, and a nominal midband Q3 input resistance. It does not reproduce pickup/interface loading upstream of conversion or large-signal transistor/diode loading.
- Clipping gain, transfer functions and remaining bandwidth approximations await Step 4 calibration. Stage A retains its previous full-drive constant and Stage B its previous full-drive per-mode constants.
- The 24 Hz blocker in `ClippingStageB` is attributed to C3 in its comment, while `ToneStack` already models C3 after the wiper. This needs resolution during the coupling review.

## Measurement baseline

[Step 1's review](baseline-review.md) records the unchanged `b98a3be` DSP, archived original builds, circuit variant, independent AC netlist, measured results, session policy, and proposed gates. `tools/Measurements.cpp` uses production DSP classes; a fixed-control tap adapter is checked sample by sample against `FuzzEngine`. It is an offline tool and does not add branches or instrumentation to the plugin's audio callback. The independent Python reference imports no production DSP. Large captures are local under ignored `artifacts/`.

[Step 2's review](tone-correction-review.md) records the corrected passive network, intended control mapping and positive live test with humbuckers. Its build and audio are retained as the comparison point for Step 3.

[Step 3's review](input-sustain-review.md) records the derived input and Sustain path. CTest independently checks nominal DC KCL, small-signal AC responses, coupled-network agreement, hot inputs, and smoothed control changes. The tone network, parameter model and recovery stage are unchanged in Step 3. The measurement adapter now records nine stages; input/preamp/Sustain taps run at the oversampled rate. Listening compensation remains confined to exported comparison files.

## Refinement priorities

1. Audition the input/Sustain revision against the retained Step 2 build, especially Hi/Lo contrast, cleanup, and low strings.
2. Calibrate clipping, remaining coupling, and recovery against measurements and listening; revisit recovery loading with that work.
3. Capture a real guitar DI for repeatable listening comparisons.
4. Measure aliasing, CPU, quality switching, and host automation behavior.
5. Complete the UI/UX and GarageBand validation gates in the [revision plan](implementation-review-plan.md).
