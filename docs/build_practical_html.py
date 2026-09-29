"""Build the offline, reflowable guide from the approved PDF (PyMuPDF needed).

Text remains HTML, not a rasterised page. Illustrated regions are rendered
losslessly with their arrows/annotations intact. Each has a full-resolution
HTML view and an explicit return link, including in wxHtmlWindow. No JavaScript,
web fonts, remote resources or runtime Python dependencies are required.
"""
import hashlib
import html
import json
from pathlib import Path
import re

import pymupdf as pdf

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'data/Practical_Guide.pdf'
ASSETS = ROOT / 'data/practical-guide'


def escape(value):
    return html.escape(value.replace('\u00a0', ' '))


def paragraph(value):
    lines = [line.strip() for line in value.splitlines() if line.strip()]
    # Preserve list steps rather than gluing them into an unreadable paragraph.
    groups = []
    for line in lines:
        if re.match(r'^(?:[•●]|\d+[.)])\s', line) or not groups:
            groups.append(line)
        else:
            groups[-1] += ' ' + line
    return ''.join('<p>' + escape(group) + '</p>\n' for group in groups)


def regions(page):
    seeds = [pdf.Rect(image['bbox']) for image in page.get_image_info()]
    if seeds:
        # Show screenshots individually at a useful reading width, rather
        # than shrinking a whole two-column slide into one tiny thumbnail.
        # Overlaid arrows/labels within a screenshot are retained by rendering
        # its rectangle; outside annotations remain selectable HTML below it.
        merged = []
        for rect in sorted(seeds, key=lambda r: (r.y0, r.x0)):
            if merged and rect.intersects(merged[-1]):
                merged[-1] |= rect
            else:
                merged.append(rect)
        return merged
    for drawing in page.get_drawings():
        rect = drawing['rect']
        # Ignore the page background and text underlines. Include substantive
        # vector diagrams (notably the Direct Triangle), not only screenshots.
        white_background = drawing.get('fill') == (1.0, 1.0, 1.0) and drawing.get('color') in (None, (1.0, 1.0, 1.0))
        if (not white_background and rect.height > 5 and rect.width > 5 and
                rect.get_area() < page.rect.get_area() * .85):
            seeds.append(pdf.Rect(rect))
    seeds.sort(key=lambda rect: rect.y0)
    bands = []
    for rect in seeds:
        if bands and rect.y0 <= bands[-1].y1 + 12:
            bands[-1] |= rect
        else:
            bands.append(rect)
    return bands


