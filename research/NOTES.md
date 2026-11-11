> Current power implementation: see the v0.3.1 correction at the end. Earlier
> legacy-wake/no-stream-guard conclusions below are retained investigation history
> and are superseded by the live-stream and installed-SDK evidence.

# Camera settings and preset coverage

Open **Settings...**, select Tracking, Image, or Device, select a row, edit its
value and choose **Apply selected setting**. Automatic is enabled only for
controls whose Windows driver advertises it. Unsupported settings are disabled.
Writes with readable state must pass camera readback before reporting success.
Gain requires manual exposure on the tested Tiny SE. Recall applies saved exposure
before gain so manual image presets work regardless of their file key order. Sleep-background selection requires an already
uploaded slot; this app cannot upload one.
Firmware-dependent modes may be rejected even when their enum is listed.

| Category | Implemented controls |
| --- | --- |
| Tracking | Off/group/human/hand modes; firmware-dependent whiteboard/desk modes; normal, upper-body, close-up, headless and lower-body framing; standard/headroom/motion style; left/right hand selection |
| Tracking configuration | Tracking/zoom/dynamic-zoom/reverse-zoom gestures; gesture zoom factor; reversed gimbal direction; composition X/Y; zone enable and automatic target selection; four zone boundaries; zone home pan/pitch/zoom; four hand-tracking boundaries |
| Image | Brightness, contrast, hue, saturation, sharpness, white balance, exposure compensation (-9..9, neutral 0), gain, focus, exposure; automatic white balance/focus/exposure where supported; HDR, face-priority exposure/focus, anti-flicker, field of view and mirror |
| Device | Microphone noise reduction, microphone during sleep, automatic microphone gain, reception distance, auto-sleep timeout, startup tracking/framing, existing sleep-background slot and background mirror |
| Set-only requests | Image style (standard/text/landscape/portrait/nightscape/film) and AI automatic zoom; current values cannot be read back on this implementation |
| Status | LED brightness is read-only; gamma is shown unavailable on the tested driver |

There are 54 setting/status rows. The tested Tiny SE returned 50 readable,
writable settings. This count can vary with the device and driver. Tracking
mode enums come from the installed application protocol; exposing a mode does
not establish that Tiny SE firmware implements its subject recognition.

## Saving settings with presets

The main window's **Include tracking, image and device settings when saving
presets** checkbox defaults to checked. Save and Update capture all available
readable/writable settings, including automatic/manual flags. Set-only values
are included only after this app has requested them in the current connection.
They are labeled **requested**, because a successful USB write cannot establish
that firmware applied them. Reconnecting clears that request history.

Uncheck the checkbox to save PTZ only. Existing PTZ-only presets continue to
work. Updating one with the checkbox enabled adds settings. The list shows the
number of settings stored in each preset. Settings are snapshots of current
camera state, not a separate editable settings profile per preset.

Recall checks setting availability before changing the camera. If the preset
contains a tracking mode, recall pauses AI tracking for the PTZ movement and
restores the requested tracking mode last. Image style is applied before
individual image adjustments. Active tracking can change framing after recall.
On failure the core attempts to restore previous readable settings and PTZ;
set-only requests cannot be reliably rolled back.

Presets include device-global settings such as startup behavior and microphone
configuration. Recalling them changes the camera's current configuration for
other applications too. Automatic exposure/focus/white-balance presets save
the automatic mode, not a frozen measured value.

The v2 UTF-8 file stores unlimited preset records. Each record has a settings
count followed by quoted key, numeric value, and automatic flag triples.
Existing v1 files are readable; their first save keeps a `.v1.backup`. Old
versions of Tiny PTZ cannot read v2 files.

## Remaining original-app features

This release does not implement the complete OBSBOT Center feature set:

- PC-side beauty, background blur/replacement, virtual-camera output, recording,
  and video processing are not implemented. They require a host video pipeline.
- Firmware updates, factory reset, custom sleep-image upload, accounts, and
  vendor service integration are not implemented.
- Original on-camera preset slots and multi-position zone-preset management
  are not imported or edited. This app stores its own unlimited presets.
- LED brightness is read-only, gamma is unavailable on the tested driver, and
  image-style/automatic-zoom requests have no verified device readback.

