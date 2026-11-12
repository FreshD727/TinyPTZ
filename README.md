# Tiny PTZ Portable

A native Qt camera controller for the OBSBOT Tiny SE, with a resizable preview,
dark/light/system themes, live PTZ controls, tracking/image/device settings and
unlimited presets. The standalone app and OBS plugin share the same camera core.

The standalone application also runs on Linux. The **v0.7.5 x86_64 AppImage**
includes Qt and video-preview dependencies and targets Ubuntu 24.04 or compatible
newer systems. See [Linux standalone instructions](docs/DESKTOP-LINUX.md) for
launching, portable data storage and building it yourself. The Windows release
below remains available separately.

## Run the Windows release

Extract the whole `TinyPTZ-v0.7.4` folder and open **TinyPTZ.exe**. Keep the Qt
DLLs and `platforms` folder beside it. The package includes the Qt and Visual C++
runtimes: OBS Studio, OBSBOT Center, Python, Node and an installer are not required.
Use a writable folder; portable data stays in `data` beside the executable.
Move the entire folder to keep its presets and preferences with the app.

1. Select a camera and click **Connect** on first use.
2. Leave **Connect to my last camera automatically** checked to reconnect to it
   on the next launch. An unplugged camera is retried; another camera is never
   silently substituted. A different USB port/PC can change its identity, in
   which case select it again once.
3. Use the **Theme** selector for System, Light or Dark. The choice is remembered.
4. Drag Pan/Tilt/Zoom sliders for live movement. Typed positions apply when
   editing finishes. Position readback updates automatically (about every 400 ms)
   and pauses while you edit, so it does not fight your input. No Read position
   button is needed in the standalone app.
5. Click **Start preview** for the camera picture. The preview is resizable and
   keeps a 16:9 aspect ratio. Use **Stop preview** when OBS is capturing the camera.
   Preview is off at launch; camera control does not need a video capture graph.

## Settings and presets

The **Settings** tab provides tracking modes, framing/composition, image controls
and supported device settings. Select a setting; drag numeric sliders to apply
live, or use **Apply selected setting** for typed values, named choices and Auto
mode. **Default selected** and **Restore image defaults** are available.
Unavailable/read-only controls are disabled. Exposure Compensation is -9 to +9
with a neutral default of 0. See [settings coverage](docs/SETTINGS.md).

In **PTZ / presets**, search the library, double-click a preset or use **Load
preset**. **Save new**, **Update selected**, **Rename** and **Delete** manage an
arbitrary number of presets. **Save camera settings with presets** captures
tracking, image and device settings along with PTZ; uncheck for PTZ-only presets.
Allow the camera to settle before saving. Tracking-enabled presets may move
again as they follow a subject.

**Import presets...** merges a `.ptz` library while preserving existing records;
same-camera duplicate names receive numbered suffixes. **Export presets...**
saves a separate copy. Camera identities and all saved settings are preserved.
Existing v1/v2 preset files are supported; the new package carries forward the
previous portable library without overwriting it. Avoid simultaneous writes to
one file from multiple app copies.

**Set boot position** writes the selected preset's PTZ to the camera and verifies
it. The app need not run when the camera boots. The camera's startup tracking mode
still applies, and image/tracking snapshots remain PC-side preset data.
**Wake** and **Sleep** use the same corrected Tiny SE power commands as v0.3.4.

`data/presets.ptz` stores the library. `data/app.json` stores the theme, last camera,
auto-connect and preset-capture preferences. Local preset paths are relative to
the data folder, so moving the portable folder does not break the library.
The old Win32 v0.3.4 app remains available; its guide is
[LEGACY-DESKTOP.md](docs/LEGACY-DESKTOP.md).

## OBS Studio plugin

The [OBS dock plugin](docs/OBS-PLUGIN.md) remains a separate package:
`dist/TinyPTZ-OBS-v0.6.2-Windows-x64-Release.zip` for OBS 31.x / Qt 6.6.3 on Windows x64.
It includes scene-driven preset recall, full configuration backups and scene-name
matching when restoring on another PC. This standalone update does not replace
the installed OBS plugin or its configuration.

## Build and test

For the native Linux OBS plugin, see [OBS-LINUX.md](docs/OBS-LINUX.md).
The Linux v0.6.2 backend uses V4L2/UVC, supports Windows-preset rebinding and
builds with `bash build-linux.sh --package`. The standalone preview app remains
Windows-only. Linux runtime archives and the source archive are separate from
the existing Windows releases.

Use Windows x64, Visual Studio C++ Build Tools and the Windows SDK. First setup:

```powershell
.\build-desktop.ps1 -Setup -Package
```

The script reuses/downloads the SHA-256-checked official Qt 6.6.3 development
package under `build/obs-sdk/qt6`; OBS itself is not needed. Later builds can use
`./build.ps1 -Package`. Use `-QtRoot` on `build-desktop.ps1` for another matching
Qt development folder. `./build.ps1 -Legacy -Package` builds the older Win32 app.
CMake also supports `BUILD_QT_DESKTOP=ON` (default) with `CMAKE_PREFIX_PATH` pointing
to Qt; `BUILD_QT_DESKTOP=OFF` selects the legacy UI.

The build runs protocol/preset tests, shared Qt control and backup tests, and
standalone theme/preferences/preview-host regression tests. Add `-HardwareTest`
to `build-desktop.ps1` to test auto-connect/restart, live measured position updates,
DirectShow preview start/stop/resize/restart and closing while previewing against
a connected Tiny SE. The test restores original camera settings/position/power
and uses a temporary preset library. Close other video capture apps before that
explicit preview test.

The CLI `ptzctl.exe` remains available for automation; `ptzctl help` lists commands.
Camera work runs on a dedicated COM STA thread, with coalesced writes, idle
position reads and verified final settings. The GUI remains responsive during
USB operations and preview startup/teardown.

Application source, Qt Base source, license texts and runtime provenance are
included; see [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md). No proprietary
OBSBOT binary is loaded or distributed.
