# Tiny PTZ dock for OBS Studio — v0.4.4

Native Windows x64 OBS plugin for the OBSBOT Tiny SE. This package targets
**OBS 31.x with Qt 6.6.3**, matching the local OBS 31.0.0-beta3 runtime used to
build it. Other OBS/Qt versions require a matching rebuild; do not replace OBS's
Qt DLLs with the development package. OBS supplies Qt and the Visual C++ runtime.
The standalone Tiny PTZ application remains independently portable.

## Install

Close OBS before installing. Extract this ZIP to a writable folder, then run
PowerShell as administrator from that folder:

```powershell
powershell -ExecutionPolicy Bypass -File .\install-obs.ps1
```

This installs the plugin under `%ProgramData%\obs-studio\plugins\tiny-ptz`.
Pass `-ObsRoot "D:\Apps\obs-studio"` if OBS is elsewhere.
For portable OBS, use `-Portable -ObsRoot "D:\OBS"`; alternatively copy the
package's `obs-plugins` and `data` folders into the portable OBS root.
No camera driver, OBSBOT Center SDK or companion process is installed.

Open OBS and enable **Docks → Tiny PTZ**. Drag its title bar to dock it beside
your other OBS panels, or leave it floating. OBS remembers the dock layout.
To uninstall, close OBS and remove the plugin DLL/data folders you installed.
Your user presets/configuration can be kept for a later reinstall.

## Camera and settings

Choose the Tiny SE and click **Connect / read**. The PTZ sliders apply while
dragging, with coalesced updates about every 80 ms. Typed PTZ values apply when
editing finishes. Use **Read position** after the gimbal settles before saving.
Wake and Sleep use the same repaired power commands as Tiny PTZ v0.3.4.

The **Settings** tab exposes the shared core's tracking modes, human framing,
composition, image and device settings. Numeric sliders apply live; numeric text,
named choices and Automatic mode use **Apply selected setting**. Final numeric
values use verified application where the setting supports readback.
**Default selected** and **Restore image defaults** are available. Unsupported
and read-only controls are disabled, using camera-reported ranges.
See the included `SETTINGS.md` for model-specific coverage.

Camera control runs on a dedicated COM STA worker. The dock never opens a preview
graph: keep using an OBS Video Capture Device source for the image. Close other
camera controllers while controlling the device from OBS, to avoid competing
tracking, sleep and position commands. Hardware work does not block OBS's GUI.

## Presets

The package includes a snapshot of your existing portable presets. On first run,
it is copied to OBS's plugin configuration folder only if no preset library exists.
Existing plugin configuration is never replaced by installation.

Use **Import presets...** to copy presets from your standalone
`TinyPTZ-v0.3.4\data\presets.ptz` into the plugin library. Scene recalls reload the
active library before resolving their preset. Previously configured external
libraries remain supported; avoid saving from two applications at once.

**Import presets...** merges a `.ptz` file into the active library. Existing
presets and scene assignments stay intact; duplicate names for the same camera
get numbered suffixes, such as `Wide (2)`. Camera identities, PTZ coordinates and
saved settings are preserved. The imported file is not modified.

**Export presets...** saves all presets and settings to a separate `presets.ptz`
file that can be backed up, moved to another PC, or imported into Tiny PTZ.
Export leaves the active library selected. Scene assignments are stored separately
in `dock.json` and are not part of a `.ptz` export.

## Full plugin backup and restore

The **Back up plugin...** and **Restore plugin...** buttons below the tabs work
with one self-contained `.tinyptz.json` file. It includes:

- Every preset, its camera identity, PTZ coordinates and saved camera settings.
- Scene assignments for every scene collection.
- Whether scene automation is enabled.
- The **Save camera settings with presets** preference (also remembered between sessions).

To back up, click **Back up plugin...** and choose a file. To restore, click
**Restore plugin...**, select that file and review the preset/assignment counts
before confirming. Restore replaces the active plugin configuration instead of
merging it. Invalid or unsupported backups are rejected before anything changes.

Restored presets go into a new managed file in the local plugin configuration
folder. The old preset file stays intact, including any external/shared library.
The old absolute path in the backup is not reused, so restoration also works
after moving the backup to another computer. A failed configuration commit
removes its staged preset file and keeps the previous configuration active.

Restore does not initiate a camera move. The restored automation preference
applies on the next Program scene change or OBS startup. Existing camera work
already in progress may finish.

Backups now include scene names as well as UUIDs. On another PC, restore first
matches existing UUIDs, then exact scene names within the same scene collection.
If the original collection no longer exists, it can match a scene in a differently
named collection when that UUID/name is unique across the destination collections.
Names are case sensitive. UUID matching wins if a scene has been renamed.

