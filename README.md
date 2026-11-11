# Tiny PTZ Portable

Windows x64 camera controller focused on the OBSBOT Tiny SE: pan, tilt, zoom,
optional live preview, tracking/image/device settings, and any number of named
presets stored on the PC. Presets can include camera settings as well as PTZ.
This independent app uses Windows camera interfaces directly; see the
[settings coverage](docs/SETTINGS.md) for capabilities and remaining gaps.

## Run

Open **TinyPTZ.exe** in the portable folder (`dist/TinyPTZ-v0.3` after building).
No installation, administrator access, Node,
Python, .NET runtime or OBSBOT Center is needed to run the built app.
The C++ runtime is statically linked; Windows supplies DirectShow.
Move the entire TinyPTZ-v0.3 folder to any writable location (including USB storage).
Presets are created in `data/presets.ptz` beside the executable.
Do not run multiple writers (GUI, CLI or two copies) against the same data file.

1. Select the Tiny SE and click **Connect**. This wakes the camera for control.
2. Start preview if desired. Leave preview off when OBS is using the camera.
3. Use the direction buttons (5 driver degrees per click), zoom buttons, or
   enter whole-number driver units and choose **Apply position**.
4. Open **Settings...** to edit Tracking, Image, or Device controls. Select a
   row, choose a value (or Automatic where available), and Apply. The current
   value is read back from the camera. Set-only requests are labeled explicitly.
5. In Image settings, **Default selected** restores one control; **Restore image
   defaults** resets the image controls together. Exposure Compensation uses
   -9 to +9 with a neutral default of 0. Apply keeps the selected setting ready
   for another edit.
6. Leave **Include tracking, image and device settings when saving presets**
   checked to capture settings, or uncheck it for PTZ-only presets.
7. Wait for movement to finish, click **Read position**, enter a name, and
   choose **Save current**.
8. Select a preset and choose **Recall selected**, or double-click its row.

Use **Update selected** to replace a preset's position and settings, **Rename**, **Delete**,
search, and **Import/Export** to manage the library. Import merges all camera
records and adds suffixes for duplicate names. Presets retain their camera
identity. Reconnecting on a different USB port or PC may change that identity;
edit the exported UTF-8 file's quoted device IDs using `ptzctl devices` if needed.
The three on-camera preset slots are not modified or imported.

To use a saved PTZ position when the camera powers on, select its preset and
click **Set as boot position**. The position is written to the camera and read
back for confirmation; Tiny PTZ does not need to run at startup. This stores
pan, tilt and zoom, independently of the preset's image/tracking snapshot.
The existing Device / Startup tracking mode remains in effect; choose Off if
you want the boot framing to stay fixed. Each camera has one boot position;
setting another replaces it. Editing/deleting the local preset afterward does
not change the stored boot position. A firmware recall and connection reopen
were tested; a physical USB power cycle has not been tested.

The **Wake** and **Sleep** buttons control the gimbal power state. The app holds
a no-stream sleep guard while controlling the camera and releases it on Sleep
or disconnect/exit. This prevents the camera from parking while preview is off.
That guard is not readable in status; the app releases its guard on exit rather
than claiming to restore a previously configured guard value. Normal exit is
needed for cleanup.

The camera reports its own limits; unsupported controls or failed writes produce
errors. Position readback is what the Windows driver returns and may reflect a
commanded setpoint rather than measured gimbal motion. Recall pauses saved AI
tracking for the PTZ move, restores saved configuration, then activates the saved
tracking mode. Active tracking can move the camera afterward. Choose Tracking
mode Off for a fixed framing. Physical movement and preview need camera
permission in Windows.

Existing v1 PTZ-only files remain readable. Their first save writes v2 and keeps
a `.v1.backup`. The build copies an existing preset file into the new release
folder only if that folder has none. Older executables cannot read v2; keep
using the new GUI and CLI together. Original on-camera preset slots are untouched.

## Command line / OBS integration

```
ptzctl devices
ptzctl read
ptzctl wake
ptzctl sleep
ptzctl settings
ptzctl image-defaults
ptzctl set exposure_compensation 0
ptzctl set brightness 50
ptzctl set white_balance 5000 --auto
ptzctl set ai_mode 2
ptzctl save "Desk"
ptzctl recall "Desk"
ptzctl set-boot "Desk"
ptzctl boot-read
ptzctl recall "Desk" --device "EXACT DEVICE ID"
```

Commands return nonzero on failure. The CLI selects a Tiny SE automatically only
when there is exactly one. Reads do not start video capture. Both executables
use the same portable data location. `hardware-test` makes small PTZ changes,
saves and recalls four test presets, and restores its starting position and
power state. It uses a separate test file and leaves user presets untouched.
`settings-test` temporarily changes supported settings, checks readback, exercises
a full disk-backed settings preset, and restores the starting values. It does
not use the user preset library. Run tests only when camera activity is acceptable.
The CLI releases its sleep guard on exit; keep OBS or another video stream open
if you want the camera to remain active after the CLI exits.

`src/core.hpp`, `camera.cpp`, `settings.cpp`, and `presets.cpp` are the reusable C++17 core with
no GUI dependency. `obs/plugin.cpp` is an optional libobs module that registers
one OBS hotkey per preset at module load. See [OBS port notes](docs/OBS-PLUGIN.md).
The standalone CLI can already be invoked from OBS scripts or external hotkeys.

## Build and test

Requires Windows, Visual Studio C++ Build Tools and Windows SDK. Run:

```powershell
.\build.ps1 -Package
```

This builds the native portable executables, runs core and hidden native-control
regression tests, and creates
`dist/TinyPTZ-v0.3-Windows-x64.zip`. CMake is also supported. No third-party binary
from OBSBOT is bundled or loaded by the app. The development encoder probe
loads the installed DLL only to generate reference packets.

## REA investigation

REA 6.8.0 is installed under `tools/rea` (ignored by Git), registered for Codex,
and used through MCP by `tools/rea-client.mjs`. Restart Codex to expose its tools
directly in a new session. The local MCP client works in this session without
that restart. Source, evidence and setup details are in
[research/NOTES.md](research/NOTES.md). REA is a development tool only.
