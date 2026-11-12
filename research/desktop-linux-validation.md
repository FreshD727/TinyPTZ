# Linux standalone v0.7.1 validation

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
