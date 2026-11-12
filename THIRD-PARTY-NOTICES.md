# Third-party components

Tiny PTZ's portable Qt desktop distribution includes Qt 6.6.3 Core, GUI, Widgets
and the Windows platform plugin, copyright The Qt Company and contributors.
The Qt runtime is dynamically linked and may be replaced with a compatible
modified build. Qt license texts are in `licenses/Qt`; upstream Qt Base source,
including third-party attributions, is in
`sources/qtbase-everywhere-src-6.6.3.tar.xz`.

The development/runtime package is the official OBS dependencies release
`2024-09-12` (`windows-deps-qt6-2024-09-12-x64.zip`). Build tooling and provenance:
https://github.com/obsproject/obs-deps/tree/2024-09-12
Upstream Qt Base: https://download.qt.io/archive/qt/6.6/6.6.3/submodules/

Tiny PTZ application source and build scripts are included in
`sources/TinyPTZ-source.zip`, provided under GPL version 2 or later (see
`LICENSE.txt`). Qt's own license choices and embedded third-party notices are
preserved in the supplied Qt source and license directory.

The Microsoft Visual C++ runtime DLLs are redistributed from the Visual Studio
VC redist directory. Windows supplies DirectShow, COM and the camera driver.
No OBSBOT Center binary or proprietary vendor SDK is included or required.
