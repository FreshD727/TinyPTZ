"""Collect installed Debian copyright notices for the bundled ELF libraries."""
import pathlib
import shutil
import subprocess
import sys

stage = pathlib.Path(sys.argv[1])
basenames = set()
for path in stage.rglob('*'):
    if path.is_file():
        with path.open('rb') as stream:
            if stream.read(4) == b'\x7fELF':
                basenames.add(path.name)
packages = set()
for name in sorted(basenames):
    result = subprocess.run(['dpkg-query', '-S', '*/' + name], text=True, capture_output=True)
    for line in result.stdout.splitlines():
        if ': /' in line:
            packages.add(line.split(': /', 1)[0].split(':', 1)[0])
destination = stage / 'usr/share/doc/third-party'
destination.mkdir(parents=True, exist_ok=True)
for package in sorted(packages):
    notice = pathlib.Path('/usr/share/doc') / package / 'copyright'
    if notice.is_file():
        shutil.copyfile(notice, destination / (package + '.copyright'))
licenses = pathlib.Path('/usr/share/common-licenses')
shutil.copytree(licenses, destination / 'common-licenses', dirs_exist_ok=True)
(destination / 'README.txt').write_text(
    'Debian package copyright notices for bundled libraries. Referenced common license texts\n'
    'are in common-licenses/. Libraries remain dynamically linked and replaceable in an\n'
    'extracted AppDir. System kernel, glibc and graphics drivers are not bundled.\n'
    'Qt sources: https://download.qt.io/archive/qt/6.4/6.4.2/submodules/\n'
    'Ubuntu dependency sources: https://archive.ubuntu.com/ubuntu/pool/\n', encoding='utf-8')
print('Collected third-party notices for', len(packages), 'packages')
