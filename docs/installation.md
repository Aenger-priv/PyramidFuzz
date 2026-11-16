# Install the macOS beta

Download the prebuilt ZIPs from the
[v0.1.0-beta.1 release](https://github.com/Aenger-priv/PyramidFuzz/releases/tag/v0.1.0-beta.1).
The AU ZIP is for Logic Pro, GarageBand and other Audio Unit hosts. The
standalone ZIP runs without a DAW. GitHub's automatically generated **Source
code** archives contain source, not the prebuilt plugin or app.

## Audio Unit

1. Quit Logic/GarageBand and unzip the **Universal-AU.zip** download.
2. In Finder, press **Command–Shift–G** and enter
   `~/Library/Audio/Plug-Ins/Components/`. Create the `Components` folder if it
   does not exist. Keep an existing Pyramid Fuzz component elsewhere if you
   want to retain it for rollback.
3. Copy the entire **Pyramid Fuzz.component** bundle into that folder. Replace
   the previous component as a whole; do not merge its internal files.
4. Reopen the host. In an audio track's Audio FX/plug-in slot, select
   **Audio Units → Anders → Pyramid Fuzz**. If it does not appear, check the
   host's Plug-in Manager and rescan Pyramid Fuzz.

Optional registration check in Terminal:

```bash
auval -v aufx PhFz AnDr
```

Diode **Off** keeps the fuzz active; use the host's plug-in bypass to compare
the complete effect. Start at **4x** oversampling and a modest Volume setting.

## Standalone app

1. Unzip **Universal-Standalone.zip** and copy **Pyramid Fuzz.app** into your
   Applications folder.
2. Open the app and allow microphone/input access when macOS asks. Select your
   guitar interface and the correct input/output channels in the audio settings.
3. Run one standalone copy at a time. Start with 4x oversampling; use a larger
   buffer if higher-quality processing overloads your audio setup.

## macOS download approval

This is an **ad-hoc signed, unnotarized test build**. Its local signatures pass
verification, but it has no Developer ID signature or Apple notarization.
Downloaded copies may be blocked by macOS; local build/host tests do not prove
that every downloaded copy will load on another Mac.

For a developer-verification warning, consult
[Apple's guidance for downloaded apps](https://support.apple.com/en-us/102445).
If you choose to allow this beta, macOS may offer an **Open Anyway** exception
in **System Settings → Privacy & Security** after the blocked attempt. Host/AU
behavior can differ. If loading still fails, report the exact alert and host
version. A Developer ID signed/notarized distribution is not provided by this
release.

## Compatibility and known test limits

- Universal binaries contain **arm64 and x86_64** code. The deployment target
  is macOS 12.0; runtime testing was on Apple Silicon with macOS 27.2.1 and
  GarageBand 10.4.14. Intel and older-macOS execution remain unverified.
- The AU passes `auval`. GarageBand editor, playback, bypass, export and saved
  settings were exercised. Logic supports the AU format, but this plugin has
  not yet been tested inside Logic.
- Use 4x for normal playing. The extreme 96 kHz/8x/stereo/32-frame corner is
  CPU-heavy, and the 4x extreme-bright/high-drive alias target remains unmet.
- Existing sessions keep their parameter values and use the revised sound.
  The previous sound model is not embedded in this beta.
- Native host bypass keeps the wet circuit running and does not save DSP CPU.
- A drawn GarageBand automation curve, complete keyboard traversal/Shift-drag,
  and a final guitar audition on the delivered build remain acceptance checks.

See the [delivery record](delivery-validation.md) and
[processing measurements](processing-quality-review.md) for detailed evidence.
For feedback, include your Mac/OS, host version, interface, buffer size,
oversampling and clipping mode, plus the steps that reproduce the issue.

## Files and verification

Each ZIP contains the complete app/component, these instructions, build
provenance and upstream JUCE notices. `SHA256SUMS.txt` on the release page records
the ZIP checksums. To check them, put both ZIPs and that file in the same folder
and run:

```bash
shasum -a 256 -c SHA256SUMS.txt
```

To uninstall, quit the host/app and remove only the Pyramid Fuzz component or
app you installed. To roll back, restore the previous complete bundle and
restart the host.