The desktop settings window is compiled but was not exercised by UI automation,
per the request to work through application code. Hardware tests use the same
backend through the CLI and verify configuration changes, restoration, and
disk-backed preset recall. They do not assess visual tracking quality. The OBS
module was still an unbuilt integration scaffold at this stage. The v0.4.0 work
below replaces it with a built dock plugin; see OBS-PLUGIN.md.

## Image defaults (v0.3)

**Default selected** restores the selected Image control; **Restore image
defaults** restores all available image defaults. Standard controls use the
Windows driver's reported default values, with automatic mode for supported
white balance, exposure, and focus. Exposure Compensation is displayed as
-9..9 and resets to 0 (driver value 9). Older `backlight` preset values in
0..18 are translated to the new `exposure_compensation` key when loading.

Vendor controls use this explicit standard reset profile: HDR off, anti-flicker
50 Hz, face-priority exposure/focus on, wide field of view, mirror off, standard
image style. Unsupported controls are skipped. Image style remains a set-only
request. This is an image reset, not a device factory reset; it leaves tracking,
microphone, and stored boot configuration alone. Gain resets while exposure is
briefly manual, then exposure returns to its default automatic mode.

The Settings editor preserves the selected row across Apply and camera refresh,
so repeated numeric/dropdown edits remain available. A hidden native-control
regression test checks this without opening or controlling a desktop window.

## Device boot preset (v0.3)

Select a library preset and choose **Set as boot position**. Its PTZ values are
stored in the camera, with readback verification. No Windows startup task or
background app is used. The camera retains one boot position; subsequent
changes to the local preset do not automatically update it. Other settings in
the preset are not copied into boot state. Device / Startup tracking mode can
still enable tracking after startup, which may move the camera away afterward.

The Tiny SE 0..12 driver zoom range is converted to the device's 1..4 zoom ratio,
and driver tilt is converted to the device's positive-down pitch. Other driver
zoom layouts are rejected. Hardware tests verified storing a pose, retaining it
after reconnecting, and firmware recall at pan 5 / tilt 5 / zoom 1, then restored
the original boot record and current camera state. Actual USB power cycling
has not been tested.

## GUI/defaults and boot position (v0.3)

The Apply issue was list reconstruction losing selection and disabling the
button. The editor now preserves the setting key, restores row selection, and
refreshes its editor state. `tests/settings_ui_tests.cpp` exercises numeric and
dropdown readback refresh repeatedly under a hidden parent window; it does not
use desktop automation. Default metadata comes from driver GetRange, plus the
explicit vendor image profile documented in docs/SETTINGS.md.

Exposure Compensation maps display -9..9 to driver 0..18 by adding 9 on write
and subtracting 9 on read. Existing v2 `backlight` records are migrated by key
and value, once, on load. Unit tests cover that migration and round trip.
`image-controls-validation.txt` records -9/+9/0 writes and original-value restore.

Boot behavior was recovered from installed libdev.dll using the existing
Microsoft dumpbin listing and hash-checked in-memory encoder oracle. Setter
aiSetGimbalBootPosR is RVA 166E0 (subsystem 3/index 07; V3 3844); getter is
168D0 (index 08; 3884); firmware recall is 16DB0 (index 0A; 3904). Empty SET
payloads use flags 05 rather than 25. The Tiny SE returns a 24-byte boot pose:
ID u32, pan/pitch/roll/zoom/b_pitch float32. Setter preserves the record ID.
Driver tilt has the opposite sign to pitch; zoom conversion is 1+driver/4.
The original cameraGetZoomAbsoluteR model-specific branch uses constant .25
at RVA 1BEB38. The implementation rejects other zoom ranges instead of guessing.
An initial V3 GET timed out; subsequent requests worked. Boot reads retry up to
three times. The legacy getter did not return a response; it is not used.

`boot-validation.txt` verifies stored readback, persistence after destroying and
reopening the camera control connection, and firmware-triggered boot-position
recall with matching driver PTZ values. The original boot record, PTZ, AI mode,
and sleep state were restored by the test. No physical power cycle was performed.
The new UI stores only PTZ into boot state, not the preset's whole settings
snapshot. It does not install a startup process or change startup tracking mode.

## Sleep/wake correction (v0.3.1)

The reported defect affected both Tiny PTZ and OBS. Gimbal state alone was not
sufficient evidence that video resumed. An independent FFmpeg DirectShow stream
was kept open throughout sleep/wake tests, with only in-memory frame statistics
retained. Early tests reproduced Run status while capture stopped delivering
frames. Tests do not save or display camera pictures.

