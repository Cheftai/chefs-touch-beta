## Chef's Touch 0.3.1 — Beta 2

Adds manual pitch regions and output LUFS metering to the shared Windows/Mac plugin code.

- **Manual pitch:** Settings > Manual pitch edits. Enter start seconds, end seconds and MIDI target note; Apply and enable the timeline. Up to 64 ordered regions, DAW song time, Vocal mode. Numeric editor; no graphical piano roll.
- **LUFS:** Momentary, short-term and gated integrated output loudness; reset in Settings. Integrated gate uses 0.1-LU bins. Not broadcast certified.
- **Stem companion:** Offline Windows CPU separation into vocals, drums, bass and other via Demucs. Transfer the resulting WAVs to Mac; your Mac needs no AI model. First run installs substantial dependencies on Windows. End-to-end inference is not yet verified here.

Windows binaries are attached only after local validation passes. Mac AU is attached only after its remote build, Apple auval and plugin host tests pass. Read BETA-2-READ-ME.txt before testing.

Signing/notarization, automatic updates and runtime activation remain absent. The 25-person usage limit is not enforced.