The restore confirmation shows UUID/name match counts and unresolved assignments;
**Show Details** lists each result. Duplicate names or competing assignments never
silently overwrite one another. Unresolved assignments remain inactive and can
match later when their collection/scene becomes available. A manually assigned
destination scene takes priority over a pending restored assignment. Scene names
are gathered from active scenes and saved inactive collections without switching
OBS scenes. New assignments and scene renames keep the saved names current.

**Create a fresh backup with v0.4.3 or newer before moving it to another PC.** Older backups
remain supported by UUID but contain no scene names to match when UUIDs change.
Changed USB camera identities may still need updating. This backup covers this
plugin's configuration; OBS owns the dock layout, scenes and streaming settings.
Create/import the actual OBS scene collections separately if they do not yet
exist on the destination PC; the plugin backup does not create OBS scenes.

**Load preset** (or double-click), **Save new**, **Update selected**, **Rename**,
and **Delete** operate on an arbitrary number of presets. Check **Save camera
settings with presets** for a settings snapshot; uncheck for PTZ-only.
Presets retain their original camera identity. A disconnected camera produces an
error rather than silently controlling another device. A changed USB identity
must be corrected in the library or saved again for that camera.

**Set boot position** stores the selected preset's PTZ in the camera itself;
this does not require OBS to run after the camera boots. Startup tracking mode
still applies. Scene automation, in contrast, runs only while OBS is open.

## Scene automation

1. Open **Scenes** in the dock.
2. Select a scene and a saved preset; click **Save scene assignment**.
3. Repeat for any other scenes. Choose **No preset** to clear an assignment.
4. Enable **Enable scene preset automation**.

Enabling automation also applies the current Program scene's assignment. With
automation enabled it also applies at OBS startup and when loading a collection.
Studio Mode Preview selection does not trigger movement; the Program scene does.
Unassigned scenes leave the camera alone and cancel pending scene recalls.
The dock can be hidden and automation continues.

Assignments are scoped by scene collection name and scene source UUID. Renaming
a scene preserves its assignment. A duplicated/new scene needs its own assignment.
Renaming a collection creates a different assignment namespace: assign its scenes
again or restore the previous collection name. Preset rename/delete in the dock
updates/removes its assignments. Externally renamed/deleted presets produce a
visible missing-preset error until reassigned.

Rapid scene changes retain only the newest queued recall. A camera operation
already in flight is allowed to finish before the latest recall, since interrupting
USB writes/rollback mid-operation is unsafe. The camera may visibly move during
a transition; one physical camera cannot show two preset angles simultaneously.
The shared core's preset recall verifies positioning and restores saved settings.
Tracking-enabled presets can move again as tracking follows the subject.

## Files and build

User files are `dock.json` and `presets.ptz` in the OBS plugin configuration
directory, normally `%APPDATA%\obs-studio\plugin_config\tiny-ptz` (inside the OBS
config directory in portable mode). The JSON stores the selected preset-file path,
automation switch and scene assignments. Presets use the same UTF-8 v2 format as
the standalone app. Preset writes use atomic replacement; configuration writes use
Qt's atomic save facility.

Plugin source is included in `TinyPTZ-plugin-source.zip`. Extract it, install
Visual Studio C++ Build Tools and CMake, then:

```powershell
.\build-obs.ps1 -Setup -Package -ObsRoot 'C:\Program Files\obs-studio'
```

The script downloads SHA-256-checked official OBS source headers and OBS's Qt
development package into ignored `build/obs-sdk`. Import libraries are generated
from the installed OBS runtime's public DLL exports, and the plugin uses `/MD` to
match OBS/Qt. No proprietary OBSBOT binary is linked or redistributed.
Offline rebuilds omit `-Setup` once dependencies exist. The optional CMake
`BUILD_OBS_PLUGIN` target also supports installed OBS/Qt SDK packages, or the
`TINY_OBS_SOURCE_DIR`/`TINY_OBS_LIBRARY_DIR` prepared by this script.

The build runs camera-free worker, scene-rule, persistence and Qt control tests,
then loads the actual DLL through the installed libobs runtime with a separate
test frontend. The test frontend DLL is never packaged or installed. Add
`-HardwareTest` to also test the dock against a connected Tiny SE (a live
brightness adjustment and scene-driven recall to its original captured position;
the original camera state is restored). These tests do not manipulate OBS's UI
or the user's OBS configuration. Native OBS GUI layout/streaming remains a manual
integration check after installation.

The plugin and included source are provided under GPL v2 or later (see
`LICENSE.txt`), compatible with the linked OBS APIs. Qt remains supplied by OBS.

Official API references: [docks and frontend events](https://docs.obsproject.com/reference-frontend-api),
[OBS plugin template dependencies](https://github.com/obsproject/obs-plugintemplate).
