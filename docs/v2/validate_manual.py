#!/usr/bin/env python3
"""Structural and portability checks for all v2 manual formats."""

from __future__ import annotations

import html
import re
import sys
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "Celestial_Navigation_Manual_v2.html"
OUTPUT = ROOT / "output"
PLUGIN_DATA = ROOT.parent.parent / "data"


def plain(fragment: str) -> str:
    return html.unescape(re.sub(r"<[^>]+>", "", fragment)).strip()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def main() -> None:
    source = SOURCE.read_text(encoding="utf-8")
    ids = set(re.findall(r'\bid="([^"]+)"', source))
    anchors = re.findall(r'href="#([^"]+)"', source)
    missing_anchors = sorted(set(anchors) - ids)
    images = re.findall(r'<img\b[^>]*\bsrc="([^"]+)"', source)
    missing_images = [name for name in images if not (ROOT / name).is_file()]
    captions = [int(n) for n in re.findall(r'<figcaption>Figure (\d+)\.', source)]

    require(not missing_anchors, f"missing internal anchors: {missing_anchors}")
    require(not missing_images, f"missing source images: {missing_images}")
    require(len(images) == 10, f"expected 10 diagrams, found {len(images)}")
    require(captions == list(range(1, 11)), f"figure numbering is not 1–10: {captions}")
    require("file://" not in source, "source HTML contains a machine-local file URL")

    abbr_section = re.search(
        r'<h1 id="abbreviations".*?</section>', source, flags=re.DOTALL
    )
    glossary_section = re.search(r'<h1 id="glossary".*?</section>', source, flags=re.DOTALL)
    require(abbr_section is not None, "abbreviations appendix is missing")
    require(glossary_section is not None, "glossary appendix is missing")
    abbreviations = [
        plain(value)
        for value in re.findall(r"<tr><td>(.*?)</td><td>", abbr_section.group(0), re.DOTALL)
    ]
    glossary = [plain(value) for value in re.findall(r"<dt>(.*?)</dt>", glossary_section.group(0), re.DOTALL)]
    require(len(abbreviations) >= 50, f"abbreviation appendix unexpectedly short: {len(abbreviations)}")
    require(len(glossary) >= 60, f"glossary unexpectedly short: {len(glossary)}")
    require(glossary == sorted(glossary, key=str.casefold), "glossary is not alphabetical")

    output_html = OUTPUT / "Celestial_Navigation_Information.html"
    plugin_html = PLUGIN_DATA / "Celestial_Navigation_Information.html"
    require(output_html.read_bytes() == SOURCE.read_bytes(), "output HTML is stale")
    require(plugin_html.read_bytes() == SOURCE.read_bytes(), "plugin Documentation HTML is stale")

    docx = OUTPUT / "Celestial_Navigation_Manual_v2.docx"
    with zipfile.ZipFile(docx) as package:
        media = [name for name in package.namelist() if name.startswith("word/media/")]
        relationships = package.read("word/_rels/document.xml.rels").decode("utf-8")
    require(len(media) == 10, f"DOCX should embed 10 diagrams, found {len(media)}")
    require(
        not re.search(r'relationships/image"[^>]*TargetMode="External"', relationships),
        "DOCX still has externally linked images",
    )

    pdf = OUTPUT / "Celestial_Navigation_Manual_v2.pdf"
    require(pdf.is_file() and pdf.stat().st_size > 100_000, "printable PDF is missing or empty")
    print(
        f"Validated 10 figures, {len(abbreviations)} abbreviations, "
        f"{len(glossary)} glossary terms, embedded DOCX images and offline HTML."
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"manual validation failed: {error}", file=sys.stderr)
        raise SystemExit(1)
