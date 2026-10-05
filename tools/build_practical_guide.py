#!/usr/bin/env python3
"""Merge Bob's issue-370 PDFs, refresh the FIX illustrations, and build offline HTML.

Requires PyMuPDF. Regeneration inputs and hashes are recorded in the
validation report. The release build consumes the generated, bundled assets.
"""
import argparse
import hashlib
import json
from pathlib import Path

import pymupdf as fitz
from build_practical_html import main as build_html


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def build(args):
    root = Path(__file__).resolve().parents[1]
    data = root / 'data'
    assets = data / 'practical-guide'
    assets.mkdir(exist_ok=True)
    audit = root / 'validation/fix-guide-2.9.7'
    audit.mkdir(exist_ok=True)
    doc = fitz.open(args.altitude)
    lunar = fitz.open(args.lunar)
    assert len(doc) == len(lunar) == 35
    # The same FIX image occurs in the overview and the worked-example page.
    fix_xref = next(i['xref'] for i in doc[16].get_image_info(xrefs=True)
                    if i['width'] == 743 and i['height'] == 584)
    fix_pages = [i + 1 for i, page in enumerate(doc)
                 if any(x['xref'] == fix_xref for x in page.get_image_info(xrefs=True))]
    assert fix_pages == [5, 17]
    doc[16].replace_image(fix_xref, filename=args.fix_screenshot)
    # Refresh the main-menu inset on the FIX page as well.
    menu_xref = next(i['xref'] for i in doc[16].get_image_info(xrefs=True)
                     if i['width'] == 368 and i['height'] == 367)
    doc[16].replace_image(menu_xref, filename=args.menu_screenshot)
    page = doc[16]
    # Remove the old result-field outline, whose position no longer matches.
    page.add_redact_annot(fitz.Rect(609, 291, 998, 329), fill=None)
    page.apply_redactions(images=0, graphics=2, text=1)
    page.add_redact_annot(fitz.Rect(1280, 140, 1900, 980), fill=(1, 1, 1))
    page.apply_redactions(images=0, graphics=0)
    explanation = (
        'FIX latitude and longitude show the result from the included sights '
        '(eyes on in the sight log).\n\n'
        'DR source defaults to the latest included sight with a valid saved DR. '
        'Choose another sight, enter DR coordinates, or explicitly choose the boat position. '
        'Missing DR blocks calculation and plotting.\n\n'
        'Calculate refreshes the result. Show fix on chart centres the chart on it.\n\n'
        'Vessel motion defaults to Each sight\'s DR Shift when an included sight has a shift; '
        'otherwise it starts as Stationary. Choose One COG and SOG for that motion model.\n\n'
        'Fix time is the common UTC reference time for a running fix. It defaults to '
        'the latest included sight. More options allows a different time and clock correction. '
        'The stationary algorithm selector is disabled for running fixes.')
    assert page.insert_textbox(fitz.Rect(1280, 145, 1880, 980), explanation,
                               fontsize=27, fontname='helv', lineheight=1.14) >= 0
    page.insert_text((600, 810), 'FIX form updated for 2.9.7 - 5 October 2026', fontsize=24)
    doc.insert_pdf(lunar)
    # Retain the definitions appendix already approved for the production guide.
    definitions = fitz.open(args.production_guide)
    assert len(definitions) == 73
    doc.insert_pdf(definitions, from_page=66, to_page=72)
    # PyMuPDF uses the key 'from', which cannot be expressed as a keyword.
    # Repair the links explicitly to avoid dependence on PDF reader file access.
    for label, target in [('Celestial Navigation Definitions', 70), ('Lunar Distance', 35)]:
        for rect in doc[0].search_for(label):
            doc[0].insert_link({'kind': fitz.LINK_GOTO, 'from': rect, 'page': target})
    chapters = [(1, 'Introduction'), (3, 'Creating and maintaining sights'),
                (6, 'Three-star example southwest of Hawaii'), (14, 'Duplicate and edit'),
                (15, 'Fix and running fix'), (19, 'Mark UTC'), (20, 'Sight analysis'),
                (21, 'Sun, Moon and sight planning'), (22, 'Planning an ETA'),
                (27, 'Position without sextant or GPS'), (30, 'Horizontal coastal sights'),
                (34, 'Vertical coastal sights'), (36, 'Lunar distance'),
                (42, 'Sun-Moon worked example'), (51, 'Sensitivity analysis'),
                (54, 'Choosing a lunar pair'), (56, 'Planning a lunar'),
                (59, 'Pre-shoot the sextant'), (60, 'Backyard lunars'),
                (61, 'Improving lunar accuracy'), (64, 'Theory and direct-triangle methods'),
                (71, 'Definitions and measurements')]
    doc.set_toc([[1, label, number] for number, label in chapters])
    doc.set_metadata({'title': 'Celestial Navigation 2.9.x How to Guide',
                      'author': 'Bob Bossert; FIX illustrations updated for 2.9.7',
                      'subject': 'Desktop practical guide, altitude/coastal and lunar distance'})
    pdf = data / 'Practical_Guide.pdf'
    doc.save(pdf, garbage=4, deflate=True)
    doc.close()
    doc = fitz.open(pdf)
    assert len(doc) == 77
    page_records = []
    for i, page in enumerate(doc):
        n = i + 1
        full = assets / f'page-{n:02d}.png'
        pix = page.get_pixmap(matrix=fitz.Matrix(1, 1), alpha=False)
        pix.save(full)
        text = page.get_text(sort=True)
        page_records.append(dict(page=n, text_sha256=hashlib.sha256(text.encode()).hexdigest(),
                                 image_sha256=digest(full)))
    report = dict(version='2.9.7.0', pages=77, core_pages=70, definitions_pages=7,
                  updated_fix_pages=fix_pages, pdf_sha256=digest(pdf),
                  sources={str(p):digest(p) for p in [args.altitude,args.lunar,args.production_guide]},
                  fix_screenshot_sha256=digest(args.fix_screenshot),
                  menu_screenshot_sha256=digest(args.menu_screenshot),
                  links=[dict(page=1,label='Definitions',target=71),dict(page=1,label='Lunar distance',target=36)],
                  page_records=page_records)
    (audit / 'guide-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    build_html()
    print('Created 77-page desktop guide with updated FIX images on pages 5 and 17.')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ['altitude','lunar','production-guide','fix-screenshot','menu-screenshot']:
        p.add_argument('--'+name,required=True)
    build(p.parse_args())
