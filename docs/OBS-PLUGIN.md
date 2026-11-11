# OBS Studio port

The portable app is the primary deliverable. The optional OBS module is source
scaffolding, not a tested or packaged plugin.

The core does not depend on the desktop UI. Both `TinyPTZ.exe` and `ptzctl.exe`
link the same static `ptz_core` library. An OBS module can link it too, with no
runtime process or proprietary SDK dependency. COM must be initialized on the
thread opening the camera, and camera interfaces must stay on that thread.

The included module registers preset recall hotkeys and uses the same core
recall path for PTZ plus saved tracking, image and device settings. To experiment:

1. Install the matching OBS development SDK and configure CMake with
   `-DBUILD_OBS_PLUGIN=ON` and `CMAKE_PREFIX_PATH` pointing at libobs.
2. Copy the built module into an OBS portable installation's plugin folder.
3. Put an exported `presets.ptz` into the module config directory returned by
   `obs_module_config_path`. Restart OBS to register changed presets.
4. Assign the registered actions in OBS Settings / Hotkeys.

Production work remaining: persist hotkey assignments with OBS APIs,
marshal camera work to a dedicated COM worker,
add a Qt dock for selecting cameras and editing presets, expose scene-change
actions, and package/test against a specific OBS version. Do not ship this
scaffold as a production plugin until those lifecycle checks are implemented.

Avoid opening a second preview graph inside OBS: use the existing video source
for the image and open only the camera's control interface. Validate driver
sharing with the intended OBS capture configuration.
