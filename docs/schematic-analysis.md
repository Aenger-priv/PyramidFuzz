# Pharo modified 47 Ram's Head schematic analysis

Reference: `Pharaoh Big Muff Clone Schematic.jpg`, supplied with this repository.

## What the schematic establishes

This is a four-transistor Big Muff family circuit, not a simple cascaded
distortion. It has an input preamplifier, two common-emitter gain stages with
collector-to-base feedback and shunt clipping networks, a passive tone network,
and a recovery/output stage. A circuit-informed digital implementation should
retain those roles and their order.

| Section | Schematic evidence | DSP implication |
| --- | --- | --- |
| Input | C1 10 uF, R1 2 MOhm, selectable R2 39 kOhm / R27 390 kOhm | Model the switch as source resistance and input attenuation/loading, rather than merely a fixed EQ crossfade. |
| Q4 preamp | MPSA18, R13 10 kOhm collector, R22 1 kOhm emitter, R9 470 kOhm and C10 470 pF collector-to-base feedback | Use a gain stage with feedback-shaped bandwidth and modest headroom. |
| First fuzz gain stage | 2N5089, R18 10 kOhm, R21 100 Ohm, R17 470 kOhm, C12 470 pF, C6 47 nF and D3/D4 1N914 feedback clipper | Use band-limited feedback-style silicon clipping; this is the first dense sustain generator. |
| Second fuzz gain stage | 2N5089, R15 470 kOhm, R10 100 Ohm, R16 100 kOhm, C11 470 pF; C7 47 nF and selectable D1/D2 silicon, D5 versus D6+D7 germanium, or no diodes | Off must remove only this stage's diode feedback. Silicon and germanium need distinct thresholds and knees, but retain the same gain-stage filtering. |
| Tone network | C9 0.01 uF, R5 470 kOhm, R25 Tone 250 kOhm, C8/R8 high-control branch | Implement a loaded, passive two-branch tone response; do not substitute a generic LP/HP blend plus a centre EQ scoop. |
| Recovery | 2N5089, R7 470 kOhm, R6 10 kOhm, C2 10 uF, R26 Volume 100 kOhm | Apply recovery gain and output coupling after the passive level loss; reserve saturation for natural recovery-stage headroom. |

## Modifications called out on the drawing

- Hi/Lo input selection changes the input resistor from 39 kOhm to 390 kOhm.
- The second clipping feedback path selects silicon, germanium, or diode bypass.
- R8 is a 25 kOhm high/presence control in the tone network.
- The caption says C8 was enlarged to **2.2 uF**. Its small on-drawing label
  appears to read **0.022 uF**. The supplied component clarification confirms
  that **0.022 uF** is the intended value.

## Consequence for the current DSP

The engine now derives input loading and Sustain placement from the schematic,
solves the passive tone topology, and uses reduced nonlinear feedback models for
the two clipping stages. C6/C7 retain capacitor memory, and germanium models the
one-junction D5 branch versus the reverse two-junction D6+D7 branch. Off opens
the diode branch while retaining the amplifier and resistor/capacitor feedback.
The centre switch wiring is ambiguous relative to its ON/OFF/ON annotation;
the open-branch interpretation follows the product brief.

C13/R12 supplies interstage coupling, C3 appears once after the tone wiper, and
C2 provides output coupling after recovery. Tone loading and recovery gain use
nominal device calculations. All nonlinear stages run inside oversampling.

This remains a reduced schematic-guided model: assumed device curves, nominal
linear output impedances, fixed bias and soft headroom replace full transistor
simulation. No measured-pedal calibration is claimed. The
[baseline](baseline-review.md), [tone](tone-correction-review.md),
[input/Sustain](input-sustain-review.md), and
[clipping/recovery](clipping-recovery-review.md) reviews record each revision,
its independent checks, retained builds and remaining assumptions.

## Implementation sequence

1. Replace input voicing with coupling-capacitor/source-resistance behaviour.
2. Rework each gain stage around its documented feedback bandwidth and bias/headroom.
3. Implement the two clipping feedback paths as bounded nonlinear feedback
   approximations inside the existing oversampled domain.
4. Validate the implemented passive tone-stack response with frequency sweeps.
5. Calibrate gain, clipping threshold, frequency response, and level loss with
   offline sweeps before final by-ear tuning.
