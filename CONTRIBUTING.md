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

## Formatting

Install the Git hook in your Python 3.10+ environment after cloning:

```bash
python -m pip install -r requirements-dev.txt
python -m pre_commit install --install-hooks
```

Commits format staged C/C++ files using `.clang-format`, format Python with Black,
and check whitespace, line endings, YAML and JSON. If a formatter changes files,
review and stage those changes, then commit again. Unstaged work is preserved.

To format all tracked files manually, run `python -m pre_commit run --all-files`.
Formatter versions are pinned in `.pre-commit-config.yaml`. Generated builds,
releases and local investigation directories are excluded.

GitHub Actions runs the same checks on pushes and pull requests. Formatting
violations fail the check and display the changes needed in the workflow log.

Keep generated output and personal data out of commits. Application icons belong
in `assets/`; runtime packaging code belongs in `packaging/` or `scripts/`.
Public changes are made under the project's GPL version 2-or-later license.
