# Steps 5–7 delivery record

Validated 2026-11-15/16 on Apple Silicon, macOS 27.2.1 (26B131), GarageBand
10.4.14, Xcode 27.2 (27B52), Apple clang 21 and SDK 27.2. JUCE remains at
`501c07674e1ad693085a7e7c398f205c2677f5da`; its checkout is clean.

This record captures the local validation before publication. The implementation
and diagrams were subsequently merged into `main`; see the
[beta installation guide](installation.md) for prebuilt distribution. Final
acceptance remains qualified by the manual checks and stress exceptions below.

## Delivered changes

- Prepared 2x/4x/8x circuit paths, fixed six-sample host latency, safer state
  loading, cheaper equivalent diode evaluation and processor integration tests.
- Clear parameter units, reset/fine-control help, keyboard focus, mode help and
  sample-timed dBFS meters, preserving the existing controls and artwork.
- Native host bypass now preserves six samples of delay and supported bus
  routing, with a 5 ms transition. Wet processing stays warm during bypass, so
  bypass does not save DSP CPU. No new parameter or legacy sound model is added.
- Removed unused contour/biquad/transfer code and tracked Python bytecode,
  restored the art helper's missing default prompt, simplified current docs,
  and made the default macOS deployment floor explicit at 12.0.

Steps 5 and 6 are separate commits, `5f6bd83` and `553e49c`. The final cleanup,
host-bypass correction and delivery record form the following Step 7 commit.

## Build and automated checks

A fresh Xcode build directory, `build/validated-release`, produced universal
arm64/x86_64 Release standalone and AU products. Both slices declare macOS 12.0
as the minimum target and SDK 27.2. Both bundles pass strict code-signature
verification. These are local development products, not a notarized release.
The usual `build/` Debug and Release app/AU products were also rebuilt.

All six CTest groups pass (29.56 seconds): DSP, processor integration,
independent references, tone responses, clipping feedback and input responses.
Processor checks include independent stereo, mono expansion, exact native
bypass delay, state rejection/recall, variable/empty/oversized buffers, quality
reversals and continuous-parameter changes. The exercised callbacks report zero
C++ heap allocations, peak 0.4703, maximum sample step 0.0289 and zero chunk-size
error. This guard does not establish a system-wide absence of malloc or locks.

The freshly installed AU passes `auval -v aufx PhFz AnDr`, including scheduled
and ramped parameter tests. Its executable hash matches the fresh universal AU.
Intel runtime testing was unavailable: Rosetta is absent. Neither Intel runtime
support nor execution on macOS 12 has been established by cross-compilation.

## Native application and GarageBand

| Check | Observed result |
| --- | --- |
| Standalone startup and reopening | Passed after Scarlett USB reconnect; see incident below. |
| Standalone interaction | Arrow adjustment, percentage typing, default reset, mode/help and quality selection passed. |
| Standalone setting recall | All eight captured guitar settings matched after closing/reopening. |
| AU insertion/editor | Updated editor loaded and displayed correctly at Retina scale. |
| AU control/choice changes | Distinct Input/Sustain/Tone/High/Volume values, Lo/Ge/4x accepted. |
| GarageBand playback | Imported five-second synthetic region played on Audio 1 through the AU. Other synthetic tracks muted; input monitoring off. |
| Native bypass/export | Host power toggled off/on; separate wet and bypass WAV exports completed. |
| Project close/reopen | All eight settings and enabled effect recalled exactly. |
| Drawn host automation curve | Not completed: the app-control tool misdirected Tracks-area clicks. Manual follow-up remains. |
| Mono/stereo layouts and delay | Processor tests and AU validation pass; each layout was not separately exercised in the GarageBand UI. |
| Final guitar acceptance | Earlier Step 4 guitar feedback is positive; a new guitar pass on this delivery remains. |

The separate project is
`artifacts/delivery-steps5-7-20261115/Pyramid Fuzz Validation.band`.
Its recalled settings are Input +2 dB, Sustain 7.4, Tone 57.8%, High 63.2%,
Volume −9 dB, Lo, Germanium, 4x. It contains only the synthetic test stimulus;
existing music projects were left unchanged.

