# Legacy Win32 desktop app

The archived v0.3.4 interface remains in source for compatibility and comparison.
The supported desktop release uses Qt; see the root [README](../README.md).

On Windows with Visual Studio C++ Build Tools and the Windows SDK:

```powershell
.\build.ps1 -Legacy
.\build.ps1 -Legacy -Package
```

The build produces `build/v034/TinyPTZ.exe` and the Windows `ptzctl.exe` CLI.
The optional package is named `TinyPTZ-Legacy-v0.3.4-Windows-x64-Release.zip`.
It contains an empty preset library. It does not copy personal presets or local
investigation files. Import/copy your own library separately after extraction.

This UI uses native Win32 controls and DirectShow preview. It does not provide
the Qt app's themes, shared dock layout or modern Linux preview. Camera commands
and preset serialization use the shared core; see [settings coverage](SETTINGS.md).
Back up preset files before using them with older application versions.

Current desktop and OBS builds are described in [BUILDING.md](BUILDING.md).
