"""Read the bundled definitions once for both the HTML and PDF appendix."""
import html
import re
from html.parser import HTMLParser
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'data/Celestial_Navigation_Definitions.html'
CORE_PAGES = 66
APPENDIX_TITLE = 'Appendix - Definitions and measurements'


def clean(text):
    return text.replace('\u00a0', ' ').replace('\u2011', '-').replace('\u2013', '-').replace('\u2014', ' - ')


def title_parts(text):
    match = re.fullmatch(r'(.*?) (\(Rev .*\))', text)
    return match.groups() if match else (text, '')


class DefinitionsParser(HTMLParser):
    """Handle the source's omitted paragraph and list-item end tags."""
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.in_body = False
        self.kind = None
        self.parts = []
        self.plain = []
        self.blocks = []
        self.inline = []
        self.headings = 0

    def flush(self):
        text = ' '.join(''.join(self.plain).split())
        if text:
            markup = ''.join(self.parts) + ''.join(f'</{tag}>' for tag in reversed(self.inline))
            block = dict(kind=self.kind or 'p', text=text, markup=markup)
            if block['kind'].startswith('h'):
                block['anchor'] = f'definitions-section-{self.headings}'
                self.headings += 1
            self.blocks.append(block)
        self.kind, self.parts, self.plain, self.inline = None, [], [], []

    def handle_starttag(self, tag, attrs):
        if tag == 'body':
            self.in_body = True
            return
        if not self.in_body:
            return
        if tag in ('h1', 'h2', 'h3', 'li', 'p'):
            if tag == 'p' and self.kind == 'li':
                self.parts.append(' ')
                self.plain.append(' ')
                return
            self.flush()
            self.kind = tag
        elif tag in ('ul', 'hr'):
            self.flush()
        elif tag in ('b', 'i', 'u', 'a'):
            if tag == 'a':
                href = dict(attrs).get('href', '')
                assert href == 'https://www.siranah.de/html/sail040e.htm#a2', href
                self.parts.append('<a href="' + html.escape(href, quote=True) + '">')
            else:
                self.parts.append('<' + tag + '>')
            self.inline.append(tag)
        elif tag == 'br':
            self.parts.append('<br/>')
            self.plain.append(' ')

    def handle_endtag(self, tag):
        if not self.in_body:
            return
        if tag in ('h1', 'h2', 'h3', 'li', 'ul', 'body') or (tag == 'p' and self.kind != 'li'):
            self.flush()
        elif tag in self.inline:
            # Close any inline descendants, including malformed source markup.
            index = len(self.inline) - 1 - self.inline[::-1].index(tag)
            self.parts.extend(f'</{name}>' for name in reversed(self.inline[index:]))
            del self.inline[index:]
        if tag == 'body':
            self.in_body = False

    def handle_data(self, value):
        if self.in_body:
            value = clean(value)
            self.parts.append(html.escape(value))
            self.plain.append(value)


def read_definitions():
    parser = DefinitionsParser()
    parser.feed(SOURCE.read_text(encoding='utf-8'))
    parser.flush()
    assert parser.blocks and parser.blocks[0]['kind'] == 'h1'
    return parser.blocks


def definitions_html(blocks):
    output, in_list = [], False
    for block in blocks:
        if in_list and block['kind'] != 'li':
            output.append('</ul>')
            in_list = False
        if block['kind'] == 'li' and not in_list:
            output.append('<ul>')
            in_list = True
        if 'anchor' in block:
            output.append(f'<a name="{block["anchor"]}"></a>')
        if block['kind'] == 'h1':
            title, revision = title_parts(block['text'])
            output.append('<h1>' + html.escape(title) + '</h1>')
            if revision:
                output.append('<p><font size="2" color="#627484">' + html.escape(revision) + '</font></p>')
        else:
            output.append(f'<{block["kind"]}>' + block['markup'] + f'</{block["kind"]}>')
    if in_list:
        output.append('</ul>')
    return '\n'.join(output)
