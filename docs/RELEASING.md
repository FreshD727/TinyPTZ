# Creating public releases

The desktop application and OBS plugin have separate versions. Do not relabel
an old binary with a new version: build each target before packaging it.

## Runtime packages

The standard release commands are:

```powershell
.\build-desktop.ps1 -Setup -Package
.\build-obs.ps1 -Setup -Package
```

```bash
bash build-linux-desktop.sh --appimage
bash build-linux.sh --package
```

Python 3.10+ is needed for source/ZIP packaging. Release ZIPs are built from
explicit inputs, not an existing `dist/` folder. User libraries/preferences,
investigation files, downloaded SDKs and test frontend binaries are excluded.
The only bundled `.ptz` file is the empty `obs/data/presets.ptz` template;
packaging fails if that template contains preset records.

For an existing Linux AppImage, create its release ZIP with:

```bash
python3 scripts/package_release.py desktop-linux --binary dist/TinyPTZ-vVERSION-Linux-x86_64.AppImage
```

The packager also supports `desktop-windows`, `obs-windows` and `obs-linux`;
run `python scripts/package_release.py --help` for their inputs. Use `--version`
only when intentionally packaging an independently versioned build.

## Public source archive

```bash
python3 scripts/package_release.py source
```

In a checkout, this includes public files in the Git index, using their current
working-tree contents. Add intended new files to Git before running it. From a
source archive without Git, the packager uses the same public directory/file
allowlist. It does not include `.git`, local research, build trees or user data.

## Release review

1. Run the enabled target's tests and the archive tests in [BUILDING.md](BUILDING.md).
2. Check the built version and package naming; keep desktop/plugin versions separate.
3. Extract the ZIP to a fresh location. Run its runtime check and verify that the
   preset library is empty. Test actual camera behavior when claiming hardware support.
4. Check the README, license texts, third-party notices and source archive.
5. Generate SHA256 checksums for the final files. Publish matching source alongside
   binaries and retain the dependency source/provenance required by their licenses.
6. Tag the reviewed commit and upload only the reviewed release assets. This
   repository does not publish automatically.

## Existing history

The project's development history is retained. Local reverse-engineering notes
and hardware logs were removed from the current public tree, but remain in older
commits. This cleanup does not rewrite that history. The public source ZIP exports
only the current public files and contains no Git history.
