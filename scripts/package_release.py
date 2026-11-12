"""Create clean runtime/source archives from explicit public inputs.

Python 3.10+. ZIPs are written atomically; existing release folders and user
data are never copied. Research, SDKs and Git history are never source inputs.
"""
import argparse
import io
import os
from pathlib import Path
import re
import stat
import subprocess
import tarfile
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIRS = ('assets', 'docs', 'obs', 'packaging', 'scripts', 'src', 'tests', '.github')
SOURCE_FILES = ('.clang-format', '.gitattributes', '.gitignore', 'CMakeLists.txt',
                'README.md', 'LICENSE.txt', 'THIRD-PARTY-NOTICES.md', 'CONTRIBUTING.md',
                'build.ps1', 'build-desktop.ps1', 'build-obs.ps1', 'build-linux.sh',
                'build-linux-desktop.sh')


def version(kind):
    if kind.startswith('obs-'):
        match = re.search(r'\[tiny-ptz\]\s+([0-9.]+)\s+dock', (ROOT / 'obs/plugin.cpp').read_text())
    else:
        match = re.search(r'project\(TinyPTZ VERSION ([0-9.]+)', (ROOT / 'CMakeLists.txt').read_text())
    if not match:
        raise ValueError('Cannot determine release version.')
    return match[1]


def read(path):
    path = Path(path)
    if not path.is_file() or path.is_symlink():
        raise ValueError(f'Missing regular release input: {path}')
    return path.read_bytes()


def empty_presets():
    content = read(ROOT / 'obs/data/presets.ptz')
    if content.decode('utf-8-sig').strip() != 'TINY_PTZ_PRESETS 2':
        raise ValueError('The bundled preset template contains records; refusing to package user data.')
    return b'TINY_PTZ_PRESETS 2\n'


def guide(name):
    """Keep source-only documentation links understandable in minimal ZIPs."""
    path = ROOT / 'docs' / name
    content = read(path).decode('utf-8')
    def link(match):
        label, target = match.groups()
        if '://' in target or target.startswith('#'):
            return match[0]
        if Path(target).name == 'LICENSE.txt':
            return f'[{label}](LICENSE.txt)'
        source = (path.parent / target).resolve().relative_to(ROOT).as_posix()
        return f'{label} (see `{source}` in the source archive)'
    return re.sub(r'\[([^\]]+)\]\(([^)]+)\)', link, content).encode('utf-8')


def source_entries():
    if (ROOT / '.git').exists():
        result = subprocess.run(['git', '-c', 'core.fsmonitor=false', 'ls-files', '-z', '--',
                                 *SOURCE_DIRS, *SOURCE_FILES], cwd=ROOT, check=True, capture_output=True)
        paths = [ROOT / name.decode('utf-8') for name in result.stdout.split(b'\0') if name]
    else:
        paths = [ROOT / name for name in SOURCE_FILES if (ROOT / name).is_file()]
        paths += [path for folder in SOURCE_DIRS for path in (ROOT / folder).rglob('*') if path.is_file()]
    entries = {}
    for path in sorted(set(paths)):
        if not path.is_file():
            continue
        name = path.relative_to(ROOT).as_posix()
        if any(part in {'__pycache__', '.git', 'build', 'dist', 'research', 'tools'} for part in Path(name).parts):
            continue
        if path.name in {'app.json', 'dock.json'} or path.suffix in {'.pyc', '.exe', '.dll', '.so', '.zip'}:
            continue
        if path.name == '.env' or path.name.startswith('.env.') or path.name.endswith('.tinyptz.json'):
            continue
        if path.suffix == '.ptz':
            if name != 'obs/data/presets.ptz':
                continue
            entries[name] = empty_presets()
        else:
            entries[name] = read(path)
    if 'scripts/package_release.py' not in entries or 'CMakeLists.txt' not in entries:
        raise ValueError('Public source inputs are incomplete; add new source files to Git before packaging.')
    return entries


def executable(name):
    return name.endswith(('.sh', '.AppImage', '.so')) or Path(name).name in {'AppRun', 'TinyPTZ'}


