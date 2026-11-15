# Implementation review and proposed revision plan

Date: 2026-11-15. Reviewed baseline: `b98a3be` on `main`.
Status: Step 1 was approved and implemented. Its [baseline and proposed limits](baseline-review.md)
are ready for review; Steps 2–7 await separate approval.

## Direction

Keep the existing JUCE/AU foundation, modular signal path, APVTS parameters,
oversampling options, artwork, and opaque circular controls. Improve circuit
correspondence and predictable control behavior before doing final sound tuning.
The target remains a practical schematic-guided model, without full transistor
simulation in the real-time plugin.

The source of truth is the repository's `Pharaoh Big Muff Clone Schematic.jpg`.
Use role names alongside its transistor numbering: Q4 input preamp, Q3 first
clipping stage, Q2 selectable clipping stage, Q1 recovery stage.

Keep C8 at 22 nF, following the clarification already recorded in
`schematic-analysis.md`. The drawing's caption and the
[original schematic author's page](https://www.bigmuffpage.com/Big_Muff_Pi_versions_schematics_part4.html)
say 2.2 uF, while its component label says 0.022 uF. Record this as an explicit
chosen variant; do not silently change the value during tuning.

## Evidence from this review

- Rebuilt Debug DSP tests, AU, and standalone successfully. After rebuilding,
  CTest passed 1/1. There are upstream JUCE/macOS SDK deprecation warnings.
- Current tests exercise a 110 Hz sine at 44.1/48/96 kHz through all clipping
  and oversampling modes, check finite/bounded output and identical-channel
  agreement, and check the clipping knee. They do not establish circuit accuracy,
  automation safety, measured aliasing, or processor state/routing correctness.
- Inspected the supplied schematic and all plugin/DSP sources. No implementation
  files were changed for this review.
- Tested the actual `ToneStack.h` in an isolated C++ harness at 48 kHz, using
  minimal JUCE math shims. With Tone=1, its steady-state 1 kHz gain changes from
  -3.59 dB at High=0 to -55.94 dB at High=1. This is a tone-block measurement,
  not an end-to-end recording or a comparison to a physical pedal.
- Live standalone UI inspection timed out. UI findings below are based on source;
  runtime visual and interaction checks remain an acceptance gate.

## Findings

### 1. Tone network topology needs correction — highest priority

`src/dsp/ToneStack.h:42-64` connects both C9 and C8 in series from the collector
to separate tone arms, then places R5 and R8 from those arms to ground. In the
drawing, the C9/R5 branch is high-pass, while the R8/C8 branch has R8 in series
from the collector and C8 to ground. The implemented second branch is therefore
not the drawn low-pass branch. A nodal solver cannot compensate for that wiring.

The High mapping also reduces R8 as High increases, contrary to its comment
about reducing shunt loss. Tone=0 selects the C9 high-pass arm, so the advertised
dark-to-bright direction needs explicit verification and mapping. Source and
recovery impedances are assumptions that must be documented and checked.

### 2. Sustain and input loading are still tuned approximations

The drawing places R24 (100 kOhm) after the input preamp, with R23 providing a
1 kOhm lower leg. Current Sustain changes drive inside the preamp and both
clipping stages. This changes the gain distribution and prevents the control
from behaving like the drawn interstage divider.

`InputVoicingStage` uses fixed 120/48 Hz high-pass filters, fixed low-pass filters,
and 0.94/0.67 gain factors. These do not calculate the C1, 39/390 kOhm input
network and preamp loading claimed by the architecture document. The actual
loading assumptions should determine the response, with artistic tuning marked
separately. A plugin cannot reproduce passive pickup interaction upstream of an
audio interface merely by adding an input filter.

### 3. Clipping and coupling need a traceable calibration

The clipping stages use feed-forward transfer functions and fixed filters rather
than a derived approximation of the feedback networks. They may sound useful,
but their gain, bandwidth, and thresholds have no current measurement reference.
The germanium branch in the drawing has one diode in one direction and two in
the other; a quadratic skew is only a loose approximation of that asymmetry.

`ClippingStageB` describes its extra 24 Hz high-pass as C3, while `ToneStack`
already includes C3 after the Tone wiper. Resolve this duplicate attribution and
any intended additional DC blocking. The output stage's gain and saturation also
need calibration against passive loss and recovery headroom.

### 4. Quality switching and CPU work need measured checks

The tone block rebuilds and solves a 5x5 system for each sample and channel,
including when controls are stationary. Cache or reduce this only after response
tests exist, preserving stable behavior while controls move.

Quality switching calls stage preparation, coefficient calculation, and resets
from the processing path; latency changes notify the host from `processBlock`.
Audit this path for allocation, locking, CPU spikes, timing, and discontinuities.
The output `tanh` operates after downsampling; measure its alias contribution
before deciding whether to relocate or remove that saturation.

### 5. UX and repository maintenance can be improved incrementally

- Preserve the recent knob and label work. Add explicit value units/formats,
  default reset gestures, concise control help, and verified keyboard operation.
- Explain that Clipping Off removes the second diode network, not the effect.
  Consider a separate latency-aligned effect bypass for useful comparisons.
- Replace linear-amplitude meter presentation with useful dB scaling, timed peak
  decay and clip hold. Current decay depends on callback frequency, and the UI
  clamps the display at unity without a separate overload indicator.
- Check Hi/Lo and clipping selection ergonomics in the actual UI; segmented
  controls are an option, not a pre-approved redesign. Verify layout, focus,
  display scaling, contrast, and numeric editing before changing decoration.
- Keep the small existing DSP directory. Introduce named circuit constants and a
  reusable DSP build target where useful, reducing duplicated test dependencies.
- Remove tracked Python bytecode, ignore caches, and repair or document the art
  script's missing default prompt file. Keep asset generation outside the build.
- Reconcile the stale schematic analysis with architecture and code. Remove the
  unused `HighControlFilter` from the engine if it has no retained purpose.

## Proposed implementation sequence

Each numbered step is a separate reviewable change. Build AU/standalone and run
relevant checks after each major change. Update README and architecture as their
claims change. Stop at the review gate before starting the next numbered step.

### Step 1 — Freeze the baseline and establish reference checks

- Create a `codex/` revision branch and preserve the current source/build identity
  and comparison renders. Keep JUCE at the existing pin during this revision.
- Define the circuit variant, node/component mapping, source/load assumptions,
  digital-to-circuit signal scale, and intended control directions.
- Add a repeatable offline measurement tool for tone responses, gain-versus-input,
  harmonic spectra, DC/decay, and stage levels. Store settings and build identity
  with results. Use an independently assembled passive AC reference for tone.
- Capture baseline synthetic signals and available dry guitar recordings. Agree
  on input levels, comparison settings, and acceptance limits before tuning.
- Decide how existing saved sessions should sound after DSP revisions. Preserve
  parameter IDs, ranges, and choice ordering; add a versioned sound model only
  if old-session sonic compatibility is required. An archived build alone does
  not make new binaries render old sessions identically.

Gate: reproducible baseline, independent tone reference, and an agreed session
compatibility policy. No sound changes in this step.

### Step 2 — Correct and validate Tone/High

- Correct the R8/C8 topology and control directions; retain the chosen C8 value.
- Check source/load assumptions, pot endpoints, passive loss, and C3 response.
- Compare a grid of Tone/High settings against the independent AC reference at
  44.1/48/96 kHz, including endpoints and low-frequency behavior.
- Add regressions that detect the current unintended near-mute corner and wrong
  control direction. Preserve legitimate passive losses in the reference model.

Gate: response plots meet agreed reference tolerances; stable automated sweeps;
audition the same dry guitar through baseline and corrected versions.

### Step 3 — Correct input conditioning and Sustain placement

- Derive the Hi/Lo input network with explicit preamp input-loading assumptions.
- Model R24/R23 as an interstage divider and restore fixed, documented preamp
  gain/headroom. Work through coupling and feedback bandwidth one block at a time.
- Check low-Sustain cleanup, full-Sustain drive, bass retention, and hot inputs.

Gate: measured level/response curves and level-matched listening show useful
control travel, predictable cleanup, and no avoidable gating or unstable decay.

### Step 4 — Calibrate clipping and recovery

- Establish per-stage gains and voltage scaling before adjusting diode knees.
- Approximate feedback-shaped bandwidth, retain the gain stage in Off, and model
  separate positive/negative germanium knees informed by the diode arrangement.
- Resolve coupling/DC blocking, test asymmetric shaping at extreme inputs, and
  calibrate recovery and default output headroom without hiding problems in clamps.
- Preserve natural mode-level differences first. Treat optional loudness matching
  as a separate UX decision, rather than silently changing the modeled circuit.

Gate: transfer/spectrum/DC/decay measurements plus same-DI auditions across all
modes. Prefer improvements supported by both evidence and listening; preserve
an earlier version when a proposed change is only different.

### Step 5 — Make processing and quality changes predictable

- Measure aliasing at 2x/4x/8x, including the base-rate recovery nonlinearity.
- Profile representative mono/stereo instances at 44.1/48/96 kHz and small buffers.
  Optimize tone calculations only against the established response tests.
- Prepare rate-specific resources outside processing where practical. Test rapid
  mode reversals and the complete fade/reset sequence. Evaluate fixed maximum
  latency with compensation versus safe dynamic host updates.
- Add processor-level tests for state round trips, invalid state, supported buses,
  mono-to-stereo, oversized/variable/empty blocks, and parameter changes.

Gate: agreed CPU/alias limits, measured latency, no audio-thread allocation in
normal processing, and clean switching/automation at tested block sizes.

### Step 6 — Improve playability and control clarity

- Implement agreed value formatting, reset/help behavior, and metering changes.
- Review effect bypass, selector presentation, and editor scaling as individual
  choices. Preserve the established artwork, opaque knobs, and readable labels.
- Verify the live editor in standalone and GarageBand with mouse, keyboard,
  numeric entry, host automation, reopened sessions, and relevant display scales.

Gate: visual captures and a short interaction checklist; confirm changes while
playing guitar, including the distinction between diode Off and effect bypass.

### Step 7 — Consolidate and validate delivery

- Complete the small build/asset/documentation cleanup identified above.
- Rebuild from a fresh build directory; verify the unchanged JUCE pin and document
  the actual toolchain/deployment targets. Check architecture support explicitly.
- Run AU validation and a GarageBand session covering insert/load, mono/stereo,
  playback, automation, bypass, save/reopen, latency, and offline export.
- Deliver concise before/after measurements, listening files, known limitations,
  and rollback instructions. Commit approved steps separately; push only when
  requested or authorized for implementation.

Gate: passing build/tests and AU validation, a completed GarageBand checklist,
and approval of the resulting sound and interface.

## Current review gate

Step 1 is complete without DSP changes. The user chose revised sound for existing
sessions with the original build retained separately, rather than a legacy model.
Review the baseline, independent reference, source/load assumptions and proposed
measurement limits before approving Step 2's tone-network correction.
