# Tiny PTZ for Linux — v0.7.1

Standalone Qt controller for the OBSBOT Tiny SE with live PTZ controls, unlimited
presets, tracking/image/device settings, camera auto-connect, dark/light/system
themes and an optional camera preview. OBS Studio and OBSBOT Center are not needed.

v0.7.1 replaces the Linux GPU video widget with software frame rendering for
systems where capture activates but the picture stays black, including the
reported Fedora 44 AppImage case. Preview becomes live only after decoding a
frame; missing/stalled streams time out with an error. The Fedora laptop still
needs a verification run with this updated release.

## Run the AppImage

The x86_64 release is built on Ubuntu 24.04 with glibc 2.39 and Qt 6.4.2. Use
Ubuntu 24.04 or a compatible newer Linux distribution. An AppImage bundles Qt
and its video dependencies; it still uses the host's kernel, graphics drivers
and glibc. Older distributions need a build against their own libraries.

```bash
chmod +x TinyPTZ-v0.7.1-Linux-x86_64.AppImage
./TinyPTZ-v0.7.1-Linux-x86_64.AppImage
```

If your system lacks FUSE, run it with `--appimage-extract-and-run`, or extract
it once using `--appimage-extract` and run `squashfs-root/AppRun`.

Select the camera and click **Connect** the first time. Leave automatic
connection checked to reconnect on later launches. PTZ position updates
automatically; sliders apply live. Use **Start preview** for video and **Stop
preview** before another app captures the camera. Preview stays off at startup.
Qt Multimedia matches the selected device node, including stable-device symlinks.
No microphone is opened or recorded.

The camera must use the `uvcvideo` driver. Your user needs read/write access to
its `/dev/video*` capture node and read access to its USB descriptors in sysfs.
Normal desktop device ACLs or membership in the `video` group provide this;
avoid running the app as root. A camera absent from WSL must be attached to
Linux before it can be controlled. Stop other camera controllers if they conflict.

## Presets and portability

By default, data is written to `$XDG_DATA_HOME/TinyPTZ`, normally
`~/.local/share/TinyPTZ`. Back up that directory to preserve `presets.ptz` and
`app.json`. The application never writes settings inside its read-only image.
For a portable folder beside the AppImage, use an absolute path:

```bash
./TinyPTZ-v0.7.1-Linux-x86_64.AppImage --data-dir "$PWD/TinyPTZ-data"
```

The preset import/export buttons transfer `.ptz` libraries. Windows and Linux
camera IDs differ. After importing presets from Windows or moving cameras,
select the destination camera and use **Assign presets to camera...** to rebind
them. Supported settings can be saved per preset. Linux controls absent from a
particular driver remain disabled; recalling an unsupported saved setting reports
it. Boot presets use the same camera-firmware commands as the Linux OBS plugin.

On Qt 6.4 the System theme uses the desktop palette at startup; explicit Light
and Dark themes work independently of desktop theme detection.

## Build

On Ubuntu/Debian install:

```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-multimedia-dev \
  qt6-wayland gstreamer1.0-plugins-good gstreamer1.0-plugins-bad \
  gstreamer1.0-libav curl patchelf squashfs-tools python3
./build-linux-desktop.sh
./build-linux-desktop.sh --appimage
```

The packaging script downloads the official linuxdeploy and Qt deployment tools
to `build/appimage-tools`, validates their pinned SHA256 hashes, and bundles the
Qt/platform/multimedia plugins and camera-preview dependencies. No OBS libraries,
personal presets, build tools or compiler files are included in the AppImage.
`--check-runtime` constructs the application without opening a camera and exits.

Native GUI/core/backend tests and packaged-runtime checks are run in WSL.
Physical Tiny SE video capture and PTZ operation on Linux require a separate
hardware check; the WSL build environment does not currently expose that camera.

GPL version 2 or later; see LICENSE.txt. Third-party license notices are included
under `usr/share/doc` inside the AppImage. Full application source is supplied
in the corresponding source archive.
