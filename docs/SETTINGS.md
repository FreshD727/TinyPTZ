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
dock plugin now exposes the same core controls and settings, with Program scene
preset automation; see [the plugin guide](OBS-PLUGIN.md) for its build and tests.

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

## Sleep and wake (v0.3.1)

Wake now uses the Tiny SE's verified protocol-3 USB report, including its
fixed-report compatibility byte. It sends a real Wake request even when the
gimbal reports Running. The Meet-only no-stream guard has been removed.
Independent capture tests kept a stream open through three sleep/wake cycles
and verified fresh live frames after each Wake. Close older Tiny PTZ copies
before running this version; no OBS source restart was used in the test.

Firmware can park the gimbal when no video is being captured. Use Tiny PTZ
preview when working alone, or keep the OBS camera source active while using
controls without preview.

## Preset position readback (v0.3.2)

Pan and tilt while awake use native motor-angle telemetry, rounded to the
nearest whole degree with the same rule for positive and negative angles.
Windows driver tilt can truncate toward zero: the tested 17/-4/0 pose reported
17/-3/0 through the driver despite native tilt readings around -4 degrees.
The UI now uses the measured native angle rather than that truncated value.
Zoom units and existing preset numbers are unchanged; no one-degree bias is
added to movement commands.

Recall waits for three matching measured position readings before applying
saved tracking behavior. A position that does not settle produces an error.
Tracking can still change framing after recall. Mechanical repeatability is
not exact below one degree; native readings in the reported example ranged
from -3.9 to -4.1 degrees and correctly round to -4.

## Numeric sliders (v0.3.3)

Every setting edited as a number has a slider alongside its number field.
The slider uses the setting's range and step, including negative Exposure
Compensation, fractional composition offsets and gesture zoom, and large
sleep-timeout ranges. Dragging updates the number; typing a complete number
updates the slider. Partial input such as a minus sign is preserved while
editing. **Apply selected setting** sends the chosen value to the camera.

Manual sliders and number fields are disabled while Automatic is checked.
Uncheck it to edit a manual value, then Apply. Read-only and unsupported
controls remain disabled. Named choices retain their existing selectors.

The main window also has Pan, Tilt and Zoom sliders using the connected
camera's driver ranges. They stay synchronized with the position fields;
**Apply position** sends all three values. Sliders are disabled until a camera
is connected. The Settings button has moved to the top right to make room.

Native-control regression tests run under a hidden parent window and exercise
negative/fractional/large ranges, both synchronization directions, partial
text input, automatic mode, and repeated Apply selection preservation. This
UI change does not alter camera commands or the preset file format.

## Live slider adjustment (v0.3.4)

Numeric setting sliders and Pan/Tilt/Zoom sliders now send changes while you
drag. No Apply click is required for slider movements. Number fields still
follow the thumb; typed edits and named selectors use their existing Apply
buttons. Automatic/read-only/unsupported controls remain protected.

Updates are throttled to about one per 80-100 ms, retaining the newest value
for each setting/axis. Releasing sends the final pending value promptly.
Settings readback is deferred until input is idle and the slider releases
mouse capture, so verification does not repeatedly block a drag. Live errors
appear in the details/status area. Pending changes are flushed before closing,
manual Apply, defaults, preset actions, and camera changes. PTZ movement still
takes time mechanically; allow it to settle before saving the current pose.

The live write path performs normal range/mode validation but skips synchronous
ACK waits and full-setting scans during intermediate writes. Final verification
reads only the affected setting. Native-control tests cover updates before
release, coalescing, final-value delivery, delayed verification, typed input,
and failure recovery. Hardware tests exercised standard, raw, scalar, and
read-modify-write controls and restored their original values.
