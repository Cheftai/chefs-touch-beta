# Chef's Touch — Beta 1

Vocal processing and stereo mastering in one plugin, with 22 factory presets and a chef mascot that reacts to your vocal.

**[Download Beta 1](https://github.com/Cheftai/chefs-touch-beta/releases/tag/v0.3.0-beta.1)** · **[Mac build status](https://github.com/Cheftai/chefs-touch-beta/actions)** · **[Report a bug](https://github.com/Cheftai/chefs-touch-beta/issues)**

## Downloads and compatibility

| Platform | Format | Status |
| --- | --- | --- |
| Windows 64-bit | VST3 | Available in the Beta 1 release; compilation and existing host tests passed |
| Mac M1 and other Apple silicon; Intel | Audio Unit, macOS 12+ | Remote build and validation in progress; download pending |

This is a private beta. Testers must be invited to this repository and accept the invitation before accessing downloads. The intended first cohort is 25 people. **Plugin activation does not currently enforce a 25-person limit.**

## Install on Windows

1. Open the Beta 1 release and download `ChefsTouch-0.3.0-beta.1-Windows-VST3.zip`.
2. Extract it and copy the entire `Chefs Touch.vst3` folder to `C:\Program Files\Common Files\VST3`.
3. Rescan plugins in your DAW and load Chef's Touch as an effect on a vocal or master track.

## Install on an M1 Mac with limited storage

Wait for a validated Mac AU download to appear in the release. Download and extract that ZIP, then move `Chefs Touch.component` into `~/Library/Audio/Plug-Ins/Components` with your DAW closed. Create the folder if necessary, restart the DAW and rescan plugins. Delete the downloaded ZIP after installation.

**You do not need Xcode, JUCE, CMake or this source repository on your Mac.** GitHub builds the plugin remotely. The intended Mac build includes native arm64 and Intel support. It is ad-hoc signed; Apple notarization is not included.

## Included in this beta

- **22 factory presets:** Vocal and Master recipes for Trap, Pop, Rock, R&B, Hip Hop, Acoustic, EDM and Jazz Soul, plus six original recipes. Presets are starting points; choose the song's actual key and scale and adjust levels for your recording.
- **Vocal chain:** Natural and Hard Lock pitch correction, key and scale selection, retune, humanize, tune amount, rumble filtering, three-band noise expansion, three-band EQ, compression, de-essing, saturation, delay and reverb.
- **DAW tempo delay:** In Settings, select Delay Sync from 1/16 through 1/2 notes, including dotted eighth and quarter notes. When the host supplies no valid tempo, the manual Delay time is used. Host-specific tempo behavior still needs DAW testing.
- **Master chain:** EQ, compression, saturation, stereo width and linked lookahead limiting with estimated inter-sample peak detection.
- **Controls and feedback:** Input/output gain, vocal wet/dry, input/output and gain reduction meters, pitch readout, bypass with latency compensation, Standard/High quality, preset save/load and DAW state recall.
- **Beat analysis and coaching:** Import an instrumental to estimate BPM and major/minor key, apply its key to the vocal settings, and use the local mix coach for adjustment guidance.
- **Interface:** Drawn kitchen tiles, gold trim, decorative flame border, vocal-level mouth animation and an in-tune chef celebration. The mouth does not recognize lyrics or phonemes.

Both processing modes report approximately 25 ms latency. Vocal pitch correction expects an isolated, monophonic voice. Noise cleanup reduces noise in pauses; it does not separate a voice from a full mix. The limiter's estimated peak ceiling is not broadcast-certified.

## Requested features still to come

Manual pitch editing, stem separation, LUFS metering, signed installers, Apple notarization, automatic updates and enforced tester activation are **not included**. No AAX or standalone application is included. A Mac download is only ready after its build and validation succeed.

## Help test the beta

Use a duplicate project. Check plugin loading, resizing, automation, save/reopen, bypass timing, mono/stereo operation, quiet passages, loud choruses and CPU use. Cross-DAW listening evaluation is still needed.

[Open an issue](https://github.com/Cheftai/chefs-touch-beta/issues/new) with your OS, CPU, DAW and version, sample rate, buffer size, preset, steps to reproduce, expected result and actual result. Attach a screenshot or short audio example if useful. See [the beta readme](Beta/BETA-READ-ME.txt) for additional details.

## Build from source

CMake uses the vendored JUCE checkout when present, otherwise it downloads the pinned JUCE revision. Source builds require CMake 3.22+, a C++20 toolchain and the platform SDK. This is only for development; testers should use the release downloads.

```powershell
cmake -S . -B Build/windows -DCHEFS_TOUCH_BUILD_TESTS=ON
cmake --build Build/windows --config Release --target ChefsTouchValidation --parallel
ctest --test-dir Build/windows -C Release --output-on-failure
```

On a Mac with Xcode and CMake, `bash Scripts/build-macos.sh --install` builds and validates the universal AU. The GitHub Actions workflow runs this remotely for the low-storage installation path.
