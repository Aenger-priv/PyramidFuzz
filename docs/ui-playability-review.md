# Step 6 — Control clarity and meters

The established black/gold artwork, opaque circular knobs, label placement,
choice ordering and eight parameter IDs/ranges/defaults are preserved.

## Changes

- Input/Volume show signed, one-decimal dB values. Sustain shows one decimal on
  its existing 0–10 scale. Tone/High show percentages; numeric entry such as
  `44.5%` maps to the original stored value 0.445. Host automation values remain
  in their existing ranges; display formatting does not migrate session state.
- The existing JUCE attachment's double-click default reset is now explained.
  Shift-drag uses a low-sensitivity velocity mode for fine adjustment. Controls
  accept keyboard focus, with a visible knob focus ring and explicit traversal
  order: three selectors, then Input through Volume. Arrow keys adjust sliders.
- Tooltips explain drive, Hi/Lo level differences, passive Tone/High interaction,
  diode modes and quality/CPU tradeoffs. The footer updates with the current
  diode mode, including **Off keeps the amplifier and first fuzz stage active**.
- Input/output meters display peak dBFS across a −60 to 0 dB bar, with a −18 dB
  marker and numeric values. Attack is immediate, release has a 400 ms time
  constant, and an overload indicator holds for one second after reaching
  0 dBFS. Values above unity remain visible numerically; no audio limiter is
  inserted. Input metering is before Input trim; output is after Volume.

A separate custom effect-bypass parameter was considered and deferred. The
existing host bypass remains the complete-effect comparison control. Adding a
second bypass/state/control path was unnecessary to resolve the diode-Off
ambiguity and would require its own aligned dry-path qualification. The
standalone does not acquire an effect-bypass button in this step.

The fixed 760×500 logical editor size is retained. Native display scaling is
handled by JUCE; arbitrary window resizing is not advertised. The established
selectors remain in place, rather than replacing them with a new control style.

## Verification

Processor tests pass, including meter release/hold across 44.1/48/96 kHz and
32/128/512-frame callbacks, value-format/parser round trips, state persistence,
processor routing and allocation guards. Normal and deliberately overloaded
component previews were rendered at 1x and 2x and visually inspected:
`artifacts/ui-step6-20261115/editor-{default,overload}-{1,2}x.png`.
The overload preview shows the red Out indicator and the actual over-range dBFS
value; the normal preview preserves the artwork and readable controls.

Native standalone interaction checks are pending restart: the computer-use tool
timed out with two standalone copies running, and the user was asked to close
them. Keyboard/numeric/reset checks and GarageBand interaction must be recorded
in the final delivery review after observing the running application. Component
previews and unit tests alone do not establish those interaction results.
