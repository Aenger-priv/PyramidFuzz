# Step 4 — Clipping feedback and recovery

Step 4 replaces the generic transfer curves with a reduced feedback-circuit
model. Existing sessions use the revised sound, with parameter IDs, ranges,
choice ordering and defaults preserved. The previous standalone/AU and source
are retained under `artifacts/clip-step4-preserved-b864aad-20261115/`.
The earlier Step 1–3 archives are also retained. Nothing is installed into
GarageBand automatically.

## Circuit model

All stages still use nominal device assumptions rather than a full runtime
transistor simulation. A model input unit represents 1 V of AC signal; this is
a chosen scale, not a measurement of the guitar, interface or original pedal.
Bias is solved at preparation using 9 V, beta 600, Vbe 0.65 V and the schematic's
resistors. Q4 keeps Step 3's beta 500 assumption.

| Block | Implementation |
| --- | --- |
| Q3 first clipper | R18 10 kOhm, R21 100 Ohm, R20 100 kOhm; R17 470 kOhm parallel C12 470 pF feedback; C6 47 nF in series with opposing single 1N914-like diodes. |
| Q2 second clipper | Same nominal amplifier parameters; R15/C11 feedback; C7 47 nF in series with the selected diode branch. |
| Silicon | Opposing single 1N4001-like diodes in Q2 feedback. |
| Germanium | D5: one junction base-to-collector; D6+D7: two junctions in reverse. The different branch slopes and thresholds generate asymmetry, without a quadratic skew or per-mode gain. |
| Off | Opens the C7/diode branch. The amplifier and R15/C11 feedback remain active. It is not effect bypass. |
| Interstage coupling | C13 47 nF, R12 6.2 kOhm, and an explicit nominal Q3 source-impedance equivalent. C5/R19 remain in the Sustain network. |
| Tone | Same Step 2 topology and 22 nF C8 variant, now loaded by nominal Q2/Q1 impedances rather than the earlier 10 kOhm/220 kOhm guesses. C3 47 nF appears once, after the tone wiper. |
| Q1 recovery | R6 10 kOhm, R4 2.2 kOhm, R3 100 kOhm and R7 470 kOhm set nominal gain/bias/headroom. C2 10 uF couples into R26's 100 kOhm load before digital Volume. |

The drawing's centre switch contact can be read as a direct connection through
C7. That conflicts with its ON/OFF/ON annotation and the product brief's open,
less-compressed Off mode. This implementation follows the open diode-branch
interpretation. A shorted C7 feedback branch would be a different circuit.

The prepared Q3/Q2 amplifier has approximately 63.93 open voltage gain before
feedback. Positive/negative normalized headroom is 4.67/4.08 V for Q3 and
4.08/4.67 V for Q2.
Headroom is a monotonic soft limit inside the feedback solve. The actual closed
response depends on source resistance, frequency, diode current and capacitor
history. No independent mode-drive multipliers or extra pre/post low-pass EQ
are used.

The engine represents four physical transistor inversions with positive-gain
block coordinates. That preserves the complete circuit's positive polarity,
but alternating blocks must reverse their normalized rail orientation. Q3 and
Q1 use the opposite headroom orientation from Q4 and Q2. Q3's symmetric silicon
pair is invariant under this coordinate reversal; Q2's germanium directions
remain referenced to the physical base/collector orientation. Regressions check
both alternate clipping rails and the recovery rail direction.

Sustain now supplies a Thevenin port into Q3; Q3's solved normalized base voltage determines
the current through C4/C5 and the pot. The isolated input/Sustain reference keeps
its nominal resistive Q3 load for comparison with Step 3. The complete engine
uses the nonlinear port.

## What “silicon” and “germanium” mean here

The diode branches use an exponential current–voltage law with the explicit
representative parameters in `tools/reference/clipping-network.json`:

| Nominal curve | Is | Ideality N | Series-resistance continuation |
| --- | ---: | ---: | ---: |
| 1N914-like | 2.52 nA | 1.752 | 2 Ohm |
| 1N4001-like | 14 nA | 1.984 | 0.05 Ohm |
| 1N34A-like | 0.2 uA | 1.35 | 40 Ohm |