The original executable's imports and callsites were inspected with dumpbin.
`cameraSetDisableSleepWithoutStreamU` is documented for Meet cameras; the main
app gates it to product types 5, 6, 10 and 13, excluding Tiny SE (12). It has been
removed from this controller. A separate, hash-checked installed-SDK probe read
the actual connected camera's protocol field: version 3, USB transport 0.

The native SDK passed three uninterrupted capture cycles. Development-only
in-process instrumentation in `sdk-wire-probe.cpp` observed its encoder and
IKsControl transfer. It did not attach to or automate a desktop application.
Power uses XU selector 2, protocol-3 command A0C2, uint32 0=run / 1=sleep. The
fixed 60-byte USB transfer also has byte 32 set to 1. Replaying only the logical
20-byte frame with zero padding did not recover streaming. Including byte 32,
with all incidental camera-specific padding removed, passed all three cycles.
The exact firmware meaning of this compatibility byte is not established; it
is retained based on observed interoperability, not a guessed field name.

The final report builder has separate header and payload CRC-16/USB checksums,
little-endian sequence, and the verified fixed-report byte. Unit tests compare
power fixtures and reject omission of byte 32 or camera-ID padding. Explicit
Wake is never skipped solely because the device already reports Run. Internal
PTZ checks avoid repeatedly sending power commands after a confirmed session
wake. Sleep completion requires state 3, not merely any non-Run state.

`power-video-sdk-reference.txt` records the installed-SDK comparison.
`power-video-marker.txt` records the minimal report experiment.
`power-video-after.txt` records the final application-backend regression, with
fresh packets continuing at about 30 fps and image differences from the sleep
screen. No video source was reopened during the cycles. The test leaves the
camera awake. Cold USB power cycling and testing every OBS capture mode are
outside this regression. `check-power-video.py` is a development test requiring
local FFmpeg, not a dependency of the portable application.

The SDK wire observer is version-locked research code. Its compiled DLL and
raw device-specific captures stay under ignored build/; neither the installed
SDK nor its observer is loaded by the portable app. The incorrect four-byte
payload in a legacy envelope was rejected by hardware and is not used. The
old legacy encoder fixture remains historical research, not the production
power implementation.

Reproducing the optional development observer (never needed to build/run Tiny PTZ):

```powershell
. .\build.ps1
& $compiler @common /LD research/sdk-wire-probe.cpp /Fobuild/sdk-wire-probe.obj /Febuild/sdk-wire-probe.dll build/ptz_core.lib ole32.lib oleaut32.lib strmiids.lib user32.lib
@('sleep','wake','quit') | python research/probe-sdk-device.py --power-session --wire-probe
```

Only use this with the exact hash-checked installed SDK and this source layout.
The simpler final regression uses `python research/check-power-video.py build/ptzctl.exe`.
The production application has no dependency on either Python, FFmpeg, the
observer, or the vendor SDK.

## Native position readback (v0.3.2)

The reported preset 17/-4/0 reproduced driver readback 17/-3/0. Native gimbal
telemetry (V3 command 6604, original aiGetGimbalStateR RVA 162D0) showed motor
pan 16.9 and motor pitch +4.0, i.e. user tilt -4.0. The SDK conversion helper at
RVA 161C0 decodes nine signed little-endian int16 angle/speed fields scaled by
0.1. For this Tiny SE the response is 24 bytes (18 angle/speed bytes plus extra
status). Motor roll/pitch/yaw are at offsets 6/8/10; the first three fields are
Euler orientation and must not be mistaken for the motor coordinates.

Read position now rounds measured motor yaw and negative motor pitch to the
nearest whole degree, with symmetric negative handling. It does not echo a
cached command or add an extra degree of movement. Zoom still uses the driver.
Unsupported telemetry layouts are rejected. Sleeping-camera parked readback
continues to use the driver because native telemetry is not available asleep.

Recall replaces a fixed one-second wait with three consecutive matching native
position readings (bounded by 60 polls) before restoring saved AI tracking.
`position-precision-validation.txt` records three disk-backed recalls of the
reported pose from different directions, plus a positive-tilt case. All rounded
readbacks matched exactly; native tilt ranged -3.9..-4.1, while the driver kept
reporting -3. The test restored the original rounded pose, tracking mode and
sleep state. This demonstrates numeric/readback consistency and sub-degree
servo variation, not an independent optical measurement of camera direction.

