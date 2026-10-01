"""Append the complete bundled definitions and keep PDF navigation internal.

PyMuPDF and ReportLab are authoring dependencies only. The 66 original pages
keep their text and appearance. Regeneration is safe and does not append twice.
"""
import hashlib
import html
import json
from pathlib import Path
import tempfile
from functools import partial
import pymupdf as pdf
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen.canvas import Canvas
from reportlab.platypus import KeepTogether, Paragraph, SimpleDocTemplate
from guide_definitions import APPENDIX_TITLE, CORE_PAGES, SOURCE as DEFINITIONS, read_definitions, title_parts

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'data/Practical_Guide.pdf'
REPORT = ROOT / 'validation/practical-guide-2.8.16/pdf-links.json'
ACCURACY = 'https://github.com/rgleason/celestial_navigation_pi/issues/131'
REFERENCE = 'https://www.siranah.de/html/sail040e.htm#a2'


def normalized(value):
    return ' '.join(value.split())


def create_appendix(path, blocks):
    font_roots = [Path('/usr/share/fonts/TTF'), Path('/usr/share/fonts/truetype/dejavu')]
    fonts = next((p for p in font_roots if (p / 'DejaVuSans.ttf').is_file()), None)
    assert fonts, 'Install DejaVu Sans for authoring Unicode definitions'
    for name, suffix in [('Guide', ''), ('GuideBold', '-Bold'), ('GuideItalic', '-Oblique'),
                         ('GuideBoldItalic', '-BoldOblique')]:
        pdfmetrics.registerFont(TTFont(name, str(fonts / ('DejaVuSans' + suffix + '.ttf'))))
    pdfmetrics.registerFontFamily('Guide', normal='Guide', bold='GuideBold',
                                  italic='GuideItalic', boldItalic='GuideBoldItalic')
    base = dict(fontName='Guide', fontSize=10.5, leading=14.5, spaceAfter=8,
                textColor=colors.HexColor('#263442'), allowWidows=0, allowOrphans=0)
    styles = {'p': ParagraphStyle('Body', **base),
              'li': ParagraphStyle('Definition', leftIndent=13, bulletIndent=0,
                                   bulletFontName='Guide', **base)}
    for tag, size in [('h1', 18), ('h2', 14), ('h3', 11.5)]:
        styles[tag] = ParagraphStyle(tag, fontName='GuideBold', fontSize=size, leading=size * 1.3,
                                     textColor=colors.HexColor('#17365d'), spaceBefore=12,
                                     spaceAfter=8, keepWithNext=True)

    class AppendixDoc(SimpleDocTemplate):
        def afterFlowable(self, flowable):
            if hasattr(flowable, 'definition_heading'):
                level, title = flowable.definition_heading
                self.heading_pages.append([level, title, CORE_PAGES + self.page])

    document = AppendixDoc(str(path), pagesize=A4, leftMargin=48, rightMargin=48,
                           topMargin=62, bottomMargin=54,
                           title=APPENDIX_TITLE, author='Celestial Navigation contributors')
    document.heading_pages = []
    story, pending_headings = [], []
    for block in blocks:
        markup = block['markup']
        if block['kind'] == 'h1':
            title, revision = title_parts(block['text'])
            markup = html.escape(title)
            if revision:
                markup += '<br/><font name="Guide" size="9" color="#627484">' + html.escape(revision) + '</font>'
        paragraph = Paragraph(markup, styles[block['kind']],
                              bulletText='\u2022' if block['kind'] == 'li' else None)
        if block['kind'].startswith('h'):
            level = int(block['kind'][1])
            paragraph.definition_heading = (level, APPENDIX_TITLE if level == 1 else block['text'])
        if block['kind'].startswith('h'):
            pending_headings.append(paragraph)
        else:
            # Keep each definition whole and headings with their first entry.
            story.append(KeepTogether(pending_headings + [paragraph]))
            pending_headings = []
    assert not pending_headings

    def decorate(canvas, doc):
        width, height = A4
        canvas.setFillColor(colors.HexColor('#627484'))
        canvas.setFont('Guide', 8)
        canvas.drawString(48, height - 31, 'Celestial Navigation | How to Guide')
        canvas.drawRightString(width - 48, height - 31, 'Definitions appendix')
        canvas.setStrokeColor(colors.HexColor('#d6e0e8'))
        canvas.line(48, 42, width - 48, 42)
        canvas.drawString(48, 29, 'Abbreviations, definitions and measurements')
        canvas.drawRightString(width - 48, 29, 'Page ' + str(CORE_PAGES + doc.page))

    document.build(story, onFirstPage=decorate, onLaterPages=decorate,
                   canvasmaker=partial(Canvas, invariant=1))
    return document.heading_pages


