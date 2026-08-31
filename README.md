# SoundMod

A Windows key/click sound mod (in the spirit of [zcb3](https://github.com/zeozeozeo/zcb3)) with:

- **Mic + click-sound routing into a virtual audio cable**, so other apps (Discord, OBS, a game capture, etc.) hear your mic *and* your click sounds mixed together as if they came from a single microphone.
- **A global input hook** that plays a down/up sound the instant a bound key or mouse button is pressed/released — works even when another window (e.g. the game) has focus.
- **A built-in browser panel** (Microsoft Edge WebView2) pointed at [db.ruikasa.lol](https://db.ruikasa.lol/), so you can search, preview, and download soundpacks without leaving the app. Downloads are redirected straight into `soundpacks/`.

## One-time setup (you, not the CI)

1. Install a virtual audio cable driver — [VB-Audio Virtual Cable](https://vb-audio.com/Cable/) is the common free option (same one zcb3-style tools typically rely on). This creates a device like `CABLE Input (VB-Audio Virtual Cable)`.
2. In whatever app you want to "hear" you (Discord, OBS, etc.), set its microphone/input source to `CABLE Output (VB-Audio Virtual Cable)`.
3. In SoundMod's config (`config.ini`, generated on first run), set `outputDeviceName` to `CABLE Input (VB-Audio Virtual Cable)`. This is where SoundMod sends the mixed mic+clicks audio.

## Soundpacks

Drop a soundpack folder into `soundpacks/<name>/` and point `activeSoundpackDir` at it in `config.ini`. The current key handler (`src/main.cpp::OnKeyEvent`) looks for:

- `soundpacks/<name>/<virtualKeyCode>_down.wav` / `_up.wav` for per-key sounds
- falls back to `soundpacks/<name>/click.wav` / `release.wav` for a single shared sound

Packs downloaded via the built-in browser land in `soundpacks/` automatically; wire up extraction/pack-selection UI as a next step (see below).

## Building locally

Requires Visual Studio 2022 (Desktop C++ workload) and the NuGet CLI.

```powershell
nuget restore packages.config -PackagesDirectory packages
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The built `SoundMod.exe` needs `WebView2Loader.dll` (from `packages/Microsoft.Web.WebView2/build/native/x64/`) next to it, plus a real Edge WebView2 Runtime installed (pre-installed on Windows 10 21H2+/Windows 11; otherwise grab the [Evergreen Bootstrapper](https://developer.microsoft.com/microsoft-edge/webview2/)).

## CI

`.github/workflows/build.yml` restores the WebView2/WIL NuGet packages, builds with MSVC via CMake on `windows-latest`, uploads `SoundMod.exe` + `WebView2Loader.dll` as a build artifact on every push/PR, and attaches them to a GitHub Release when you push a `vX.Y.Z` tag.

## Project layout

```
include/            headers (AudioEngine, InputHook, BrowserPanel, Config, miniaudio.h)
src/                implementations + main.cpp (window + wiring)
soundpacks/default/ place click.wav / release.wav here to test immediately
.github/workflows/  CI build
```

## What's stubbed / next steps

- **UI**: `main.cpp` currently makes the browser panel fill the whole window. Add a control strip (device pickers, soundpack list, volume sliders) — plain Win32 controls or an ImGui overlay both work fine alongside WebView2.
- **Per-key binding UI**: `Config.h` already has a `KeyBinding` struct for mapping specific sounds to specific keys; `OnKeyEvent` currently uses the simpler filename-convention fallback — swap it to read `AppConfig::bindings` once you have a UI to edit them.
- **Soundpack auto-extract**: downloads land in `soundpacks/` as whatever archive format db.ruikasa.lol serves; add a zip-extract step in the `SetDownloadCompleteCallback` in `main.cpp`.
- **Pitch shifting** in `AudioEngine::TriggerSound` is a cheap resample-rate trick, not a true pitch-preserving algorithm — fine for the "randomize pitch a little" effect zcb3-style tools use, but swap for a proper resampler if you want cleaner results.
