# Tiny PTZ Portable

Windows x64 camera controller focused on the OBSBOT Tiny SE: pan, tilt, zoom,
optional live preview, and any number of named presets stored on the PC.
This is an independent focused replacement for PTZ control, not a full clone of
OBSBOT Center's recording, tracking, effects, firmware or account features.

## Run

Open **TinyPTZ.exe** in the portable folder (`dist/TinyPTZ` after building).
No installation, administrator access, Node,
Python, .NET runtime or OBSBOT Center is needed to run the built app.
The C++ runtime is statically linked; Windows supplies DirectShow.
Move the entire TinyPTZ folder to any writable location (including USB storage).
Presets are created in `data/presets.ptz` beside the executable.
Do not run multiple writers (GUI, CLI or two copies) against the same data file.

1. Select the Tiny SE and click **Connect**. This wakes the camera for control.
2. Start preview if desired. Leave preview off when OBS is using the camera.
3. Use the direction buttons (5 driver degrees per click), zoom buttons, or
   enter whole-number driver units and choose **Apply position**.
4. Wait for movement to finish, click **Read position**, enter a name, and
   choose **Save current**.
5. Select a preset and choose **Recall selected**, or double-click its row.

Use **Update selected** to replace a preset's position, **Rename**, **Delete**,
search, and **Import/Export** to manage the library. Import merges all camera
records and adds suffixes for duplicate names. Presets retain their camera
identity. Reconnecting on a different USB port or PC may change that identity;
edit the exported UTF-8 file's quoted device IDs using `ptzctl devices` if needed.
The three on-camera preset slots are not modified or imported.

The **Wake** and **Sleep** buttons control the gimbal power state. The app holds
a no-stream sleep guard while controlling the camera and releases it on Sleep
or disconnect/exit. This prevents the camera from parking while preview is off.
That guard is not readable in status; the app releases its guard on exit rather
than claiming to restore a previously configured guard value. Normal exit is
needed for cleanup.

The camera reports its own limits; unsupported controls or failed writes produce
errors. Position readback is what the Windows driver returns and may reflect a
commanded setpoint rather than measured gimbal motion. AI tracking can interfere
with manual positioning; disable tracking in OBSBOT
Center if needed. AI configuration is not implemented. Physical movement and
preview need camera permission in Windows.

## Command line / OBS integration

```
ptzctl devices
ptzctl read
ptzctl wake
ptzctl sleep
ptzctl save "Desk"
ptzctl recall "Desk"
ptzctl recall "Desk" --device "EXACT DEVICE ID"
```

Commands return nonzero on failure. The CLI selects a Tiny SE automatically only
when there is exactly one. Reads do not start video capture. Both executables
use the same portable data location. `hardware-test` makes small PTZ changes,
saves and recalls four test presets, and restores its starting position and
power state. It uses a separate test file and leaves user presets untouched.
The CLI releases its sleep guard on exit; keep OBS or another video stream open
if you want the camera to remain active after the CLI exits.

`src/core.hpp`, `camera.cpp`, and `presets.cpp` are the reusable C++17 core with
no GUI dependency. `obs/plugin.cpp` is an optional libobs module that registers
one OBS hotkey per preset at module load. See [OBS port notes](docs/OBS-PLUGIN.md).
The standalone CLI can already be invoked from OBS scripts or external hotkeys.

## Build and test

Requires Windows, Visual Studio C++ Build Tools and Windows SDK. Run:

```powershell
.\build.ps1 -Package
```

This builds the native portable executables, runs the core tests, and creates
`dist/TinyPTZ-Windows-x64.zip`. CMake is also supported. No third-party binary
from OBSBOT is bundled or loaded by the app. The development encoder probe
loads the installed DLL only to generate reference packets.

## REA investigation

REA 6.8.0 is installed under `tools/rea` (ignored by Git), registered for Codex,
and used through MCP by `tools/rea-client.mjs`. Restart Codex to expose its tools
directly in a new session. The local MCP client works in this session without
that restart. Source, evidence and setup details are in
[research/NOTES.md](research/NOTES.md). REA is a development tool only.
