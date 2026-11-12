# Tiny PTZ

A desktop controller and OBS Studio plugin for the **OBSBOT Tiny SE** camera,
available for Windows and Linux. Control the camera, save unlimited positions
and settings, and recall presets automatically when OBS scenes change.

This is an independent project, not affiliated with or endorsed by OBSBOT.
Only the Tiny SE has been tested; support for other OBSBOT models is not claimed.

## Features

- Live pan, tilt and zoom controls with automatic position readback.
- Unlimited named presets, optionally including camera settings; import/export
  libraries in the shared `.ptz` format.
- Tracking, image and device controls supported by the camera and driver,
  including image defaults and exposure compensation.
- Camera boot-position presets, sleep/wake and reconnect to the last camera.
- Standalone camera preview, preset search and light, dark or system themes.
- OBS dock with scene preset assignments and full configuration backups.
  Backups can match scenes by name when restored on another computer.

Available settings depend on firmware and the operating-system driver. See
[settings coverage](docs/SETTINGS.md) for details and limitations.

## Download and install

Download the matching asset from this repository's Releases page. Desktop and
plugin releases are versioned separately; Windows and Linux binaries may have
different version numbers.

| Product | Platform | Release asset | Installation |
| --- | --- | --- | --- |
| Desktop app | Windows x64 | `TinyPTZ-v…-Windows-x64-Release.zip` | Extract the entire folder and run `TinyPTZ.exe`. |
| Desktop app | Linux x86-64 | `TinyPTZ-v…-Linux-x86_64.AppImage` or its release ZIP | Make the AppImage executable and open it. |
| OBS plugin | Windows x64 | `TinyPTZ-OBS-v…-Windows-x64-Release.zip` | Close OBS and copy `tiny-ptz-obs` into `C:\ProgramData\obs-studio\plugins`. |
| OBS plugin | Linux x86-64 | `TinyPTZ-OBS-v…-Linux-x86_64-Release.zip` | Close OBS and copy `tiny-ptz-obs` into `~/.config/obs-studio/plugins`. |

Keep all supplied files together. Release packages start with an empty preset
library; import your existing presets or restore a plugin backup after upgrading.

The Linux desktop app adds its menu entry and icon on first launch. If you move
the AppImage, open it once from its new location to update the launcher.

### Compatibility

- **Windows desktop:** Windows 10/11 x64. Qt and Visual C++ runtimes are bundled.
- **Linux desktop:** the supplied AppImage is built on Ubuntu 24.04 with glibc
  2.39 and Qt 6.4.2. It has also been reported working on Fedora 44. Older systems
  may need a native build.
- **Windows OBS plugin:** the supplied binary targets OBS 31.x with Qt 6.6.3.
- **Linux OBS plugin:** built against native OBS 30.0.2 and Qt 6.4.2 on Ubuntu
  24.04. Other OBS/Qt builds, including Flatpak, may require a matching rebuild.

OBS supplies the plugin's runtime libraries. Do not copy the standalone app's
Qt libraries into OBS.

Platform guides: [Windows desktop](docs/DESKTOP-WINDOWS.md),
[Linux desktop](docs/DESKTOP-LINUX.md), [Windows OBS](docs/OBS-PLUGIN.md),
[Linux OBS](docs/OBS-LINUX.md).

## First use

1. Connect the Tiny SE, select it and click **Connect** / **Connect / read**.
2. Drag the PTZ sliders to move the camera. Wait for it to settle, then choose
   **Save new** to capture a preset.
3. Enable **Save camera settings with presets** to include supported settings.
4. In the standalone app, use **Start preview** for video. Stop preview before
   another application captures the camera. In OBS, use a Video Capture Device
   source for the picture and enable **Docks → Tiny PTZ** for controls.
5. In the OBS dock's **Scenes** tab, assign presets to scenes and enable scene
   automation. It follows the active Program scene, not Studio Mode Preview.

**Set boot position** stores PTZ in the camera itself. Scene automation and
settings snapshots are application features; they require the app or OBS to run.

## Presets and backups

| Product | Default user data |
| --- | --- |
| Windows desktop | `data/` beside `TinyPTZ.exe` |
| Linux desktop | `$XDG_DATA_HOME/TinyPTZ`, normally `~/.local/share/TinyPTZ` |
| Windows OBS plugin | `%APPDATA%\obs-studio\plugin_config\tiny-ptz-obs` |
| Linux OBS plugin | OBS's configuration directory, under `plugin_config/tiny-ptz-obs` |

Back up the desktop data folder to retain presets and preferences. The desktop
app also accepts `--data-dir <directory>` for a chosen portable data location.
Use **Back up plugin…** / **Restore plugin…** for presets and scene assignments
together; a `.ptz` export contains presets only.

Windows and Linux camera IDs differ. On Linux, use **Assign presets to camera…**
after importing a library from Windows or changing camera identity. Keep only one
plugin copy installed, and avoid simultaneous writes to the same preset file.

## Development

See [building and testing](docs/BUILDING.md), [contributing](CONTRIBUTING.md),
and [creating release packages](docs/RELEASING.md).

```text
assets/       Shared application icons and resources
docs/         User, build and release guides
obs/          Shared Qt controls, OBS integration and backup handling
packaging/    Linux launcher and AppImage runtime tooling
scripts/      Public source/runtime archive creation
src/          Camera backends, presets, protocol and desktop app
tests/        Core, UI, backend and OBS module tests
```

Build scripts remain at the repository root. Generated binaries, SDKs, personal
configuration and local investigation files are excluded from the source tree.

## AI assistance

This project was developed with substantial assistance from AI tools for implementation, debugging and documentation. AI-assisted code
can contain mistakes, and compatibility is not guaranteed beyond the tested
hardware and software configurations. Please report reproducible issues.

## License

Tiny PTZ is licensed under **GPL version 2 or later**; see [LICENSE.txt](LICENSE.txt).
See [third-party notices](THIRD-PARTY-NOTICES.md) for Qt, OBS and runtime provenance.
No proprietary OBSBOT binaries or SDK are distributed with the application.