`garageband-wet.wav` and `garageband-bypass.wav` are five-second 44.1 kHz,
stereo, 16-bit host exports. Both are non-silent and differ in waveform/RMS.
Both peak at 0.9885, so these host exports do not establish unity-gain bypass,
absolute output calibration or a level-matched sonic preference. Exact dry
delay/gain is checked in the processor harness. The independent measurement
renders retain raw levels and explicitly label any listening-only matching.

The original standalone guitar settings were restored separately:
Input +8.01 dB, Sustain 5.59, Tone 44.5%, High 58.1%, Volume −13.86 dB,
Hi, Germanium, 4x. All eight matched the saved state after reopening.

### Audio-device incident

Several standalone copies blocked during CoreAudio device startup/shutdown.
A sampled stack stopped in `AudioDeviceStart`/HAL locking, before normal DSP
playback. With explicit user approval, only the verified Pyramid Fuzz processes
were force-quit. A fresh launch still blocked until the user unplugged/reconnected
the Scarlett Solo. Subsequent standalone interaction, close and reopen passed.
This does not establish a general fix for the driver/startup failure; no driver,
system audio daemon or JUCE device-startup code was changed.

## Circuit and quality evidence

`artifacts/measurement-delivery-final-20261116/` freezes final source hashes,
measurement executable identity, raw tone/input/clipping/stage/harmonic/decay
captures and synthetic audio. All three independent capture validators pass.
Tone's maximum magnitude error is 0.000011 dB; coupled input response error is
0.00249 dB; feedback output error is below 1.1e−10 model volts. These quantify
agreement with the declared reduced model, not a measured physical pedal.

Comparison files retain the user-auditioned Step 4 source separately. The
equivalent nonlinear solve changes spectral-capture samples by at most about
8.95e−8 digital units after latency alignment; model laws and nominal component
values are preserved in Steps 5–7. No extra voicing or automatic diode loudness
matching was introduced.

`artifacts/quality-step7-final-20261115/` repeats 108 CPU and 72 spectral cases.
Worst default 48 kHz/4x p95 is 24.2% of its deadline (50% target passes).
Worst overall p95 is 91.4% (90% target fails), at the demanding 96 kHz/8x/stereo/
32-frame corner. These are local engine benchmark timings, not complete host
realtime guarantees. The 4x extreme-bright/drive alias target also remains unmet:
worst relative nonharmonic energy is −44.8 dB against a −60 dB target; 8x reaches
−65.7 dB. See the [quality review](processing-quality-review.md) for the precise
protocol and absolute levels. Use 4x for playing and a larger buffer for costly
8x settings. A further antialiasing revision requires a separate sound review.

## Products, records and rollback

- Latest validated app:
  `build/validated-release/PyramidFuzz_artefacts/Release/Standalone/Pyramid Fuzz.app`.
- Matching AU: `build/validated-release/PyramidFuzz_artefacts/Release/AU/Pyramid Fuzz.component`.
- Installed AU: `~/Library/Audio/Plug-Ins/Components/Pyramid Fuzz.component`.
- Local delivery archive/index: `artifacts/delivery-steps5-7-20261115/`.
- Original installed AU: `installed-backup/previous-installed.component` and
  `installed-backup/previous-installed-au.zip` within that delivery folder.
- Auditioned Step 4 evidence: `artifacts/clip-step4-final-20261115/`;
  previous source/executable: `artifacts/processing-step5-preserved-0661d95/`.
- Original baseline source/build records: `artifacts/baseline-b98a3be-20261115/`.

Quit the standalone/host before changing products. To roll back the installed
AU, retain the current component elsewhere and copy the preserved original
component back to the installed path, then restart GarageBand and run AU
validation. Avoid merging two component directories. The archived executable
and source hashes distinguish versions even though their bundle names match.

Final manual acceptance: replay a drawn Tone/High automation curve in the saved
GarageBand test project and do a short guitar pass on the delivered Release
build, including mode/quality/bypass transitions. Full focus traversal and
Shift-drag should also be checked manually. These are explicitly open checks,
not implied by compilation, previews, AU validation or processor tests.
