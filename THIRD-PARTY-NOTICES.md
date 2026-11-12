# Third-party components

Tiny PTZ is GPL version 2 or later; see LICENSE.txt. Application source is
available in this repository and the corresponding public source archive.
No proprietary OBSBOT Center binary or vendor SDK is distributed or loaded.

## Windows desktop releases

The portable desktop app includes dynamically linked Qt 6.6.3 Core, GUI,
Widgets and the Windows platform plugin, copyright The Qt Company and
contributors. The Qt runtime can be replaced by a compatible modified build.
Qt license texts are supplied under `licenses/Qt` in the Windows release.

The pinned development/runtime package is the official OBS dependency build:
https://github.com/obsproject/obs-deps/releases/tag/2024-09-12

Qt Base source, including its third-party attributions, is available at:
https://download.qt.io/archive/qt/6.6/6.6.3/submodules/

The Visual C++ runtime DLLs come from the installed Visual Studio
redistributable. Windows supplies DirectShow, COM and camera drivers.

## Linux desktop releases

The reference AppImage is built on Ubuntu 24.04 using Qt 6.4.2 and GStreamer
1.24. The image includes its dynamically linked runtime libraries/plugins and
collected Debian package copyright notices under `usr/share/doc`. Referenced
common license texts are included there. An extracted AppDir permits compatible
runtime libraries to be replaced. Host glibc, the kernel and graphics drivers
are not bundled.

Qt sources: https://download.qt.io/archive/qt/6.4/6.4.2/submodules/
GStreamer sources: https://gstreamer.freedesktop.org/src/
Ubuntu package sources: https://archive.ubuntu.com/ubuntu/pool/

AppImage packaging uses official linuxdeploy and Qt deployment tools; their
SHA256 hashes are pinned in `packaging/linux/appimage-tools.sha256`. Build tools
are not shipped in the application image.

## OBS plugin

The plugin links OBS's public libobs/frontend APIs and Qt supplied by OBS.
Its minimal release does not bundle OBS, Qt or an OBSBOT SDK. Build/version
compatibility is described in the platform guides under `docs/`.

OBS source and licensing: https://github.com/obsproject/obs-studio