These are **assumed curves, not manufacturer-fitted or measured specimens**.
The schematic does not identify a specific germanium part. Manufacturer data
for [1N914](https://www.vishay.com/docs/85622/1n914.pdf),
[1N4001](https://www.vishay.com/docs/88503/1n4001.pdf) and
[1N34A](https://my.centralsemi.com/datasheets/1N34A.PDF) provide useful physical
context but their limits do not uniquely determine low-current Is/N values.
Temperature, part spread, junction capacitance and reverse breakdown are not
modeled. Each series junction adds its exponential voltage drop. At high
currents a continuous-slope tangent continuation bounds the exponential and
approximates series resistance; it is not an exact series-resistor diode solve.

A bounded, bracketed Newton solve closes the feedback equation for every
oversampled sample, including C6/C7 capacitor memory. An independently written
Python bisection solver checks the result. Mode smoothing interpolates diode
conductance contributions in this circuit, preserving a single capacitor state.
It does not crossfade separately processed output waves.

## Recovery, levels and oversampling

The nominal Q1 input resistance is 45.91 kOhm; the chosen linear Q2 source
resistance is 4.886 kOhm. These are independently checked by small-signal KCL.
The Q2 source calculation uses R12 plus nominal Q3 output resistance; Q3's
source equivalent assumes 30 kOhm source resistance. Q1's output equivalent
assumes a nominal 50 kOhm tone-wiper source. They do not track diode-dependent
output impedance, all tone positions, or complete bidirectional stage loading.

Q1's nominal unloaded voltage gain is approximately 4.20, with 2.34/5.98 V normalized positive/negative
headroom. Its nonlinearity runs inside oversampling along with the passive tone
network and C2. The tone solver uses double states and caches its Cholesky
factorization between control changes. This supports the higher processing
rate without rebuilding a matrix for every unchanged sample.

At the output boundary, 3 model volts correspond to 1 digital unit before the
unchanged Volume control. This common scale gives useful default headroom;
there is no per-mode normalization or safety clamp concealing overload. Boosted
Volume can still exceed 0 dBFS. Natural mode-level differences remain part of
the sound; RMS matching applies only to listening exports.

C2's discharge time constant is about one second. Short DC-decay windows that
were appropriate for the removed arbitrary 24 Hz blocker are insufficient.
Input dynamics record both the original early-tail window and a later settling
window; hot asymmetric/mode-change tests allow eight seconds of silence.

## Verification and listening

The local capture is `artifacts/clip-step4-final-20261115/`. It includes immutable
source hashes/archive, toolchain identity, raw float audio, gain/harmonic/stage
measurements, feedback traces, and circuit-validation reports. The production
engine and measurement adapter are compared sample by sample. Ten stage taps
now include oversampled `tone` and `recovery`; `downsampled` and final `output`
run at host rate. Absolute tap levels and stage order therefore differ from the
previous capture.

The checks cover:

- Independent DC/AC reference tests, 348 isolated input/Sustain responses, and
  the loaded Sustain endpoints.
- 75 base-rate tone curves and 189 additional tone responses at the actual
  oversampled rates from 88.2 to 768 kHz.
- 30,720 feedback samples over 15 cases: fixed silicon A; Off, Si, Ge and morphing
  B; source steps, resistance steps, both polarities and capacitor history.
- Mode headroom, lower Ge limiting level and asymmetric even harmonics, plus
  hot-input/mode-change safety and long DC decay at all host rates/qualities.
- 117 gain cases, 32 harmonics per case, and twelve unnormalized synthetic
  renders across modes, voicing and quality settings.

Measured results for this capture:

| Check | Result |
| --- | ---: |
| Production/tap adapter maximum difference | 2.98e-8 digital units |
| Feedback/bisection maximum output difference | 1.02e-10 model V |
| Feedback maximum voltage-equation residual | 9.99e-11 V; at most 9 iterations in the reference traces |
| Base-rate tone maximum gain/phase error | 0.0000105 dB / 0.000072 degrees |
| Oversampled tone maximum gain/phase error | 0.000000111 dB / 0.000000226 degrees |
| Isolated input/Sustain maximum gain error | 0.000969 dB |
| Combined nominal input/Sustain versus coupled reference | 0.00249 dB / 0.0147 degrees |
| Sine output RMS: Off / Si / Ge | −17.38 / −30.76 / −34.68 dBFS |

The sine-level comparison uses −24 dBFS input peak, Lo, Sustain 6.8, default
Tone/High/Volume and 48 kHz/4x. It is a signal-specific measurement; guitar mode
level differences vary with attack, frequency and input level. The short sine
capture includes C2 startup settling, so its mean is not a steady-state DC
measurement. Long-decay regressions separately check persistent DC.

See the capture's validation JSON and `clipping-review/clipping-model.png` for
numerical results. `comparison/` contains Step 3 versus Step 4 pairs;
`clipping-review/` contains matched mode comparisons. They use identical
synthetic plucks, **not recorded guitar**. Matching attenuates louder files only.
There is no physical-pedal match or listening approval implied by the checks.

The standalone and AU are rebuilt at their usual Debug paths. Quit a running
standalone instance before reopening it. The next gate is live guitar review:
compare Off/Si/Ge at the same settings, compensate using Volume for a second
listening pass, and check attack, cleanup, low notes and decay. Step 5's broader
CPU, aliasing, quality-switch and host qualification remains separate.

## Subsequent guitar feedback

The user tested this revision with guitar, heard the mode differences and
described germanium as smoother and darker. They then authorized Steps 5–7.
This is useful live-listening feedback; no recorded DI or hardware comparison
was added by that test.
