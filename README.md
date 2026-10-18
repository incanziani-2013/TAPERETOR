# TRAPERETOR

Industrial-style oneshot sampler and synth, built as a real **VST3 plugin** (JUCE 8) for FL Studio and any other VST3 host.

Drop your oneshots on the plugin window, play them from MIDI, and shape them with 30 knobs. Every knob is automatable from the host.

## Features
- **Library browser (banks).** Create banks, add sounds or whole folders, search, and click a sound to load it into the selected slot. Banks live in `Documents/TRAPERETOR/Banks/<bank name>/` (sounds are copied there, so they never go missing). You can also drop a folder of sounds straight into that directory.
- **BPM control.** By default sounds play untouched (no change in length or tempo). Turn on **BPM SELECT**, type the sound's **ORIG BPM** and the **TARGET BPM** (or press **HOST** to follow the DAW tempo): the sound is time-stretched (pitch preserved) to fit. The original BPM of each sound is saved in its bank.
- **Resizable window** that scales to your screen (drag the bottom-right corner).
- 8 oneshot slots, drag and drop sounds or folders onto the plugin (WAV, AIFF, MP3, FLAC, OGG). Slots are saved with the project.
- Sample layer: level, start, length, pitch, fine, loop, reverse.
- Oscillator layer: wave, level, unison (1-5), detune, sub, noise.
- Filter (LP/HP/BP) with its own envelope, amp envelope (ADSR), LFO (sin/tri/saw/sqr to amp, filter or pitch).
- Effects: drive, delay, reverb, tone, master.
- 16-voice polyphony, MIDI note input, built-in test keyboard.
- No presets: every sound is built by you.

## Build with GitHub (no setup on your PC)
1. Create a repo and push this folder to it.
2. Open the **Actions** tab. The `Build plugin` workflow runs on every push (or press *Run workflow*).
3. When it finishes, download the artifact `TRAPERETOR-Windows` or `TRAPERETOR-macOS`. Each one contains:
   - `TRAPERETOR-<version>-Windows-Setup.exe` / `TRAPERETOR-<version>-macOS-Setup.pkg`: an installer that lets you pick VST3 and/or VST2.
   - `TRAPERETOR-<version>-<OS>-Plugins.zip`: the raw plugin files, for manual install.

### Releases
Bump `VERSION` in `CMakeLists.txt`, then push a matching tag:

    git tag v1.0.0
    git push origin v1.0.0

The workflow builds everything and publishes the installers in a GitHub Release.

### VST2
Steinberg no longer distributes the VST2 SDK, so it is not in this repo and VST2 is only built when you provide it. Add **one** of these repository secrets (Settings > Secrets and variables > Actions):
- `VST2_SDK_URL`: a direct download link to a `.zip` of the SDK.
- `VST2_SDK_ZIP_BASE64`: the `.zip` encoded as base64. Only the `pluginterfaces/vst2.x/` folder is needed, which keeps it small enough for a secret (on macOS/Linux: `base64 -i vst2sdk.zip`; on Windows: `[Convert]::ToBase64String([IO.File]::ReadAllBytes("vst2sdk.zip"))`).

The zip must contain `pluginterfaces/vst2.x/aeffect.h` (at any depth). Without a secret, the build skips VST2 and the installers only offer VST3. You need a VST2 license agreement with Steinberg to distribute VST2 plugins.

## Install in FL Studio
1. Run the installer. It puts the VST3 in `C:\Program Files\Common Files\VST3\` (macOS: `/Library/Audio/Plug-Ins/VST3/`) and lets you choose the VST2 folder on Windows (default `C:\Program Files\Common Files\VST2\`; macOS: `/Library/Audio/Plug-Ins/VST/`). For a manual install, copy the files from the Plugins zip to those folders.
2. In FL Studio: Options > Manage plugins > **Find installed plugins** (Verify plugins).
3. Add it from the plugin database: Installed > Plugin database > VST3 > TRAPERETOR. It is a generator (instrument).

## Build locally
Requires CMake 3.22+ and a C++17 compiler (Visual Studio 2022 on Windows, Xcode on macOS).

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

The plugin ends up in `build/TRAPERETOR_artefacts/Release/VST3/`. To also build VST2, add `-DVST2_SDK_PATH=<folder containing pluginterfaces/vst2.x>` to the first command. A Standalone app is built too, for quick testing.

## Notes
- macOS builds and the `.pkg` installer are not notarized: right-click > Open the installer, or allow it in System Settings > Privacy & Security.
- The Windows installer is unsigned, so SmartScreen may warn: click *More info* > *Run anyway*.
- JUCE is downloaded automatically by CMake and is licensed under AGPLv3/commercial. If you distribute this plugin, check the JUCE license terms.

## License
Source code: MIT (JUCE itself has its own license, see above).
