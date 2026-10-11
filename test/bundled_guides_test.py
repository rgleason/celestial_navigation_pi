"""Offline reader/link completeness and platform-specific packaged guides."""
import hashlib
import io
import json
from html.parser import HTMLParser
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from urllib.parse import unquote, urlsplit

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'ci'))
import bundled_guides as guides

class BundledGuides(unittest.TestCase):
    def archive(self,path,android=False,omit=None,corrupt=None):
        names=list(guides.ANDROID_GUIDES if android else guides.GUIDES+guides.DESKTOP_REFERENCES)
        if not android: names += ['practical-guide/'+p.name for p in (ROOT/'data/practical-guide').glob('*') if p.is_file()]
        with tarfile.open(path,'w:gz',compresslevel=1) as t:
            for name in names:
                if name==omit: continue
                data=(ROOT/'data'/name).read_bytes()
                if name==corrupt: data=b'bad document'
                entry=tarfile.TarInfo('share/opencpn/plugins/celestial_navigation_pi/data/'+name)
                entry.size=len(data); t.addfile(entry,io.BytesIO(data))

    def test_complete_desktop_and_android_archives(self):
        with tempfile.TemporaryDirectory() as d:
            for android in [False,True]:
                p=Path(d)/('test-android-arm64.tar.gz' if android else 'test.tar.gz')
                self.archive(p,android); guides.check_guides(p,ROOT)
            self.archive(p,False)
            with self.assertRaises(ValueError): guides.check_guides(p,ROOT)

    def test_missing_or_corrupt_guide_and_illustration_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'test.tar.gz'
            for name in ['Practical_Guide.pdf','Practical_Guide.html','practical-guide/page-17-figure-1.png']:
                self.archive(p,omit=name)
                with self.assertRaises(ValueError): guides.check_guides(p,ROOT)
            self.archive(p,corrupt='Practical_Guide.pdf')
            with self.assertRaises(ValueError): guides.check_guides(p,ROOT)

    def test_reader_pages_and_links_are_complete_and_offline(self):
        class Links(HTMLParser):
            def __init__(self): super().__init__(); self.links=[]
            def handle_starttag(self,tag,attrs):
                self.links += [v for k,v in attrs if k in ('src','href')]
        pages=list((ROOT/'data/practical-guide').glob('page-??.html'))
        self.assertEqual(len(pages),77)
        for p in [ROOT/'data/Practical_Guide.html']+list((ROOT/'data/practical-guide').glob('*.html')):
            text=p.read_text(); self.assertTrue(text.isascii()); self.assertNotIn('<script',text.lower())
            links=Links(); links.feed(text)
            for ref in links.links:
                parsed=urlsplit(ref)
                if parsed.scheme or parsed.netloc:
                    self.assertIn(ref, ['https://github.com/rgleason/celestial_navigation_pi/issues/131',
                                        'https://www.siranah.de/html/sail040e.htm#a2'])
                    self.assertNotIn('src="'+ref+'"', text)
                    continue
                target=(p.parent/unquote(parsed.path)).resolve()
                self.assertTrue(target.is_relative_to((ROOT/'data').resolve()))
                self.assertTrue(target.is_file(),target)

    def test_pdf_and_fix_screenshots_match_document_audit(self):
        report=json.loads((ROOT/'validation/fix-guide-2.9.7/guide-audit.json').read_text())
        self.assertEqual(report['pdf_sha256'],hashlib.sha256((ROOT/'data/Practical_Guide.pdf').read_bytes()).hexdigest())
        self.assertEqual(report['pages'],77)
        self.assertEqual(report['updated_fix_pages'],[5,17])
        self.assertTrue((ROOT/'data/Practical_Guide.pdf').read_bytes().startswith(b'%PDF-'))
        for page in report['page_records']:
            p=ROOT/'data/practical-guide'/('page-%02d.png'%page['page'])
            self.assertEqual(page['image_sha256'],hashlib.sha256(p.read_bytes()).hexdigest())
        self.assertIn('latest included sight', (ROOT/'data/practical-guide/page-17.html').read_text())
        self.assertNotIn('Default is &#x201c;your boat position', (ROOT/'data/practical-guide/page-17.html').read_text())

    def test_reflowed_reader_preserves_source_prose_and_complete_illustrations(self):
        class Text(HTMLParser):
            def __init__(self): super().__init__(); self.parts=[]
            def handle_data(self, value): self.parts.append(value)
            def handle_endtag(self, tag):
                if tag in ('p','li','h1','h2','h3','td'): self.parts.append(' ')
            def plain(self): return ' '.join(''.join(self.parts).split())
        report=json.loads((ROOT/'validation/fix-guide-2.9.7/html-conversion.json').read_text())
        self.assertEqual(report['source_sha256'], hashlib.sha256((ROOT/'data/Practical_Guide.pdf').read_bytes()).hexdigest())
        self.assertEqual(len(report['pages']),70)
        def prose(value):
            # Actual HTML list markers replace the PDF's literal bullet glyphs.
            return ' '.join(value.replace('\u2022',' ').replace('\u25cf',' ').split())
        for page in report['pages']:
            n=page['page']; main=(ROOT/'data/practical-guide'/('page-%02d.html'%n)).read_text()
            self.assertNotIn('Selectable transcription',main)
            parser=Text(); parser.feed(main)
            combined=parser.plain()
            for figure in page['figures']:
                self.assertGreater(figure['width'],0); self.assertGreater(figure['height'],0)
                figure_html=ROOT/'data/practical-guide'/figure['file'].replace('.png','.html')
                parser=Text(); parser.feed(figure_html.read_text()); combined += ' '+parser.plain()
            for text in page['source_text']:
                self.assertIn(prose(text),prose(combined),'source prose on page '+str(n))
        self.assertEqual(report['definitions_text_blocks'],107)
        parser=Text(); parser.feed((ROOT/'data/practical-guide/definitions.html').read_text())
        for text in report['definitions_source_text']:
            self.assertIn(prose(text),prose(parser.plain()))
        contents=(ROOT/'data/Practical_Guide.html').read_text()
        self.assertIn('Altitude sights and coastal navigation',contents)
        self.assertIn('practical-guide/definitions.html',contents)

if __name__=='__main__': unittest.main()
