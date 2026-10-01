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
            names = list(guides.ANDROID_GUIDES) if android else list(guides.GUIDES) + list(guides.DESKTOP_REFERENCES) + [
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

    def test_missing_local_definitions_fails(self):
        with tempfile.TemporaryDirectory() as folder:
            archive = self.make_archive(Path(folder), exclude='Celestial_Navigation_Definitions.html')
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
            self.assertTrue(text.isascii(), path)
            self.assertIn('http-equiv="Content-Type" content="text/html; charset=utf-8"', text)
            parser.feed(text)
            self.assertNotIn('<script', text.lower())
            for ref in parser.references:
                parsed = urlsplit(ref)
                if parsed.scheme or parsed.netloc:
                    online = {'page-01.html': 'https://github.com/rgleason/celestial_navigation_pi/issues/131',
                              'definitions.html': 'https://www.siranah.de/html/sail040e.htm#a2'}
                    self.assertEqual(ref, online.get(path.name))
                    continue
                if parsed.path:
                    target = (path.parent / unquote(parsed.path)).resolve()
                    self.assertTrue(target.is_relative_to((ROOT / 'data').resolve()), ref)
                    self.assertTrue(target.is_file(), str(target))
        for image in assets.glob('*.png'):
            self.assertEqual(image.read_bytes()[:8], b'\x89PNG\r\n\x1a\n')

    def test_overview_is_complete_and_has_a_reading_size_preview(self):
        import json
        import struct
        assets = ROOT / 'data/practical-guide'
        overview = (assets / 'page-04.html').read_text()
        self.assertEqual(overview.count('<img '), 1)
        audit = json.loads((ROOT / 'validation/practical-guide-2.8.16/html-conversion.json').read_text())
        self.assertEqual(audit['pages'][3]['figures'][0]['clip'], [0, 0, 1920, 1080])
        for page in audit['pages']:
            for figure in page['figures']:
                full = struct.unpack('>II', (assets / figure['file']).read_bytes()[16:24])
                preview = struct.unpack('>II', (assets / figure['preview']).read_bytes()[16:24])
                self.assertLessEqual(preview[0], 762)
                self.assertLess(preview[0], full[0])
                self.assertAlmostEqual(preview[0] / preview[1], full[0] / full[1], delta=.02)

    def test_pdf_link_audit_matches_bundled_documents(self):
        import hashlib
        import json
        report = json.loads((ROOT / 'validation/practical-guide-2.8.16/pdf-links.json').read_text())
        self.assertEqual(report['unchanged_rendered_pages'], 66)
        self.assertEqual(report['sha256'], hashlib.sha256((ROOT / 'data/Practical_Guide.pdf').read_bytes()).hexdigest())
        self.assertEqual(len(report['links']), 3)
        self.assertEqual(report['links'][1]['destination'], 'Practical_Guide.pdf')
        self.assertEqual(report['links'][1]['action'], 'GoTo')
        self.assertEqual(report['links'][1]['page'], 67)
        self.assertEqual(report['pages'], 66 + report['appendix_pages'])
        self.assertGreater(report['appendix_pages'], 0)
        self.assertEqual(report['definitions_source_sha256'],
                         hashlib.sha256((ROOT / 'data/Celestial_Navigation_Definitions.html').read_bytes()).hexdigest())
        self.assertEqual(report['links'][2]['page'], 32)
        for link in report['links']:
            if link['offline']:
                self.assertTrue((ROOT / 'data' / link['destination']).is_file())

    def test_definitions_appendix_contains_the_complete_source_and_internal_navigation(self):
        import re
        class BodyText(HTMLParser):
            def __init__(self):
                super().__init__()
                self.inside, self.parts = False, []
            def handle_starttag(self, tag, attrs):
                if tag == 'body':
                    self.inside = True
            def handle_endtag(self, tag):
                if tag == 'body':
                    self.inside = False
            def handle_data(self, text):
                if self.inside:
                    self.parts.append(text)
        source = BodyText()
        source.feed((ROOT / 'data/Celestial_Navigation_Definitions.html').read_text())
        appendix_path = ROOT / 'data/practical-guide/definitions.html'
        appendix = BodyText()
        markup = appendix_path.read_text()
        appendix.feed(markup)
        def normal(text):
            text = text.replace('\u00a0', ' ').replace('\u2011', '-').replace('\u2013', '-').replace('\u2014', ' - ')
            return ' '.join(text.split())
        self.assertIn(normal(''.join(source.parts)), normal(''.join(appendix.parts)))
        contents = (ROOT / 'data/Practical_Guide.html').read_text()
        first = (ROOT / 'data/practical-guide/page-01.html').read_text()
        last = (ROOT / 'data/practical-guide/page-66.html').read_text()
        self.assertIn('practical-guide/definitions.html#definitions-section-0', contents)
        self.assertIn('href="definitions.html"', first)
        self.assertIn('href="definitions.html"', last)
        self.assertIn('href="../Practical_Guide.html"', markup)
        for anchor in re.findall(r'practical-guide/definitions\.html#([^"\s]+)', contents):
            self.assertIn('name="' + anchor + '"', markup)
        with tempfile.TemporaryDirectory() as folder:
            archive = self.make_archive(Path(folder), exclude='practical-guide/definitions.html')
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