def main():
    blocks = read_definitions()
    before = pdf.open(SOURCE)
    assert len(before) >= CORE_PAGES
    core_toc = [entry for entry in before.get_toc() if entry[2] <= CORE_PAGES]
    assert len(core_toc) == 28 and [e[2] for e in core_toc if e[1] == 'Part II - Lunar distances'] == [32]
    labels = ['Accuracy, Goals, Precision and Testing',
              'Celestial Navigation Definitions', 'Lunar Distance Use Case']
    with tempfile.TemporaryDirectory() as folder:
        appendix_path = Path(folder) / 'definitions.pdf'
        headings = create_appendix(appendix_path, blocks)
        appendix = pdf.open(appendix_path)
        result = pdf.open()
        result.insert_pdf(before, to_page=CORE_PAGES - 1)
        result.insert_pdf(appendix)
        result.set_metadata(before.metadata)
        result.set_toc(core_toc + headings)
        rectangles = [result[0].search_for(label) for label in labels]
        assert all(len(matches) == 1 for matches in rectangles)
        for index, target_page in [(1, CORE_PAGES), (2, 31)]:
            links = result[0].get_links()
            existing = [link for link in links if link['from'].intersects(rectangles[index][0])]
            wanted = dict(kind=pdf.LINK_GOTO, **{'from': rectangles[index][0]},
                          page=target_page, to=pdf.Point(0, 0), zoom=0)
            if existing:
                assert len(existing) == 1
                result[0].update_link(dict(wanted, xref=existing[0]['xref']))
            else:
                result[0].insert_link(wanted)
        candidate = Path(folder) / SOURCE.name
        result.save(candidate, garbage=4, deflate=True, no_new_id=True)
        result.close()
        after = pdf.open(candidate)
        assert len(after) == CORE_PAGES + len(appendix)
        assert after.get_toc() == core_toc + headings
        for n in range(CORE_PAGES):
            assert before[n].get_text() == after[n].get_text(), n + 1
            old = before[n].get_pixmap(matrix=pdf.Matrix(.5, .5), alpha=False)
            new = after[n].get_pixmap(matrix=pdf.Matrix(.5, .5), alpha=False)
            assert old.samples == new.samples, n + 1
        appendix_text = normalized(' '.join(page.get_text(clip=pdf.Rect(46, 60, page.rect.width - 46,
                                                                       page.rect.height - 50))
                                           for page in list(after)[CORE_PAGES:]))
        for block in blocks:
            assert normalized(block['text']) in appendix_text, block['text']
        first_links = after[0].get_links()
        assert len(first_links) == 3
        assert sum(l.get('uri') == ACCURACY for l in first_links) == 1
        assert any(l['kind'] == pdf.LINK_GOTO and l['page'] == CORE_PAGES for l in first_links)
        assert any(l['kind'] == pdf.LINK_GOTO and l['page'] == 31 for l in first_links)
        for page in after:
            for link in page.get_links():
                assert link['kind'] in (pdf.LINK_GOTO, pdf.LINK_URI), link
                if link['kind'] == pdf.LINK_GOTO:
                    assert 0 <= link['page'] < len(after)
                else:
                    assert link['uri'] in (ACCURACY, REFERENCE)
        unchanged = len(before) == len(after) and before.get_toc() == after.get_toc()
        unchanged = unchanged and all(l['kind'] == pdf.LINK_GOTO for l in before[0].get_links()
                                      if l['from'].intersects(rectangles[1][0]))
        unchanged = unchanged and any(l.get('page') == CORE_PAGES for l in before[0].get_links())
        if unchanged:
            for n in range(CORE_PAGES, len(after)):
                old = before[n].get_pixmap(matrix=pdf.Matrix(1, 1), alpha=False)
                new = after[n].get_pixmap(matrix=pdf.Matrix(1, 1), alpha=False)
                if before[n].get_text() != after[n].get_text() or old.samples != new.samples:
                    unchanged = False
                    break
        total_pages, appendix_pages = len(after), len(appendix)
        after.close()
        appendix.close()
        before.close()
        if not unchanged:
            SOURCE.write_bytes(candidate.read_bytes())
    report = {'pages': total_pages, 'core_pages': CORE_PAGES, 'unchanged_rendered_pages': CORE_PAGES,
              'appendix_first_page': CORE_PAGES + 1, 'appendix_pages': appendix_pages,
              'definitions_source_sha256': hashlib.sha256(DEFINITIONS.read_bytes()).hexdigest(),
              'definitions_text_blocks_verified': len(blocks), 'bookmarks': len(core_toc + headings),
              'sha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
              'links': [{'label': labels[0], 'destination': ACCURACY, 'offline': False},
                        {'label': labels[1], 'destination': SOURCE.name, 'page': CORE_PAGES + 1,
                         'action': 'GoTo', 'offline': True},
                        {'label': labels[2], 'destination': SOURCE.name, 'page': 32, 'offline': True}]}
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
