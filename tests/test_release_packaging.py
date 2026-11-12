import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

WORKSPACE = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('package_release', WORKSPACE / 'scripts/package_release.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class ReleasePackagingTests(unittest.TestCase):
    def setUp(self):
        directory = WORKSPACE / 'build' / 'release-tests'
        directory.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=directory)
        self.root = Path(self.temporary.name).resolve()
        self.assertTrue(self.root.is_relative_to(directory.resolve()))

    def tearDown(self):
        self.temporary.cleanup()

    def write(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        return path

    def test_public_source_excludes_local_and_personal_data(self):
        self.write('CMakeLists.txt', b'project(TinyPTZ VERSION 1.0.0)')
        self.write('scripts/package_release.py', b'# public packaging code')
        self.write('src/example.cpp', b'// public source')
        self.write('src/app.json', b'private preferences')
        self.write('src/backup.tinyptz.json', b'private scene backup')
        self.write('src/.env', b'private environment')
        self.write('src/personal.ptz', b'private presets')
        self.write('research/private.txt', b'private notes')
        self.write('dist/saved.ptz', b'private presets')
        self.write('obs/data/presets.ptz', b'TINY_PTZ_PRESETS 2\n')
        with patch.object(package, 'ROOT', self.root):
            entries = package.source_entries()
        self.assertIn('src/example.cpp', entries)
        self.assertIn('obs/data/presets.ptz', entries)
        self.assertFalse(any('private' in data.decode() for data in entries.values()))
        self.assertNotIn('src/app.json', entries)

    def test_nonempty_template_is_rejected(self):
        self.write('obs/data/presets.ptz', b'TINY_PTZ_PRESETS 2\n"Personal room" "camera" 0 0 0 0\n')
        with patch.object(package, 'ROOT', self.root), self.assertRaisesRegex(ValueError, 'user data'):
            package.empty_presets()

    def test_archive_replaces_old_contents_and_sets_linux_permissions(self):
        archive = self.root / 'release.zip'
        package.write_zip(archive, 'tiny-ptz-obs', {'private.json': b'old private file'})
        package.write_zip(archive, 'tiny-ptz-obs', {'bin/64bit/plugin.so': b'new binary', 'README.md': b'instructions'})
        with zipfile.ZipFile(archive) as result:
            self.assertEqual(set(result.namelist()), {'tiny-ptz-obs/bin/64bit/plugin.so', 'tiny-ptz-obs/README.md'})
            self.assertEqual(result.getinfo('tiny-ptz-obs/bin/64bit/plugin.so').external_attr >> 16 & 0o777, 0o755)
            self.assertIsNone(result.testzip())


if __name__ == '__main__':
    unittest.main()
