# AGENTS.md

## Project goal
Build a JUCE-based AU fuzz plugin for macOS/GarageBand inspired by a modified 47 Ram’s Head Big Muff style circuit.

## Working rules
- Always inspect the repository before changing files.
- Work in small, verifiable steps.
- Keep the project buildable after each major change.
- Prefer CMake + JUCE.
- Separate DSP code from UI code.
- Use AudioProcessorValueTreeState for parameters.
- Build and fix compile errors after major edits.
- Update README.md and docs/architecture.md as the implementation evolves.
- Do not use placeholder DSP if it can be reasonably implemented.
- Do not attempt full transistor-level circuit simulation in v1.
- Use schematic-guided DSP blocks with oversampling.
- Prioritize AU support for GarageBand.
- Keep code clean, documented, and maintainable.

## Sonic priorities
- Thick sustain
- Big Muff family voice
- Strong but controlled harmonic density
- Distinct silicon/germanium/off clipping modes
- Well-implemented tone stack
- Reduced aliasing and digital harshness

## Product requirements:
Build a white-box-inspired DSP model of a modified Ram’s Head Big Muff style fuzz pedal based on the following conceptual circuit structure:

1. Input stage / voicing
- Include input gain trim.
- Include a hi/lo voicing switch inspired by the original schematic’s hi-lo input network.
- Model the input as a pre-filter and gain-conditioning stage rather than literal transistor simulation.

2. Pre-gain stage
- Model the first transistor-like stage as:
  - pre-emphasis / coupling filter behavior
  - gain
  - mild soft saturation
- Make it tunable in code.

3. First clipping stage
- Implement a gain stage with band-limited nonlinear clipping.
- Include inter-stage filtering around the nonlinearity.
- This stage should contribute strong sustain and fuzzy compression.

4. Second clipping stage with selectable diode mode
- Add a 3-way clipping mode:
  - Off / bypass-like
  - Silicon
  - Germanium
- Each mode must have audibly distinct behavior:
  - Off: more open and less compressed
  - Silicon: harder, tighter, brighter clipping
  - Germanium: softer, squishier, lower-threshold clipping
- Implement these as tuned nonlinear transfer functions and/or threshold/knee variants, not literal diode equation solving unless that proves simple and stable.

5. Tone stack
- Build a Big Muff-inspired passive tone stack approximation.
- Include a main Tone control that sweeps between darker/bassier and brighter/thinner responses.
- Preserve the recognizable mid-scooped family character.
- Implement the tone section carefully. This is a high-priority sonic block.

6. High control
- Add a secondary “High” or presence-style control inspired by the schematic’s modified tone network.
- This should adjust upper-frequency emphasis after or around the tone stack without making the plugin harsh.

7. Output stage
- Add a final recovery/output stage with optional mild saturation and a Volume control.
- Ensure output level is manageable and does not clip internally by default.

DSP requirements:
- Use oversampling for the nonlinear stages.
- Minimum: 4x oversampling.
- Prefer an option for 2x / 4x / 8x if practical.
- Add anti-alias filtering appropriate for the oversampling path.
- Avoid obvious digital fizz and aliasing artifacts.
- Keep CPU use reasonable for real-time guitar playing.

Controls / parameters:
Create these user-facing parameters at minimum:
- Input
- Sustain
- Tone
- High
- Volume
- Voicing switch: Hi / Lo
- Clipping mode: Off / Silicon / Germanium
- Oversampling: 2x / 4x / 8x (or 4x only if needed initially)

Optional advanced parameters if useful:
- Output trim
- Noise gate (disabled by default)
- Internal bias or character control hidden as advanced setting
- Quality mode

Plugin behavior:
- Mono in / stereo out is acceptable, but mono in / mono out is also acceptable for v1 if JUCE routing is simpler.
- Prioritize guitar-friendly behavior.
- Avoid denormals, zipper noise, unsafe parameter changes, and obvious clicks when toggling modes if possible.
- Smooth continuous parameters where appropriate.

UI requirements:
- Build a clean, usable GUI in JUCE.
- Use a stompbox-inspired layout, not photorealistic skeuomorphism.
- Show all main controls clearly.
- Include a clear title, clipping mode selector, voicing selector, and oversampling selector.
- Add input/output level indicators if practical.
- Focus on readability and function over decoration.

## Milestones
1. Scaffold JUCE plugin project
2. Implement DSP blocks
3. Add parameters and smoothing
4. Add oversampling
5. Add GUI
6. Build on macOS
7. Write docs