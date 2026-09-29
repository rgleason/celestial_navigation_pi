"""Release-version and archive checks; no GUI, network or extraction required."""
import io
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'ci'))
from plugin_version import plugin_version
import bundled_guides as guides


class BundledGuides(unittest.TestCase):
    def make_archive(self, folder, exclude=None, corrupt=None, duplicate=None):
        path = folder / 'test.tar.gz'
        with tarfile.open(path, 'w:gz') as archive:
            for name in guides.GUIDES:
                if name == exclude:
                    continue
                data = (ROOT / 'data' / name).read_bytes()
                if name == corrupt:
                    data = b'not the approved guide'
                entry = tarfile.TarInfo('package/share/plugin/data/' + name)
                entry.size = len(data)
                archive.addfile(entry, io.BytesIO(data))
                if name == duplicate:
                    archive.addfile(entry, io.BytesIO(data))
        return path

    def test_exact_guides_pass(self):
        with tempfile.TemporaryDirectory() as folder:
            self.assertEqual(len(guides.check_guides(self.make_archive(Path(folder)), ROOT)), 3)

    def test_missing_corrupt_and_duplicate_guides_fail(self):
        with tempfile.TemporaryDirectory() as folder:
            for case in ('exclude', 'corrupt', 'duplicate'):
                with self.subTest(case=case):
                    archive = self.make_archive(Path(folder), **{case: 'Practical_Guide.pdf'})
                    with self.assertRaises(ValueError):
                        guides.check_guides(archive, ROOT)

    def test_version_follows_cmake_and_rejects_ambiguity(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            text = '\n'.join('set(VERSION_%s "%s")' % pair for pair in
                             zip(('MAJOR', 'MINOR', 'PATCH', 'TWEAK'), ('2', '8', '99', '0')))
            (root / 'CMakeLists.txt').write_text(text)
            self.assertEqual(plugin_version(root), '2.8.99.0')
            (root / 'CMakeLists.txt').write_text(text + '\nset(VERSION_PATCH "98")')
            with self.assertRaises(ValueError):
                plugin_version(root)


if __name__ == '__main__':
    unittest.main()
