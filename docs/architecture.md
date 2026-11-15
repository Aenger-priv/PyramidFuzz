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
  - Handles input gain conditioning and a tuned Hi / Lo approximation
  - Uses fixed filter curves and gain factors; these are not yet derived from C1 and the 39 kOhm / 390 kOhm input-resistor paths
  - Lo currently retains more bass and has lower drive

- `PreGainStage`
  - Models the first transistor-like stage as coupling-style filtering into mild asymmetric saturation
  - Lives inside the oversampled path because it directly feeds the nonlinear core

- `ClippingStageA`
  - First major fuzz generator
  - Uses pre/post filtering and a dense soft-to-hard transfer with cubic weighting for sustain and compression

- `ClippingStageB`
  - Second major clipping stage
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

1. Input voicing and gain at base sample rate
2. Upsample into the nonlinear domain
3. PreGainStage
4. ClippingStageA
5. ClippingStageB
6. Downsample back to the host sample rate
7. ToneStack
8. OutputStage

When the oversampling mode changes, the output stage is briefly muted while the nonlinear filters and oversampler are reconfigured, then returned with a 6 ms linear fade. This is deliberately a small quality-switch interruption rather than a click or burst of stale filter state.

## Oversampling placement

Oversampling currently wraps the three nonlinear stages:

- `PreGainStage`
- `ClippingStageA`
- `ClippingStageB`

This keeps the strongest alias generators inside the oversampled path while leaving the later tonal shaping at base rate to control CPU use.

## Current approximations

- The tone stack solves a simplified passive network with approximated source and load impedances; it is not a complete transistor-level circuit simulation.
- The clipping stages are tuned transfer functions rather than literal diode equations.
- Filter controls that are implemented as path blends are smoothed musically, but the design is still intended for refinement by ear.
- Sustain currently changes drive in all three nonlinear stages; the schematic places R24 after the input preamp. A later step will address this gain distribution.
- The 24 Hz blocker in `ClippingStageB` is attributed to C3 in its comment, while `ToneStack` already models C3 after the wiper. This needs resolution during the coupling review.

## Measurement baseline

[Step 1's review](baseline-review.md) records the unchanged `b98a3be` DSP, archived original builds, circuit variant, independent AC netlist, measured results, session policy, and proposed gates. `tools/Measurements.cpp` uses production DSP classes; a fixed-control tap adapter is checked sample by sample against `FuzzEngine`. It is an offline tool and does not add branches or instrumentation to the plugin's audio callback. The independent Python reference imports no production DSP. Large captures are local under ignored `artifacts/`.

[Step 2's review](tone-correction-review.md) records the corrected passive network and intended control mapping. CTest compares measured magnitude/phase against the unchanged independent netlist and checks tone-only smoothed control sweeps. Input, preamp, clipping, oversampling and recovery code are unchanged. Default values and parameter identifiers are unchanged, but existing settings intentionally render with the corrected tone response. Listening comparisons apply gain only to separate exported files.

## Refinement priorities

1. Audition the corrected tone network, including a real guitar DI comparison; revisit source/recovery loading when calibrating the surrounding stages.
2. Align input loading and Sustain placement with the schematic.
3. Calibrate clipping, coupling, and recovery against measurements and listening.
4. Measure aliasing, CPU, quality switching, and host automation behavior.
5. Complete the UI/UX and GarageBand validation gates in the [revision plan](implementation-review-plan.md).
