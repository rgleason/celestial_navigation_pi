"""Build the offline guide, preserving complete annotated illustrations.

PyMuPDF is an authoring dependency only. Generated pages use basic HTML
supported by wxHtmlWindow and browsers, without scripts or remote assets.
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
TITLES = [
    'Introduction', 'Altitude and coastal navigation topics',
    'Working with the plugin', 'Plugin overview', 'The Hawaii worked example',
    'Create an altitude sight', 'Enter the sight time', 'Set sight parameters',
    'Find a celestial body', 'Interpret the calculated altitude', 'Save the sight',
    'Duplicate and edit sights', 'Prepare a running fix', 'Advance the position circles',
    'Calculate the fix', 'Read the plotted fix', 'Mark and copy UTC time',
    'Analyze a sequence of sights', 'Plan celestial sights', 'Plan sunrise and meridian transit',
    'Enter the planning position and course', 'Read the sunrise plan',
    'Position from a horizon event', 'Record a sunrise observation', 'Compare the horizon-event position',
    'Horizontal coastal sights', 'Select the coastal landmarks', 'Enter horizontal angles',
    'Plot the coastal fix', 'Vertical coastal sights', 'Plot a vertical coastal sight',
    'Lunar distance topics', 'Lunar distancing: objective and background',
    'Lunar distancing and chronometers', 'Accuracy and relevance of lunars',
    'Observe lunars aboard ship', 'Practice lunars on land', 'The Sun-Moon worked example',
    'Create a lunar sight', 'Enter the three measured angles', 'Near and far limbs',
    'Enter the observer position', 'UTC search span', 'Separately timed observations',
    'Read the lunar results', 'Interpret UTC and position candidates', 'Explore time sensitivity',
    'Plot the lunar altitude circles', 'Compare the corrected position', 'Choose a lunar pair',
    'Lunar pair geometry', 'Plan a Jupiter-Moon lunar', 'Use the lunar planner',
    'The Jupiter-Moon worked example', 'Preset the sextant', 'Backyard lunars',
    'Improve lunar observations', 'Reduce measurement errors', 'Analyze repeated observations',
    'Direct Triangle: example observations', 'Direct Triangle: reference calculation',
    'Direct Triangle: apparent distance and bearing', 'Direct Triangle: clear the distance',
    'Compare the Direct Triangle working', 'UTC and position solver working', 'Try it yourself',
]
HEAD = '''<!DOCTYPE html>
<html lang="en"><head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Celestial Navigation - How to Guide</title>
<style>
body {font-family:Arial,sans-serif;font-size:16px;line-height:1.55;margin:24px auto;
      padding:0 20px;max-width:900px;color:#263442;background:#ffffff}
h1 {font-size:28px;line-height:1.25;color:#17365d;margin:20px 0 16px}
h2 {font-size:21px;color:#17365d;margin:24px 0 12px}
p {margin:12px 0} li {margin:8px 0} a {color:#245b87}
table {border-collapse:collapse} img {max-width:100%;height:auto}
.figure {margin:20px auto} .navigation {font-size:14px} .caption {font-size:14px}
</style></head><body bgcolor="#ffffff" text="#263442" link="#245b87">
'''


def escape(value):
    # Remove discretionary PDF breaks, preserving minus signs and angles.
    return html.escape(value.replace('\u00a0', ' ').replace('\u00ad', '')
                       .replace('\u2028', '\n'))


def write_html(path, value):
    # Older Windows wx builds recognise HTTP-EQUIV rather than HTML5's short
    # charset declaration. Numeric entities also avoid locale-based decoding.
    path.write_bytes(value.encode('ascii', 'xmlcharrefreplace'))


def paragraph(value):
    lines = [s.strip() for s in value.replace('\u2028', '\n').splitlines() if s.strip()]
    groups = []
    for line in lines:
        if line == 'Additional documents' or re.match(r'^(?:[\u2022\u25cf]|\d{1,2}[.)])(?:\s|$)', line) or not groups:
            groups.append(line)
        else:
            groups[-1] += ' ' + line
    output, bullets = [], []
    for group in groups:
        if group == 'Additional documents':
            if bullets:
                output.append('<ul>' + ''.join(bullets) + '</ul>')
                bullets = []
            output.append('<h2>Additional documents</h2>')
        elif re.match(r'^[\u2022\u25cf](?:\s|$)', group):
            bullets.append('<li>' + escape(group[1:].strip()) + '</li>')
        else:
            if bullets:
                output.append('<ul>' + ''.join(bullets) + '</ul>')
                bullets = []
            output.append('<p>' + escape(group) + '</p>')
    if bullets:
        output.append('<ul>' + ''.join(bullets) + '</ul>')
    return '\n'.join(output)


def text_blocks(page):
    blocks = []
    for block in page.get_text('dict', sort=True)['blocks']:
        if block['type'] != 0:
            continue
        spans = [s for line in block['lines'] for s in line['spans']]
        value = '\n'.join(''.join(s['text'] for s in line['spans']) for line in block['lines'])
        # Remove pagination/revision, without losing instructions near the foot.
        if block['bbox'][1] > 1000 and (re.fullmatch(r'\s*\d+\s*', value) or 'RMB' in value):
            continue
        blocks.append({'rect': pdf.Rect(block['bbox']), 'text': value,
                       'size': max(s['size'] for s in spans),
                       'bold': all('Bold' in s['font'] for s in spans if s['text'].strip())})
    return blocks


def illustration(page, blocks, number):
    if number == 4:
        # A connected hub-and-wheel diagram, not independent screenshots.
        return page.rect, blocks
    seeds = [pdf.Rect(i['bbox']) & page.rect for i in page.get_image_info()]
    for drawing in page.get_drawings():
        rect = drawing['rect'] & page.rect
        white = drawing.get('fill') == (1.0, 1.0, 1.0) and drawing.get('color') in (None, (1.0, 1.0, 1.0))
        if (not white and rect.width > 5 and rect.height > 5 and
                rect.get_area() < page.rect.get_area() * .85):
            seeds.append(rect)
    if not seeds:
        return None, []
    clip = pdf.Rect(seeds[0])
    for rect in seeds[1:]:
        clip |= rect
    # Preserve each authored composition, including arrows and whole labels.
    clip = (clip + (-10, -10, 10, 10)) & page.rect
    while True:
        included = [b for b in blocks if b['rect'].intersects(clip)]
        expanded = pdf.Rect(clip)
        for block in included:
            expanded |= block['rect']
        expanded &= page.rect
        if expanded == clip:
            return clip, included
        clip = expanded


def block_html(block):
    value = block['text'].strip()
    if value == 'Additional documents':
        return '<h2>Additional documents</h2>'
    if block['bold'] and block['size'] >= 30 and block['rect'].y0 < 150:
        return '<p><font size="2" color="#627484">' + escape(' '.join(value.splitlines())) + '</font></p>'
    if block['bold'] and block['size'] >= 30 and len(value) < 220:
        return '<h2>' + escape(' '.join(value.splitlines())) + '</h2>'
    return paragraph(value)


def page_block_html(block, number):
    if number == 1 and block['rect'].y0 < 150:
        return ''  # The reader's title already supplies the introduction heading.
    markup = block_html(block)
    if number == 1:
        for label, target in [
            ('Accuracy, Goals, Precision and Testing', 'https://github.com/rgleason/celestial_navigation_pi/issues/131'),
            ('Celestial Navigation Definitions', '../Celestial_Navigation_Definitions.html'),
            ('Lunar Distance Use Case', 'page-32.html'),
        ]:
            markup = markup.replace(label, f'<a href="{target}">{label}</a>')
    return markup


def navigation(number, count):
    previous = f'<a href="page-{number-1:02d}.html">Previous</a>' if number > 1 else 'Previous'
    following = f'<a href="page-{number+1:02d}.html">Next</a>' if number < count else 'Next'
    return f'''<table class="navigation" width="100%" bgcolor="#edf2f6" cellpadding="10" cellspacing="0">
<tr><td width="25%">{previous}</td><td align="center"><a href="../Practical_Guide.html">Contents</a></td>
<td width="25%" align="right">{following}</td></tr></table>'''


def main():
    ASSETS.mkdir(exist_ok=True)
    document = pdf.open(SOURCE)
    assert len(document) == len(TITLES), 'Review headings when the source changes'
    output = [HEAD, '<h1>Celestial Navigation</h1><h2>How to Guide</h2>',
              '<p>Practical instructions and worked examples by <b>Robert Bossert</b>.</p>',
              '<p>Choose a topic below, or start with the <a href="practical-guide/page-01.html">introduction</a>. '
              'Use Previous and Next to work through the guide. Select an illustration to see it at full size.</p>',
              '<a name="contents"></a><h2>Contents</h2>']
    section = None
    for level, title, page_number in document.get_toc():
        if level == 1:
            if section is not None:
                output.append('</ul>')
            section = title
            output.append(f'<h2><a href="practical-guide/page-{page_number:02d}.html">{escape(title)}</a></h2><ul>')
        else:
            output.append(f'<li><a href="practical-guide/page-{page_number:02d}.html">{escape(title)}</a></li>')
    output.append('</ul>')
    output.append('<h2>Reference documents</h2><ul><li><a href="Celestial_Navigation_Definitions.html">'
                  'Abbreviations, definitions and measurements</a></li><li><a href="Celestial_Navigation_Information.html">'
                  'Reference manual</a></li></ul>')
    audit = {'source_sha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
             'pages': [], 'format': 'reflowable HTML with complete annotated illustrations'}
    written = set()
    for number, page in enumerate(document, 1):
        nav = navigation(number, len(document))
        part = 'Altitude sights and coastal navigation' if number < 32 else 'Lunar distances'
        page_output = [HEAD, nav, f'<p><font color="#627484">{part} &middot; Page {number} of {len(document)}</font></p>',
                       '<h1>' + escape(TITLES[number - 1]) + '</h1>']
        blocks = text_blocks(page)
        clip, included = illustration(page, blocks, number)
        covered = {id(b) for b in included}
        items = [(b['rect'].y0, 'text', b) for b in blocks if id(b) not in covered]
        if clip is not None:
            items.append((clip.y0, 'figure', None))
        page_audit = {'page': number, 'title': TITLES[number - 1], 'text_blocks': len(blocks), 'figures': []}
        for _, kind, value in sorted(items, key=lambda item: item[0]):
            if kind == 'text':
                page_output.append(page_block_html(value, number))
                continue
            name = f'page-{number:02d}-figure-1'
            written.update((name + '.png', name + '-preview.png', name + '.html'))
            image = page.get_pixmap(matrix=pdf.Matrix(1.5, 1.5), clip=clip, alpha=False)
            image.save(ASSETS / (name + '.png'))
            # Pre-render a reading-size preview instead of relying on the
            # platform to downscale large bitmaps correctly on high-DPI screens.
            scale = min(1.0, 760 / clip.width)
            preview = page.get_pixmap(matrix=pdf.Matrix(scale, scale), clip=clip, alpha=False)
            preview.save(ASSETS / (name + '-preview.png'))
            back = f'<p><a href="page-{number:02d}.html">Back to guide - page {number}</a></p>'
            view = [HEAD.replace('img {max-width:100%;height:auto}', 'img {max-width:none}'), back,
                    '<h1>' + escape(TITLES[number - 1]) + '</h1>',
                    '<p>Full-size illustration. Scroll to inspect the details.</p>',
                    f'<img src="{name}.png" width="{image.width}" height="{image.height}" '
                    f'alt="{escape(TITLES[number - 1])}: complete illustration">', back]
            if included:
                view.append('<h2>Text in this illustration</h2>')
                view.extend(block_html(b) for b in included)
            view.append('</body></html>')
            write_html(ASSETS / (name + '.html'), '\n'.join(view))
            # A single form should not fill the whole reader. Wide diagrams
            # retain the available width; both layouts shrink with the window.
            figure_width = '70%' if clip.width < 900 else '100%'
            page_output.append(f'''<table class="figure" align="center" width="{figure_width}" cellpadding="8" cellspacing="0" bgcolor="#f3f6f8">
<tr><td><a href="{name}.html"><img src="{name}-preview.png" width="100%"
alt="{escape(TITLES[number - 1])}: complete illustration"></a></td></tr>
<tr><td class="caption"><a href="{name}.html">Open illustration at full size</a></td></tr></table>''')
            # Body prose stays selectable. Diagram/table labels have a complete
            # selectable transcription in the full-size illustration view.
            if number != 4:
                page_output.extend(page_block_html(b, number) for b in included if b['size'] >= 28)
            page_audit['figures'].append({'file': name + '.png', 'width': image.width,
                                         'height': image.height, 'preview': name + '-preview.png', 'clip': list(clip)})
        audit['pages'].append(page_audit)
        page_output.extend(['<hr>', nav, '</body></html>'])
        write_html(ASSETS / f'page-{number:02d}.html', '\n'.join(page_output))
        written.add(f'page-{number:02d}.html')
    for old in ASSETS.iterdir():
        if old.is_file() and old.name not in written and re.fullmatch(r'page-\d{2}(?:-figure-\d+(?:-preview)?)?\.(?:png|html)', old.name):
            old.unlink()
    output.append('</body></html>')
    write_html(ROOT / 'data/Practical_Guide.html', '\n'.join(output))
    report = ROOT / 'validation/practical-guide-2.8.15/html-conversion.json'
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(audit, indent=2) + '\n')
    print(f'Converted {len(document)} pages; {sum(len(p["figures"]) for p in audit["pages"])} complete illustrations')


if __name__ == '__main__':
    main()
