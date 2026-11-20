# Chef's Touch

A JUCE effect for vocal tuning and mixing, or stereo mastering. The Windows build is a 64-bit VST3. The same project enables an Audio Unit effect target on macOS.

## Use it in a mix

- **Vocal** works with an isolated, monophonic performance. Pick the song's Key and Scale, then shape the correction with Retune, Tune Amount and Humanize. Chromatic mode can target any semitone. For an obvious hard-tune effect, select Hard Lock, use a short Retune, high Tune Amount, low Humanize and the song's scale. Hard Lock applies a shorter retune time constant than Natural at the same setting. The chain includes input trim, pitch correction, noise cleanup, three-band EQ, compression, de-essing, saturation, delay and reverb. Wet / Dry blends the processed signal with latency-aligned dry audio, and Output trims the blend.
- **Analyze beat** estimates the BPM and major or minor key from the first minute of a locally selected instrumental audio file. **Use key** writes the detected key and scale to the Vocal parameters saved in the DAW project. BPM is a reference for the user. Tempo estimates can land at half or double time, and closely scored keys are marked **check**.
- **Noise Clean** applies a rumble filter and three-band downward expansion. Start with Noise Floor below the quietest sung words. Raise it until the noise drops in pauses, then use Noise Clean to blend the result. Quiet fan or room noise and rumble can be reduced. Loud bleed or noise occupying the same frequencies as the singer can remain.
- **Master** includes input trim, three-band EQ, compression, saturation, mid/side width, output trim and a linked lookahead limiter. Width at 0x sums the output to mono and 1x preserves the input side level. The limiter estimates inter-sample peaks with 4x interpolation in Standard and 8x in High. Its ceiling is an estimate and has not been certified against a broadcast true-peak test suite.
- **Settings** shows reported latency, switches processing quality, toggles mascot animation, and opens and saves reusable `.chefpreset` files. Factory presets cover both modes. FL Studio project state stores parameter values and the selected factory preset.
- **Learn** gives step-by-step starting values, knob adjustments, and listening checks for tuning, noise cleanup, EQ, dynamics, space, and mastering. Live feedback is calculated locally.
- The chef opens and closes his mouth with the isolated vocal level. A well-tracked note within 12 cents of the selected scale target for about half a second cues the sauce pour and pan flare. This is a pitch milestone.

Both modes report 25 ms of latency, rounded up to a whole sample. Bypass keeps the same delay for host delay compensation. Vocal correction targets an isolated voice in roughly the 65–950 Hz analysis range.

## Windows build and validation

Run the following in PowerShell from the project root. The build uses the vendored JUCE checkout and keeps generated files under `Build/windows`.

```powershell
$vs = & "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -property installationPath
$bin = Join-Path $vs "Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin"
New-Item -ItemType Directory -Path Build/temp -Force | Out-Null
$env:TEMP = (Resolve-Path Build/temp).Path
$env:TMP = $env:TEMP

& "$bin/cmake.exe" -S . -B Build/windows -G "Visual Studio 18 2026" -A x64 -DCHEFS_TOUCH_BUILD_TESTS=ON
& "$bin/cmake.exe" --build Build/windows --config Release --target ChefsTouchValidation --parallel
& "$bin/ctest.exe" --test-dir Build/windows -C Release --output-on-failure
```

The bundle is `Build/windows/ChefsTouch_artefacts/Release/VST3/Chefs Touch.vst3`. The file name omits the apostrophe for MSBuild compatibility. The host displays **Chef's Touch**. Install the entire bundle in `C:\Program Files\Common Files\VST3`. In FL Studio, open **Options → Manage plugins**, enable **Verify plugins**, then select **Find installed plugins**. Load it in an FX slot on your vocal insert or master mixer track.

The smoke test opens the released bundle through a JUCE VST3 host. It exercises pitch correction, Vocal effects and noise cleanup, Master limiting and width, mono and stereo layouts, automation, latency, bypass, state recall and editor creation. A generated instrumental fixture checks BPM and key analysis.

## macOS build and Logic Pro

To save disk space, download **Chefs-Touch-Logic-Pro-AU** from the most recent successful **Build Logic Pro Audio Unit** workflow run. Extract the inner AU ZIP, then copy `Chefs Touch.component` to `~/Library/Audio/Plug-Ins/Components` with Logic closed. You only need the component on your Mac.

Copy the source tree or extract the Mac source archive on a Mac with full Xcode and CMake 3.22 or newer. Close Logic Pro before replacing an installed component. From the extracted directory, run:

```sh
bash Scripts/build-macos.sh --install
```

The script builds the AU effect for Apple silicon and Intel, checks both architectures, and ad-hoc signs the bundle for local use. With `--install`, it installs under `~/Library/Audio/Plug-Ins/Components`, runs `auval -v aufx CfTc ChTi`, and tests the installed component in the JUCE host. A failed validation restores the previous component if present. Omit `--install` to build and package without touching the Mac's Components directory.

The component is written to `Build/macos/ChefsTouch_artefacts/Release/AU/Chefs Touch.component`. The packaged ZIP is `Build/macos/ChefsTouch-0.3.0-macOS-Universal-AU.zip`. The build targets macOS 12 or later. Each version of Logic Pro has its own macOS requirement. The ZIP is ad-hoc signed for development. A retail build needs Developer ID signing and notarization.

In Logic Pro, add the effect from **Audio FX → Audio Units → Chef Tai → Chef's Touch**. If it is listed but unavailable, open **Logic Pro → Settings → Plug-in Manager**, select Chef's Touch, and choose **Reset & Rescan Selection**. [Apple's Plug-in Manager guide](https://support.apple.com/guide/logicpro/lgcp9e26ef17/mac)

## Source layout

- `Source/PluginProcessor.*` owns automation, factory presets and serialized state.
- `Source/DSP/Processing.*` owns audio processing and preallocated delay and analysis buffers.
- `Source/PluginEditor.*` owns the controls, meters, coach and animation. A JUCE message-thread timer reads atomic audio measurements.
- `Assets/Images` contains the embedded original cartoon poses. See [art generation notes](Assets/Images/Art.md).

