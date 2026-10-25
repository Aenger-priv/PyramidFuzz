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
  - Handles input gain conditioning plus the schematic's Hi / Lo input loading
  - Models the C1 coupling behaviour and 39 kOhm / 390 kOhm input-resistor paths
  - Lo retains more bass but has lower drive, as in the higher-resistance path

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
  - Solves the modified C9/R5, C8/R8, R25 and C3 passive network by trapezoidal nodal analysis
  - Uses the confirmed 0.022 uF C8 and 25 kOhm R8 as component values, including source and recovery-stage loading

- `HighControlFilter`
  - Retained as a reusable contour block, but bypassed in the main path
  - `High` is now the actual R8 rheostat in `ToneStack`, as wired in the schematic

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

## Refinement priorities

1. Tune the clipping transfer constants by ear against a target pedal reference.
2. Improve the tone stack fit so the bright side cuts harder without adding fizz.
3. Refine output calibration and perceived loudness across clipping modes.
4. Expand offline checks with measured response and reference recordings.
5. Validate host behavior and automation smoothness in GarageBand.
