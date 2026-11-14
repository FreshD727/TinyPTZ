# Tiny PTZ desktop app for Windows

The v1.0.2 Windows package reuses the tested v1.0.1 binaries unchanged. This
release fixes Linux camera movement only; internal Windows version strings
therefore still show 1.0.1.

Extract the entire Windows x64 release ZIP to a writable folder and run
`TinyPTZ.exe`. Keep the Qt/Visual C++ DLLs and `platforms` folder beside it.
OBS Studio, OBSBOT Center and a separate Qt installation are not required.

Select the Tiny SE and click **Connect** once. Automatic connection remembers
that camera for future launches. The **Theme** selector offers System, Light
and Dark. PTZ values refresh automatically; sliders apply while dragging.

Use **Start preview** to see video, and **Stop preview** before another app
captures the camera. Camera control works while preview is off.

The **Settings** tab provides supported tracking, image and device controls.
Numeric sliders apply live. Use **Apply selected setting** for named choices,
automatic mode and typed settings. **Default selected** and **Restore image
defaults** restore camera-reported defaults. See [settings coverage](SETTINGS.md).

Use **Save new**, **Load preset**, **Update selected**, **Rename** and **Delete**
in **PTZ / presets**. The optional settings checkbox captures supported settings
with the position. Wait for the gimbal to settle before saving. Import/export
transfers `.ptz` libraries; it does not transfer OBS scene assignments.

**Set boot position** stores PTZ in the camera. Startup tracking can still alter
the framing; the full image/tracking snapshot remains application-side data.

## Data and upgrades

`data/presets.ptz` stores presets. `data/app.json` stores the last camera, theme
and preferences. Back up or move the complete data folder with the app. The
`--data-dir <directory>` option selects a different data directory.

Public releases have an empty library. Import your presets, or copy your old
data folder into the new release folder while Tiny PTZ is closed. Avoid running
two app copies against the same preset file simultaneously.

The [OBS plugin](OBS-PLUGIN.md) is distributed separately. Build instructions
are in [BUILDING.md](BUILDING.md). License and runtime attribution are included
in each release and described in [third-party notices](../THIRD-PARTY-NOTICES.md).
