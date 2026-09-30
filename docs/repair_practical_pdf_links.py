"""Restore the first-page reference links without changing rendered content.

PyMuPDF is needed only when authoring. Re-running is safe: existing links are
verified rather than duplicated. The lunar section is in this PDF; definitions
open Bob's already-bundled definitions HTML, without internet access.
"""
import hashlib
import json
from pathlib import Path
import tempfile
import pymupdf as pdf

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'data/Practical_Guide.pdf'
REFERENCE = ROOT / 'data/Celestial_Navigation_Definitions.html'
REPORT = ROOT / 'validation/practical-guide-2.8.15/pdf-links.json'


def main():
    before = pdf.open(SOURCE)
    assert REFERENCE.is_file(), 'The intended definitions document must be bundled'
    lunar_pages = [entry[2] - 1 for entry in before.get_toc()
                   if entry[1] == 'Part II - Lunar distances']
    assert lunar_pages == [31]
    labels = ['Accuracy, Goals, Precision and Testing',
              'Celestial Navigation Definitions', 'Lunar Distance Use Case']
    rectangles = [before[0].search_for(label) for label in labels]
    assert all(len(matches) == 1 for matches in rectangles)
    links = before[0].get_links()
    accuracy = [link for link in links if link.get('uri') ==
                'https://github.com/rgleason/celestial_navigation_pi/issues/131']
    assert len(accuracy) == 1
    expected = [
        {'kind': pdf.LINK_LAUNCH, 'from': rectangles[1][0], 'file': REFERENCE.name},
        {'kind': pdf.LINK_GOTO, 'from': rectangles[2][0], 'page': lunar_pages[0],
         'to': pdf.Point(0, 0), 'zoom': 0},
    ]
    with tempfile.TemporaryDirectory() as folder:
        target = Path(folder) / SOURCE.name
        target.write_bytes(SOURCE.read_bytes())
        edited = pdf.open(target)
        for wanted in expected:
            existing = [link for link in links if link['from'].intersects(wanted['from'])]
            if existing:
                assert len(existing) == 1
                # MuPDF normalises local Launch links as remote-go-to links
                # when reading. Check the actual action dictionary as well.
                correct = (edited.xref_get_key(existing[0]['xref'], 'A/S') == ('name', '/Launch')
                           if wanted['kind'] == pdf.LINK_LAUNCH
                           else existing[0]['kind'] == wanted['kind'])
                correct = correct and all(existing[0].get(key) == wanted[key]
                                          for key in ('page', 'file') if key in wanted)
                if not correct:
                    edited[0].update_link(dict(wanted, xref=existing[0]['xref']))
            else:
                edited[0].insert_link(wanted)
        if edited.is_dirty:
            edited.saveIncr()
        edited.close()
        after = pdf.open(target)
        assert len(after) == len(before) == 66
        assert before.get_toc() == after.get_toc()
        # Every page must remain byte-identical when rendered, not only page 1.
        for n in range(len(before)):
            assert before[n].get_text() == after[n].get_text(), n + 1
            old = before[n].get_pixmap(matrix=pdf.Matrix(.5, .5), alpha=False)
            new = after[n].get_pixmap(matrix=pdf.Matrix(.5, .5), alpha=False)
            assert old.samples == new.samples, n + 1
        result = after[0].get_links()
        assert len(result) == 3
        assert any(l['kind'] == pdf.LINK_GOTO and l['page'] == 31 for l in result)
        assert any(l.get('file') == REFERENCE.name and
                   after.xref_get_key(l['xref'], 'A/S') == ('name', '/Launch') for l in result)
        report = {'pages': 66, 'unchanged_rendered_pages': 66,
                  'sha256': hashlib.sha256(target.read_bytes()).hexdigest(),
                  'links': [{'label': labels[0], 'destination': accuracy[0]['uri'], 'offline': False},
                            {'label': labels[1], 'destination': REFERENCE.name,
                             'action': 'Launch', 'offline': True},
                            {'label': labels[2], 'destination': SOURCE.name,
                             'page': 32, 'offline': True}]}
        after.close()
        before.close()
        SOURCE.write_bytes(target.read_bytes())
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
