# Architecture

## Current implementation

The repository is now set up as a JUCE audio plugin shell with a first-pass fuzz engine:

- `PyramidFuzzAudioProcessor`
  - Owns bus layout and plugin lifecycle
  - Owns `AudioProcessorValueTreeState`
  - Exposes the parameter layout required by the product brief
  - Pulls parameter values into a modular DSP engine each block
  - Reports oversampling latency back to the host

- `PyramidFuzzAudioProcessorEditor`
  - Owns control widgets and parameter attachments
  - Presents the minimum stompbox-style UI shell for the core controls

## DSP blocks

DSP is organized under `src/dsp/` as discrete blocks:

- `InputVoicingStage`
  - Handles input gain conditioning plus Hi / Lo input voicing
  - Hi mode trims more low end and keeps attack tighter
  - Lo mode leaves more low-frequency weight in front of the gain stages

- `PreGainStage`
  - Models the first transistor-like stage as coupling-style filtering into mild asymmetric saturation
  - Lives inside the oversampled path because it directly feeds the nonlinear core

- `ClippingStageA`
  - First major fuzz generator
  - Uses pre/post filtering and a dense soft-to-hard transfer with cubic weighting for sustain and compression

- `ClippingStageB`
  - Second major clipping stage
  - Offers three continuously morphable internal responses:
    - Off: lightly limited, more open
    - Silicon: harder knee, brighter ceiling, tighter attack
    - Germanium: lower threshold, softer knee, slight asymmetry

- `ToneStack`
  - Approximates the Big Muff family tone section as a dark low-pass branch and bright high-pass branch
  - Adds a center-position scoop blend so the middle of the sweep feels recognizably Muff-like rather than flat

- `HighControlFilter`
  - Adds a restrained upper-mid / treble contour after the main tone stack
  - Implemented as a blend into a fixed high-shelf and presence contour path

- `OutputStage`
  - Applies recovery gain and a small amount of final glue saturation before volume scaling

- `OversamplingEngine`
  - Wraps JUCE oversamplers for 2x / 4x / 8x operation
  - 4x is the default working mode for v1

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
- `Oversampling` changes at block boundaries and re-prepares the nonlinear stages for the new oversampled rate.

## Signal path

Current processing order:

1. Input voicing and gain at base sample rate
2. Upsample into the nonlinear domain
3. PreGainStage
4. ClippingStageA
5. ClippingStageB
6. Downsample back to the host sample rate
7. ToneStack
8. HighControlFilter
9. OutputStage

## Oversampling placement

Oversampling currently wraps the three nonlinear stages:

- `PreGainStage`
- `ClippingStageA`
- `ClippingStageB`

This keeps the strongest alias generators inside the oversampled path while leaving the later tonal shaping at base rate to control CPU use.

## Current approximations

- The tone stack is a practical branch-blend approximation, not a strict passive network solver.
- The clipping stages are tuned transfer functions rather than literal diode equations.
- Filter controls that are implemented as path blends are smoothed musically, but the design is still intended for refinement by ear.

## Refinement priorities

1. Tune the clipping transfer constants by ear against a target pedal reference.
2. Improve the tone stack fit so the bright side cuts harder without adding fizz.
3. Refine output calibration and perceived loudness across clipping modes.
4. Add metering and optional hidden trim controls for development.
5. Validate host behavior and automation smoothness in GarageBand.
