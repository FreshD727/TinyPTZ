# Linux standalone v0.7.5 validation

Built in Ubuntu 24.04 WSL, GCC 13, glibc 2.39 and Qt 6.4.2.

- Linux desktop/core/V4L2 backend CTest: 3/3 passed.
- GUI tests cover themes, Unicode/portable preset data, remembered cameras,
  automatic position UI, preset search, shutdown and the Qt video output.
- Linux preview tests submit a decoded video frame and verify it is cleared
  when preview stops. Dark and light offscreen screenshots were inspected.
- Staged AppDir launches without a camera. Bundled GStreamer V4L2, camerabin,
  wrapper source, MJPEG decoder/encoder, video conversion, appsink, encodebin
  and capsfilter factories all instantiate successfully.
- Desktop GUI tests also pass with the AppDir's Qt libraries, Qt plugins and
  GStreamer dependencies, rather than the development Qt installation.
- AppImage extraction/run launch check passes. Loader diagnostics resolve Qt
  Core/Gui/Widgets, Multimedia, offscreen and the GStreamer
  media plugin inside the extracted image.
- Windows standalone regression build/tests passed after the shared Qt port.
- Existing Linux OBS plugin CTest: 4/4 passed.
- linuxdeploy and Qt deployment tool downloads are SHA256 pinned; third-party
  copyright and common license texts are included in the AppImage.

Physical Tiny SE PTZ, camera settings and live video capture on Linux have not
been exercised: the camera is not exposed inside this WSL environment. Native
X11/Wayland runtime plugins are bundled, but the automated GUI runs use offscreen
rendering. The binary requires glibc 2.39 or newer; older distributions need a
native rebuild. The AppImage contains no personal presets/configuration and
uses XDG application data or an explicit --data-dir outside the mounted image.

Logs (generated under build/): linux-desktop-final-tests.log,
linux-appdir-check.log, linux-appimage-runtime.log, linux-appimage-final-build.log,
linux-appimage-final-check.log, linux-plugin-desktop-regression.log and
desktop/linux-desktop-regression.log.

## v0.7.1 black-preview correction

Linux preview uses a plain QVideoSink and QPainter image rendering, bypassing
QVideoWidget/RHI texture presentation. Tests verify actual RGB painted pixels
and CPU conversion/painting of YUYV and NV12 frames with no RHI context, plus
clearing on stop. The AppDir GUI tests pass using bundled Qt and GStreamer.
Camera activation no longer means live video: the first decoded frame confirms
live preview and a seven-second watchdog reports absent/stalled frames.
Windows regression tests pass. The reported Fedora 44 laptop is not locally
available; this candidate must still be verified with its physical camera.
Logs: build/linux-software-preview-tests.log, build/linux-preview-v071-package.log,
build/desktop/software-preview-regression.log.

## v0.7.2 opaque device IDs and camera startup

The pinned upstream Qt v6.4.2 qgstreamervideodevices.cpp assigns generated numeric
IDs to GstDevice entries. These IDs cannot be compared to /dev/video paths.
Direct paths/symlinks now take priority; opaque IDs can match only a unique,
normalized selected-camera name. Tests cover numeric IDs, enumeration-order
changes, duplicate-name rejection, canonical symlinks and rejection of a known
different device path. Missing entries no longer claim a permissions failure.

The upstream qgstreamercamera.cpp constructor also instantiates videotestsrc
before replacing it with the selected camera. This required plugin is now in
the AppImage and included in the bundled factory check. A QCamera construction
test checks the startup path without activating a physical stream.
Upstream reference: https://github.com/qt/qtmultimedia/tree/v6.4.2/src/plugins/multimedia/gstreamer
Logs: build/linux-preview-v072-package.log and build/desktop/opaque-camera-regression.log.
Fedora camera operation still needs verification on the user's laptop.

## v0.7.3 exact-device capture (replaces camera-name matching)

Preview now opens the canonical selected V4L2 node using GStreamer's v4l2src,
memory-mapped buffers, decodebin, videoconvert and a one-frame bounded appsink.
It does not enumerate Qt camera IDs or select a device by name. The opaque-ID
matching helper is removed. Supported MJPEG 720p30 is preferred, with supported
raw modes or native negotiation as fallbacks. Errors originate from the selected
video node/pipeline instead of generic name/permission guesses.

New tests stream a live generated pattern through actual JPEG encoding,
automatic decoding, RGBA conversion, appsink delivery and QWidget painting.
They check multiple frames, displayed pixels, two stream starts/stops and failure
on an absent device node. The generated MJPEG preview screenshot was inspected.
The AppDir tests run against bundled libraries. The final AppImage also runs
--check-preview-runtime to decode/display generated MJPEG without camera hardware.

Logs: build/linux-native-preview-tests.log, build/linux-preview-v073-package.log,
build/desktop/native-preview-regression.log. Fedora physical-camera verification
remains outstanding because that laptop/camera is outside this workspace.

## v0.7.4 application icons

The same PTZ camera artwork is embedded as a Qt resource on both platforms.
Windows also embeds a multi-resolution ICO as a native executable resource.
The Windows shell icon was extracted and visually inspected; the minimal
Windows runtime --check-runtime passed with only its bundled runtime files.
Linux uses desktop ID tinyptz and supports --install-desktop-entry to install
its launcher and PNG icon in the user's XDG data directory for Wayland menus
and docks. Registration was checked in an isolated XDG_DATA_HOME using desktop-
file-validate, GIO metadata, GLib argument parsing (including a path with spaces)
and the installed PNG signature. Linux AppImage UI/preview runtime checks and
both platform regression suites passed. The user reported v0.7.3 camera control
and preview working on Fedora; icon changes do not alter the capture backend.
Logs: build/linux-icon-package.log, build/linux-icon-launcher-check.log,
build/desktop/icon-build.log.

## v0.7.5 first-launch menu registration

Normal Linux GUI startup now installs/refreshes the user launcher and icon
without a command. Identical files are compared before writing, so later
launches do not rewrite them. A stale executable location or missing icon is
repaired on the next launch. Failures are logged without stopping the controller;
runtime/preview check modes skip automatic registration. --install-desktop-entry
remains available for explicit repair. The AppImage camera-preview self-check
and Linux core/GUI/backend tests pass. Automatic startup registration is checked
in an isolated XDG_DATA_HOME with the actual AppImage.
Logs: build/linux-auto-launcher-package.log and build/linux-auto-launcher-check.log.