## Numeric slider UI (v0.3.3)

A shared `NumericSlider` adapter maps native trackbar ticks to metadata step
indices, rather than casting fractional values or negative ranges to raw
trackbar positions. Separate 32-bit range messages and TBM_GETPOS avoid the
16-bit packed scroll-position limit. It supports numeric Settings controls and
the main Pan/Tilt/Zoom fields. Automatic/read-only/unavailable controls disable
manual entry. Slider edits stage values for the existing Apply actions.

`settings_ui_tests` drives actual hidden trackbar/edit/checkbox controls through
window messages. It verifies -9..9 mapping, .01 fractional steps, a 100000-step
range, typed-value synchronization, partial input, automatic toggling, and
selection/Apply behavior after refresh. No camera access or desktop automation
was needed for this UI-only change.

## Live slider adjustment (v0.3.4)

`LiveSliderEdits` coalesces by setting/axis, limits intermediate writes to an
80 ms cadence, and forces the release value. A recurring 25 ms window timer is
started once, rather than restarted on every mouse event (which would debounce
all writes until the drag stops). Readback waits 150 ms of idle input and never
runs while the trackbar has mouse capture. UI rows update without rebuilding
selection or resetting the dragged thumb. No extra COM thread/connection is
introduced; all camera interfaces stay on their owning UI thread.

`applySetting(value,false)` retains validation and writes immediately without
ACK/readback delays. `verifySetting` performs selected-setting readback without
rewriting. Existing calls default to verified application. A filtered settings
query avoids scanning unrelated camera controls for each verification.

`live-slider-validation.txt` records hardware standard/raw/scalar/block writes,
final readback, and restoration. Observed intermediate writes took 0-16 ms for
brightness, composition and timeout, and about 47 ms for zone-home read/modify/
write. Native-control tests verify the leading live update, continuous-drag
progress, newest-value coalescing, release flush, mouse-capture verification
suppression, no writes from typing, and clearing failures without modal popups.

## Native OBS dock (v0.4.0, 2026-11-12)

Replaced the unbuilt hotkey scaffold with a Qt dock using the shared camera core.
The local OBS runtime is 31.0.0-beta3 with Qt 6.6.3. `build-obs.ps1` pins official
OBS 31.0.0-beta3 source headers and the 2024-09-12 Qt development archive with
SHA-256 checks; dependencies stay under ignored `build/obs-sdk`. Public runtime
exports provide import libraries. The plugin and its copy of the core use `/MD`,
while the standalone app retains its static CRT and existing portable package.

All camera interfaces live on one dedicated COM STA thread, including teardown.
Named pending jobs coalesce slider and scene bursts. Qt callbacks carry values,
not COM objects. Live sliders throttle writes and verify final settings without
blocking the OBS UI. The plugin opens no video preview graph. Native controls
expose PTZ, all shared-core settings/defaults, unlimited preset editing/capture,
and boot position storage. Default presets are copied from the packaged library
only on first use; an external portable library can be opened directly.

Scene rules use collection names and source UUIDs. Program scene callbacks apply
saved PTZ/settings, Preview events do nothing, unmapped scenes cancel pending
recalls, and automation starts disabled. Startup waits for FINISHED_LOADING.
In-flight camera operations finish before the newest queued recall. Rename and
delete operations in the dock maintain preset references in scene assignments.
EXIT stops the worker and removes the dock while frontend APIs remain valid;
QPointer guards module teardown if the host has already destroyed its widgets.

Both direct MSVC and CMake builds passed. CTest passed core, native settings UI
and dock tests (3/3). An isolated frontend fixture loaded the actual plugin DLL
through installed libobs, testing native dock registration, actual OBS scenes,
scene/preview callbacks, UUID rename persistence, disconnected-camera reporting,
and unloading the module before destroying its Qt host window. Test fixtures
are never packaged. The real Tiny SE passed dock connection, settings/PTZ reads,
live brightness application/readback and scene-driven recall. Original state was
restored; one bounded cleanup retry allowed a power transition to settle.
Windows-font renders of the plugin's own hidden widgets were visually reviewed.
No desktop UI automation or changes to the user's OBS configuration were used.

`obs-plugin-validation.txt` records the results. Live OBS streaming/layout and
visual tracking quality remain manual integration checks after installation.
