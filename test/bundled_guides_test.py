"""Release-version and archive checks; no GUI, network or extraction required."""
import io
from html.parser import HTMLParser
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'ci'))
from plugin_version import plugin_version
import bundled_guides as guides


class BundledGuides(unittest.TestCase):
    def make_archive(self, folder, exclude=None, corrupt=None, duplicate=None, android=False):
        path = folder / ('plugin-android-arm64.tar.gz' if android else 'test.tar.gz')
        with tarfile.open(path, 'w:gz') as archive:
            names = list(guides.ANDROID_GUIDES) if android else list(guides.GUIDES) + [
                'practical-guide/' + p.name for p in
                sorted((ROOT / 'data/practical-guide').glob('*')) if p.is_file()]
            for name in names:
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
            self.assertEqual(len(guides.check_guides(self.make_archive(Path(folder)), ROOT)), 4)

    def test_missing_corrupt_and_duplicate_guides_fail(self):
        with tempfile.TemporaryDirectory() as folder:
            for case in ('exclude', 'corrupt', 'duplicate'):
                with self.subTest(case=case):
                    archive = self.make_archive(Path(folder), **{case: 'Practical_Guide.pdf'})
                    with self.assertRaises(ValueError):
                        guides.check_guides(archive, ROOT)

    def test_missing_illustration_fails(self):
        with tempfile.TemporaryDirectory() as folder:
            archive = self.make_archive(Path(folder), exclude='practical-guide/page-09-figure-1.png')
            with self.assertRaises(ValueError):
                guides.check_guides(archive, ROOT)

    def test_android_has_its_own_guide_not_desktop_instructions(self):
        with tempfile.TemporaryDirectory() as folder:
            archive = self.make_archive(Path(folder), android=True)
            self.assertEqual(set(guides.check_guides(archive, ROOT)), set(guides.ANDROID_GUIDES))
            archive = self.make_archive(Path(folder))
            target = Path(folder) / 'plugin-android-arm64.tar.gz'
            archive.replace(target)
            with self.assertRaises(ValueError):
                guides.check_guides(target, ROOT)

    def test_html_is_complete_offline_and_illustrated(self):
        class Links(HTMLParser):
            def __init__(self):
                super().__init__()
                self.references = []
            def handle_starttag(self, tag, attrs):
                for key, value in attrs:
                    if key in ('href', 'src'):
                        self.references.append(value)
        assets = ROOT / 'data/practical-guide'
        pages = sorted(assets.glob('page-??.html'))
        self.assertEqual(len(pages), 66)
        for path in [ROOT / 'data/Practical_Guide.html'] + sorted(assets.glob('*.html')):
            parser = Links()
            text = path.read_text(encoding='utf-8')
            parser.feed(text)
            self.assertNotIn('<script', text.lower())
            for ref in parser.references:
                parsed = urlsplit(ref)
                self.assertFalse(parsed.scheme or parsed.netloc, ref)
                if parsed.path:
                    target = (path.parent / unquote(parsed.path)).resolve()
                    self.assertTrue(target.is_relative_to((ROOT / 'data').resolve()), ref)
                    self.assertTrue(target.is_file(), str(target))
        for image in assets.glob('*.png'):
            self.assertEqual(image.read_bytes()[:8], b'\x89PNG\r\n\x1a\n')

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
