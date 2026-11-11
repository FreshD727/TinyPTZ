# Camera-control reconstruction

## Target and method

Target: locally installed `OBSBOT Center/bin/libdev.dll`, x64 PE,
2,074,424 bytes. SHA-256:
`e23a445bb8874f2426a53d6269876a8dd5a478d1c9c0a6e1196fc8fffc8bc994`.
The connected camera's USB identity is VID 3564 / PID FEFF and its Windows
friendly name is OBSBOT Tiny SE StreamCamera. The XU product string says
"Tiny 2 Lite StreamCamera"; that string alone is not used to identify the device.

REA 6.8.0 was installed locally and registered for Codex. The project MCP client
performed `inspect_desktop_application`, `inspect_pe_imports`, and
`inspect_with_objdump` (section headers). Retained JSON files record the
evidence, hashes, and limitations. No decompiler engine was installed.
Targeted assembly inspection used Microsoft's `dumpbin` separately; those
listings are ordinary local analysis, not REA decompilation output.

The follow-up fix used binary/code inspection and CLI camera probes only.
No desktop automation, UI clicking, OBSBOT process attachment, firmware changes,
or USB driver replacement was used for the fix.

## Recovered behavior

The DLL's exported `cameraSetDevRunStatusR` is at RVA 24230.
Its legacy branch uses a two-byte payload: `01 <state>` with run=1 and sleep=0.
RVA EE320 constructs legacy headers; EDCB0 serializes header fields and writes
the CRC. V3 has distinct encoders at EE430 / EDD50.

`probe-encoder.py` loads the installed DLL and calls only the encoder/checksum
routines on allocated buffers. It checks the exact DLL SHA-256 before using
RVAs, and does not enumerate or write to a camera. `encoder-reference.json`
retains the results. These are generated sample packets, not USB captures.
The gimbal encoder samples are not used by the final controller.

The verified Tiny SE wake packet at sequence 1 begins:

```
aa 00 0e 10 00 01 ed 97 00 e1 13 c2 01 01
```

It is padded to 60 bytes and sent to XU selector 2. CRC-16/USB covers the first
14 bytes with CRC bytes zero; the token is stored big-endian. The original
encoder's packet is a regression-test fixture. V3 packets were accepted by the
Windows transport but produced no mailbox reply or wake on this camera.

`cameraSetDisableSleepWithoutStreamU` at RVA 24950 sends a zero-padded
60-byte selector-6 record beginning `12 01 <bool>`. Without this guard, the
camera parks its tilt between commands when no video stream is running.
The controller holds this guard during active control, releases it on Sleep
or disconnect, and checks the selector-6 sleep status at byte 2. The guard
setting itself was not observable in status readback, so prior guard-value
restoration is not claimed. A forced process termination cannot run cleanup.

The XU descriptor GUID occurs at file offset 1B81E8:
`9A1E7291-6843-4683-6D92-39BC7906EE49`.
It is accessed through `IKsTopologyInfo` and `IKsControl`, without vendor DLLs.
Pan, tilt and zoom use `IAMCameraControl` after verified wake. The camera
reports pan -130..130, tilt -90..90, and zoom 0..12, each with step 1.
These are driver-reported ranges, not a claim about every mechanical pose.

## Validation

`hardware-validation.txt` contains the final portable-build run. Each command
was followed by driver position readback after a settle interval. The test
moves pan and tilt five degrees and zoom one step, saves four presets to a
separate file, reloads and recalls all four, and restores its starting position
and power state. It allows one degree of pan/tilt readback tolerance. No video
was captured or stored. Driver readback is evidence of reported position;
this was not an independent optical measurement.

Core tests cover the original encoder fixture, 1,000 Unicode/quoted presets,
PTZ persistence, range quantization, malformed records, and preserving the
original preset file on failed writes.

The desktop GUI and optional preview have not been exercised through UI
automation at the user's request. The reusable control core is shared with
the verified CLI. The optional OBS module is source scaffolding and has not
been built/tested against an OBS SDK.

## References consulted

- [REA official source](https://github.com/morluto/rea)
- [Microsoft IAMCameraControl](https://learn.microsoft.com/en-us/windows/win32/api/strmif/nn-strmif-iamcameracontrol)
- [OBSBOT SDK overview](https://www.obsbot.com/sdk)
- [Independent Tiny 2 protocol research](https://github.com/lxman/obsbot-mcp/blob/master/PROTOCOL.md)

The Tiny 2 V3 description was a starting hypothesis; the Tiny SE implementation
was corrected against the locally installed binary and physical camera probes.
