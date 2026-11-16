# Step 5 — Processing, quality and host-state checks

The user auditioned Step 4 with guitar and reported audible mode differences,
with germanium smoother/darker, then authorized the remaining plan steps.
Step 5 preserves those circuit curves while preparing quality resources before
processing, fixing host latency and measuring the remaining quality tradeoffs.

## Changes

- Three circuit paths and all oversamplers are prepared in `prepareToPlay`.
  Quality changes select/reset an existing path after fading down, then fade up.
  They do not prepare filters, resize vectors or notify host latency in playback.
- Latency is fixed at **6 host samples** for all qualities/rates tested. Native
  integer latencies are 4/6/6 samples for 2x/4x/8x; 2x receives two padding samples.
  Impulse first moments independently verify the compensated delay. IIR phase
  still depends on frequency; a fixed report does not make every quality's
  phase response identical.
- Antiparallel silicon evaluation uses equivalent single-exponential algebra.
  A short step-limited warm Newton solve precedes the safeguarded fallback.
  The current/voltage law, capacitor equations and 48-iteration total budget
  remain unchanged. Independent bisection/circuit checks still pass. Across the
  before/after spectral captures, maximum latency-aligned sample difference is
  8.95e-8 digital units.
- Mono-to-stereo processing runs one circuit path, then copies the output.
  Stereo input retains two independent paths.
- State restore rejects the wrong root, missing/duplicate/unknown parameters,
  invalid numbers and out-of-range values before replacing current settings.
  Existing eight-parameter states retain their IDs, ranges, ordering and defaults.
- The processor handles empty/variable/oversized blocks using prepared views,
  rejects non-finite input samples before filter state, and reports a conservative
  ten-second tail for slow output coupling.

## Verification

Six CTest groups pass: DSP behavior, processor integration, independent reference
sanity, tone responses, clipping feedback and input responses. Processor tests
cover state round trips/rejection, mono/stereo/mono-to-stereo layouts, sample
rates, empty and oversized blocks, quality reversals, parameter changes,
re-preparation and chunk-size invariance.

The processor test measured zero C++ heap allocations during its callbacks,
including quality changes; a source audit also checks JUCE's stack-backed audio
views and string-reference parameter lookups. This is a scoped C++ allocation
guard, not a system-wide malloc/lock trace. It measured peak 0.471, maximum
sample step 0.029 and zero chunk-size error for the exercised automation signal.
No audible-click claim follows from these safety bounds alone.

The local before/after measurement bundles are:

- `artifacts/quality-step5-before-20261115/`
- `artifacts/quality-step5-after-20261115/`
- `artifacts/processing-step5-preserved-0661d95/`: previous source and measurement executable.

`tools/QualityMeasurements.cpp` records 108 wall-clock CPU cases across
44.1/48/96 kHz, 2x/4x/8x, mono/stereo, 32/128/512 frames and fixed/moving controls.
`tools/analyse_quality.py` produces the CSV, figures and explicit pass/fail
summary. The benchmark is an ordinary local process, not a realtime-priority
host; scheduling outliers are retained.

At 48 kHz/stereo/128 frames with moving controls, 4x p95 fell from 33.1% to 23.1%
of the buffer deadline. Across the tested 48 kHz/4x cases, the worst p95 was
25.2%, passing the 50% default-use target. The worst overall p95 was 99.3% and
p99 112.9%, at 96 kHz/8x/stereo/32 frames. **The 90% all-settings target is not
met.** Use 4x or a larger buffer for that corner; do not treat 8x as a free quality
upgrade in a busy session. No Intel CPU result is inferred from this arm64 run.

## Alias characterization and limits

The sine grid covers 54 ordinary captures plus 18 convergence references: all
host rates and modes, Hi/full Sustain/Tone/High, input peak 0.4, and frequencies
683/16384 and 2423/16384 of the host sample rate. The reference runs at four times
the host rate with 8x internal processing (32x overall), then uses an independent
1025-tap FIR decimator. A Blackman–Harris analysis masks true in-band harmonics
and measures other energy from 100 Hz to min(0.42 Fs, 20 kHz).

| Quality | Worst nonharmonic energy relative to signal | Worst absolute nonharmonic RMS across cases |
| --- | ---: | ---: |
| 2x | −27.0 dB | −42.6 dBFS |
| 4x | −44.8 dB | −67.1 dBFS |
| 8x | −65.7 dB | −99.4 dBFS |

Relative and absolute worst cases need not be the same signal. The 32x reference
floor was at most −126.2 dB relative. The **−60 dB relative 4x stress target is
not met**, particularly at the high test frequency with every brightness/drive
control raised. At the unchanged default Tone/High and normal guitar input this
is not a measured result; the synthetic stress result must not be generalized
to every playing setting or dismissed as inaudible. Use 8x for exposed bright,
high-drive sounds where CPU allows; 2x trades quality for CPU. This measurement
is not a complete two-tone, guitar-DI or perceptual alias assessment.

No extra voicing filter or automatic per-mode gain was added to conceal these
limits. The approved circuit sound remains the reference, and the report keeps
unmet stress targets visible. Further antialiasing methods would be a separate
sonic/modeling revision, with another guitar review.

## Final delivery repeat

The fresh universal Release measurement executable repeated all 108 CPU and 72
spectral cases under `artifacts/quality-step7-final-20261115/`. Default 48 kHz/4x
worst p95 was 24.2% of deadline; worst overall p95/p99 were 91.4%/97.4%.
The 90% all-settings target still fails. Spectral extrema remain those tabulated
above, and the 4x stress target still fails. These benchmark results measure
`FuzzEngine`, not the complete host/UI or native-bypass bookkeeping. The
[delivery record](delivery-validation.md) records the separate actual host tests.