def main():
    ASSETS.mkdir(exist_ok=True)
    document = pdf.open(SOURCE)
    head = '''<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Celestial Navigation - How to Guide</title>
<style>
body {font-family:Arial,sans-serif;max-width:980px;margin:auto;padding:20px;color:#202830;line-height:1.5}
h1,h2,h3 {color:#17365d} p,li {font-size:17px} img {max-width:100%;height:auto}
hr {border:0;border-top:1px solid #bbc6d2;margin:28px 0} a {color:#1f5f99}
</style></head><body bgcolor="#ffffff" text="#202830" link="#1f5f99">
'''
    output = [head, '<a name="contents"></a><h1>Celestial Navigation - How to Guide</h1>',
              '<p>Practical instructions and worked examples by Robert Bossert. '
              'This offline HTML edition follows the approved Practical Guide PDF. '
              'Text reflows to your window; screenshots and diagrams are lossless PNGs. '
              'Select an illustration to inspect it at full resolution, then use its '
              'Back to guide link. Original PDF page numbers are retained for reference.</p>',
              '<h2>Contents</h2><ul>']
    for level, title, page_number in document.get_toc():
        output.append(f'<li><a href="practical-guide/page-{page_number:02d}.html">{escape(title)}</a> '
                      f'(PDF page {page_number})</li>')
    output.append('</ul>')
    audit = {'source_sha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
             'pages': [], 'format': 'reflowable HTML with lossless annotated figures'}
    written = set()
    for number, page in enumerate(document, 1):
        navigation = '<p><a href="../Practical_Guide.html">Contents</a>'
        if number > 1:
            navigation += f' | <a href="page-{number-1:02d}.html">Previous page</a>'
        if number < len(document):
            navigation += f' | <a href="page-{number+1:02d}.html">Next page</a>'
        navigation += '</p>'
        page_output = [head, navigation, f'<h2>PDF page {number}</h2>']
        blocks = [block for block in page.get_text('blocks', sort=True) if block[6] == 0]
        # Footers are source pagination/editor revision, not instructions.
        blocks = [b for b in blocks if b[1] < page.rect.height * .952]
        figures = []
        for band in regions(page):
            included = [b for b in blocks if pdf.Rect(b[:4]).intersects(band)]
            clip = pdf.Rect(band)
            if not page.get_image_info():
                for block in included:
                    clip |= pdf.Rect(block[:4])
                clip = (clip + (-6, -6, 6, 6)) & page.rect
                # Do not leave slivers of neighbouring headings in a vector
                # crop: include each intersecting text block in its entirety.
                while True:
                    neighbours = [b for b in blocks if pdf.Rect(b[:4]).intersects(clip)]
                    expanded = pdf.Rect(clip)
                    for block in neighbours:
                        expanded |= pdf.Rect(block[:4])
                    if expanded == clip:
                        included = neighbours
                        break
                    clip = expanded & page.rect
            else:
                clip &= page.rect
            figures.append((clip, included))
        # After including nearby annotations, bands can touch. Merge them to
        # avoid slicing a label or drawing twice across neighbouring crops.
        merged = []
        for clip, included in figures:
            if merged and clip.intersects(merged[-1][0]):
                old_clip, old_blocks = merged.pop()
                merged.append((old_clip | clip, list({b[5]: b for b in old_blocks + included}.values())))
            else:
                merged.append((clip, included))
        covered = {b[5] for _, included in merged for b in included}
        items = [(b[1], 'text', b) for b in blocks if b[5] not in covered]
        items += [(clip.y0, 'figure', (clip, included)) for clip, included in merged]
        page_audit = {'page': number, 'text_blocks': len(blocks), 'figures': []}
        for _, kind, value in sorted(items, key=lambda item: item[0]):
            if kind == 'text':
                page_output.append(paragraph(value[4]))
                continue
            clip, included = value
            index = len(page_audit['figures']) + 1
            name = f'page-{number:02d}-figure-{index}'
            written.update((name + '.png', name + '.html'))
            image = page.get_pixmap(matrix=pdf.Matrix(1.5, 1.5), clip=clip, alpha=False)
            image.save(ASSETS / (name + '.png'))
            display_width = min(620, round(clip.width))
            display_height = round(display_width * image.height / image.width)
            view = head + f'''<h1>Illustration - PDF page {number}</h1>
<p><a href="page-{number:02d}.html">Back to guide - page {number}</a></p>
<p>Full-resolution lossless image. Scroll horizontally if needed.</p>
<img src="{name}.png" width="{image.width}" height="{image.height}" alt="Illustration from guide page {number}">
<p><a href="page-{number:02d}.html">Back to guide - page {number}</a></p></body></html>'''
            # No browser CSS downscaling in the explicitly full-size view.
            view = view.replace('img {max-width:100%;height:auto}', 'img {max-width:none}')
            (ASSETS / (name + '.html')).write_text(view, encoding='utf-8')
            page_output.append(f'<p><a href="{name}.html">'
                          f'<img src="{name}.png" width="{display_width}" '
                          f'height="{display_height}" alt="Illustration from PDF page {number}"></a></p>')
            page_output.append(f'<p><a href="{name}.html">View full-resolution illustration</a></p>')
            # The source figure annotations also stay selectable and readable
            # at normal text size, independent of how the reader scales images.
            for block in sorted(included, key=lambda b: (b[1], b[0])):
                page_output.append(paragraph(block[4]))
            page_audit['figures'].append({'file': name + '.png', 'width': image.width,
                                         'height': image.height})
        audit['pages'].append(page_audit)
        page_output.extend([navigation, '</body></html>'])
        (ASSETS / f'page-{number:02d}.html').write_text('\n'.join(page_output), encoding='utf-8')
        written.add(f'page-{number:02d}.html')
    for old in ASSETS.iterdir():
        if (old.is_file() and old.name not in written and
                re.fullmatch(r'page-\d{2}(?:-figure-\d+)?\.(?:png|html)', old.name)):
            old.unlink()  # Only obsolete outputs owned by this generator.
    output.append('<hr><p><a href="#contents">Back to contents</a></p></body></html>')
    (ROOT / 'data/Practical_Guide.html').write_text('\n'.join(output), encoding='utf-8')
    (ROOT / 'validation/practical-guide-2.8.14/html-conversion.json').write_text(
        json.dumps(audit, indent=2) + '\n')
    print(f'Converted {len(document)} pages; {sum(len(p["figures"]) for p in audit["pages"])} lossless illustrations')


if __name__ == '__main__':
    main()
