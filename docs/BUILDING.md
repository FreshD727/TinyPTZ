# Building and testing

Run commands from the repository root. Generated output, downloaded SDKs and
release archives stay in ignored `build/` and `dist/` directories.

## Windows desktop

Install Visual Studio C++ Build Tools and the Windows SDK. The setup option
downloads the pinned Qt 6.6.3 SDK and checks its SHA256:

```powershell
.\build-desktop.ps1 -Setup
```

Subsequent builds use `./build-desktop.ps1` or `./build.ps1`. Supply `-QtRoot`
to use an existing matching Qt SDK. OBS is not required for the desktop build.
The script builds the GUI, CLI and tests, then runs the camera-free tests.

For release packaging, install Python 3.10+ and add `-Package`:

```powershell
.\build-desktop.ps1 -Package
```

The archived Win32 UI is available through `./build.ps1 -Legacy`; see
[the legacy guide](LEGACY-DESKTOP.md). It is not the current Qt application.

## Linux desktop

On Ubuntu/Debian:

```bash
sudo apt install build-essential cmake ninja-build pkg-config qt6-base-dev \
  qt6-multimedia-dev qt6-wayland libgstreamer1.0-dev \
  libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad gstreamer1.0-libav python3 curl patchelf squashfs-tools
bash build-linux-desktop.sh
```

`bash build-linux-desktop.sh --appimage` also downloads the hash-checked
linuxdeploy tools, collects runtime dependencies/notices, and creates an AppImage.
The result depends on the build distribution's glibc; building an AppImage does
not make a newer glibc binary run on older systems.

## OBS plugin

On Windows, install OBS and the same compiler/SDK prerequisites:

```powershell
.\build-obs.ps1 -Setup
.\build-obs.ps1 -Package
```

Use `-ObsRoot` for another OBS installation. The default script pins OBS 31
headers and Qt 6.6.3; use a matching SDK/runtime when targeting another OBS build.

On Linux:

```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev libobs-dev obs-studio python3
bash build-linux.sh
bash build-linux.sh --package
```

This build uses the distribution's libobs, OBS frontend and Qt 6 development
files. The plugin does not link the standalone video-preview libraries.

## CMake

Examples for an installed Qt SDK:

```bash
cmake -S . -B build/cmake -DBUILD_QT_DESKTOP=ON -DBUILD_OBS_PLUGIN=OFF
cmake --build build/cmake --config Release
ctest --test-dir build/cmake -C Release --output-on-failure
```

For the plugin, set `BUILD_QT_DESKTOP=OFF` and `BUILD_OBS_PLUGIN=ON`. Set
`CMAKE_PREFIX_PATH` if Qt/OBS are installed outside the usual search paths.
On Windows, CMake can use `TINY_OBS_SOURCE_DIR` and `TINY_OBS_LIBRARY_DIR` prepared
by the OBS build script.

## Verification

- Build scripts run tests for presets/protocol, UI state and the enabled platform
  backend. OBS tests use an isolated frontend fixture; that fixture is not shipped.
- `python -m unittest discover -s tests -p "test_release_*.py"` checks archive
  contents and rejection of personal preset data.
- `TinyPTZ --check-runtime` constructs the desktop UI without connecting a camera.
- Linux `--check-preview-runtime` decodes a generated MJPEG stream and exercises
  the preview path without a physical camera.
- Windows `-HardwareTest` is opt-in. It exercises the connected camera and restores
  its state afterward. Close competing camera applications before running it.
- Windows `build\desktop\ptzctl.exe small-step-test` checks repeated one-step pan
  and tilt changes for drift on the untouched axis and slow changes followed by
  duplicate release targets, then restores the starting
  position and power state. It physically moves the camera; disable tracking and
  close competing camera applications before running it.

A passing camera-free test does not establish compatibility with every firmware,
USB driver, display server or OBS/Qt combination. Include actual tested versions
and hardware when reporting results.
