# Tiny PTZ OBS plugin for Linux — v0.6.1

Native Linux OBS dock for the OBSBOT Tiny SE: PTZ, tracking/image/device settings,
unlimited presets, scene automation, full backups and scene-name restore matching.
OBS provides video capture; this plugin opens only camera controls.

## Install the Linux release

The supplied x86_64 binary is built on **Ubuntu 24.04, glibc 2.39, OBS 30.0.2 and
Qt 6.4.2**. Use a native Qt 6 OBS installation (OBS 30+). Other/older distributions
may need a build against their own OBS/Qt libraries; do not copy Windows DLLs or
Qt runtime libraries into OBS. `ldd bin/64bit/tiny-ptz-obs.so` lists dependencies.

Close OBS, extract the archive, and copy `tiny-ptz-obs` into:

```text
~/.config/obs-studio/plugins/
```

Use `$XDG_CONFIG_HOME/obs-studio/plugins` if XDG_CONFIG_HOME is customized. Layout:

```text
tiny-ptz-obs/
  bin/64bit/tiny-ptz-obs.so
  data/presets.ptz
  README.md
  LICENSE.txt
```

Start OBS and enable **Docks → Tiny PTZ**. Do not install another copy of this
plugin under a different folder name: both would register the same dock.
The minimal release has an empty initial preset library, without personal data.
Full sources/build scripts are available in the separate source archive.

Flatpak OBS uses
`~/.var/app/com.obsproject.Studio/config/obs-studio/plugins`. This native distro
binary has not been validated against Flatpak's runtime; build against its SDK
if needed. The sandbox must expose the actual camera video device and sysfs USB
descriptors. Snap/sandbox installations may similarly restrict camera access.

## Camera access and usage

Select the Tiny SE and click **Connect / read**. The driver must be `uvcvideo`,
and your user needs read/write access to its capture node and read access to USB
descriptors in sysfs. Device identities prefer `/dev/v4l/by-id`, then by-path,
then `/dev/videoN`. Metadata-only video nodes are excluded. No root process,
libusb driver detach, device-format change, or streaming start/stop is required
by the control backend. OBS continues to own its video capture stream.

If no camera appears, check `ls -l /dev/v4l/by-id` and the permissions on the
resolved `/dev/videoN`. Log-in session ACLs or membership in the system's `video`
group normally provide access. Do not run OBS as root or make devices world-writable.
Missing/unsupported V4L2 controls are disabled in the settings list.

PTZ sliders apply live. Position values refresh automatically every 400 ms while
connected and pause while you edit. Use **Settings** for tracking, image and device controls.
Presets include saved camera settings when the capture checkbox is enabled.
Scene automation follows the Program scene, not Studio Mode Preview selection.
**Back up plugin...** and **Restore plugin...** include presets and assignments;
restore prefers scene UUIDs and falls back to unambiguous names, as on Windows.

### Importing Windows presets or changing camera identity

Camera device IDs differ between Windows and Linux. Import your `.ptz` file or
restore a full backup, select the destination Linux camera, then click
**Assign presets to camera...**. Choose the original camera identity and confirm.
All its presets and scene references are rebound together; duplicate destination
names get numbered suffixes. The old preset file is preserved. No camera movement
is triggered by rebinding. This also handles IDs changed by USB ports/reinstalls.
If the Linux driver does not expose a setting saved by Windows, recall reports
that setting instead of silently dropping it; save a supported-settings or
PTZ-only preset for that driver.

The standard exposure setting is normalized to the Windows preset convention
(log2 seconds); Linux's native exposure control uses 100-microsecond units.
Pan/tilt controls convert between UI degrees and V4L2 arcseconds. Native Tiny SE
motor telemetry remains the position-readback source, preserving negative tilt
rounding. Vendor tracking/device commands use the same verified protocol packets
through `UVCIOC_CTRL_QUERY`; the extension ID is discovered by GUID from USB
descriptors instead of hard-coded or blindly probed unit numbers.

## Build on Linux

Example native Ubuntu/Debian development dependencies:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build qt6-base-dev libobs-dev obs-studio
./build-linux.sh --package
```

Or configure manually:

```bash
cmake -S . -B build/linux -G Ninja \
  -DBUILD_QT_DESKTOP=OFF -DBUILD_OBS_PLUGIN=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux -j
QT_QPA_PLATFORM=offscreen ctest --test-dir build/linux --output-on-failure
cmake --install build/linux --prefix "$PWD/dist/linux/tiny-ptz-obs"
```

Linux support currently targets the OBS plugin, not the standalone DirectShow
preview application. Windows source/builds remain supported separately.

## Verification and limits

The Linux build and tests run in Ubuntu WSL. Tests cover vendor descriptor parsing,
V4L2 control mapping, auto/manual modes, exposure/angle conversion, permissions and
range failures through a mocked ioctl transport, UTF-8 file paths and atomic
presets, backup/restore/rebinding, Qt dock behavior and loading the real plugin
through libobs with an isolated frontend fixture. The fixture is not packaged.

**A physical Tiny SE has not been exercised by this Linux build.** USB control,
stream sharing and the installed native OBS GUI should be validated on the target
Linux PC. Boot-position writes retain the validated Tiny SE zoom-layout guard;
driver/firmware controls not reported by the device are not assumed supported.
Windows camera protocol and hardware tests remain prior evidence, not a claim
of Linux hardware validation. The plugin does not create OBS scenes or translate
arbitrary camera identities automatically.

GPL version 2 or later; see LICENSE.txt. No proprietary OBSBOT SDK, OBS/Qt runtime,
personal presets, or development libraries are included in the minimal release.

API references: [Linux UVC extension queries](https://www.kernel.org/doc/html/latest/userspace-api/media/drivers/uvcvideo.html),
[V4L2 camera controls](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/ext-ctrls-camera.html).