def write_zip(path, prefix, entries):
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=path.name + '.', suffix='.tmp', dir=path.parent)
    os.close(descriptor)
    try:
        with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            for name, data in sorted(entries.items()):
                if name.startswith('/') or '..' in Path(name).parts:
                    raise ValueError(f'Unsafe archive member: {name}')
                info = zipfile.ZipInfo(prefix + '/' + name)
                info.create_system = 3
                info.external_attr = (stat.S_IFREG | (0o755 if executable(name) else 0o644)) << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                archive.writestr(info, data)
        os.replace(temporary, path)
    finally:
        Path(temporary).unlink(missing_ok=True)
    print(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kind', choices=('source', 'desktop-windows', 'desktop-linux', 'desktop-legacy-windows', 'obs-windows', 'obs-linux'))
    parser.add_argument('--version')
    parser.add_argument('--binary', type=Path)
    parser.add_argument('--cli', type=Path)
    parser.add_argument('--qt-root', type=Path)
    parser.add_argument('--vc-runtime', type=Path)
    parser.add_argument('--qt-licenses', type=Path)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'dist')
    args = parser.parse_args()
    release_version = args.version or version(args.kind)
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', release_version):
        parser.error('Invalid version string.')
    if args.kind == 'source':
        name = f'TinyPTZ-v{release_version}-Source'
        write_zip(args.output_dir / (name + '.zip'), name, source_entries())
        return
    if not args.binary:
        parser.error('--binary is required for runtime packages.')
    entries = {'LICENSE.txt': read(ROOT / 'LICENSE.txt')}
    if args.kind.startswith('obs-'):
        linux = args.kind == 'obs-linux'
        platform = 'Linux-x86_64' if linux else 'Windows-x64'
        prefix = 'tiny-ptz-obs'
        name = f'TinyPTZ-OBS-v{release_version}-{platform}-Release'
        entries.update({'bin/64bit/tiny-ptz-obs.' + ('so' if linux else 'dll'): read(args.binary),
                        'data/presets.ptz': empty_presets(),
                        'README.md': guide('OBS-LINUX.md' if linux else 'OBS-PLUGIN.md')})
    elif args.kind == 'desktop-linux':
        prefix = f'TinyPTZ-v{release_version}-Linux-x86_64'
        name = prefix + '-Release'
        entries.update({prefix + '.AppImage': read(args.binary), 'README.md': guide('DESKTOP-LINUX.md')})
    elif args.kind == 'desktop-legacy-windows':
        prefix = f'TinyPTZ-Legacy-v{release_version}'
        name = prefix + '-Windows-x64-Release'
        entries.update({'TinyPTZ.exe': read(args.binary), 'README.md': guide('LEGACY-DESKTOP.md'),
                        'data/presets.ptz': empty_presets()})
        if args.cli:
            entries['ptzctl.exe'] = read(args.cli)
    else:
        if not all((args.qt_root, args.vc_runtime, args.qt_licenses)):
            parser.error('Windows desktop packaging needs --qt-root, --vc-runtime and --qt-licenses.')
        prefix = f'TinyPTZ-v{release_version}'
        name = prefix + '-Windows-x64-Release'
        entries.update({'TinyPTZ.exe': read(args.binary), 'README.md': guide('DESKTOP-WINDOWS.md'),
                        'THIRD-PARTY-NOTICES.md': read(ROOT / 'THIRD-PARTY-NOTICES.md'),
                        'data/presets.ptz': empty_presets(),
                        'platforms/qwindows.dll': read(args.qt_root / 'plugins/platforms/qwindows.dll')})
        for library in ('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll'):
            entries[library] = read(args.qt_root / 'bin' / library)
        runtimes = list(args.vc_runtime.glob('*.dll'))
        licenses = list(args.qt_licenses.rglob('*.txt'))
        if not runtimes or not licenses:
            raise ValueError('Visual C++ runtime or Qt license inputs are missing.')
        for library in runtimes:
            entries[library.name] = read(library)
        for license_path in licenses:
            entries['licenses/Qt/' + license_path.relative_to(args.qt_licenses).as_posix()] = read(license_path)
    write_zip(args.output_dir / (name + '.zip'), prefix, entries)
    if args.kind == 'obs-linux':
        archive_path = args.output_dir / (name + '.tar.gz')
        with tarfile.open(archive_path, 'w:gz') as archive:
            for member, data in sorted(entries.items()):
                info = tarfile.TarInfo(prefix + '/' + member)
                info.size = len(data)
                info.mode = 0o755 if executable(member) else 0o644
                archive.addfile(info, io.BytesIO(data))
        print(archive_path)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
