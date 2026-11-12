# Contributing

Tiny PTZ currently targets the OBSBOT Tiny SE on Windows and Linux. Before
opening a change, read the [build guide](docs/BUILDING.md) and the relevant
platform guide in `docs/`.

For a bug report, include your app/plugin version, operating system, camera
model/firmware, and steps to reproduce. For OBS issues, include the OBS and Qt
versions and whether OBS is native, portable or sandboxed. Remove device serial
numbers, personal preset libraries and scene/configuration data from shared logs.

Keep camera operations on the worker thread and preserve error reporting,
range checks and preset-file compatibility. Changes to live settings must retain
coalescing/final verification and must not accidentally send commands to a newly
selected camera. Prefer a focused regression test for behavioral changes.

Run the enabled target's build/tests and the release archive tests when changing
packaging. Hardware tests are opt-in; describe what you actually tested. A new
model, backend or OBS runtime needs its own evidence of compatibility.

Keep generated output and personal data out of commits. Application icons belong
in `assets/`; runtime packaging code belongs in `packaging/` or `scripts/`.
Public changes are made under the project's GPL version 2-or-later license.
