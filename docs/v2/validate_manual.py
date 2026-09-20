#!/usr/bin/env python3
"""Structural and portability checks for all v2 manual formats."""

from __future__ import annotations

import html
import re
import sys
import subprocess
import zipfile
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image


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
    cmake = (ROOT.parent.parent / "CMakeLists.txt").read_text(encoding="utf-8")
    version = ".".join(
        re.search(r'set\(VERSION_' + part + r'\s+"(\d+)"\)', cmake).group(1)
        for part in ("MAJOR", "MINOR", "PATCH")
    )
    edition = "Celestial Navigation plugin " + version
    require(edition in source, "manual cover does not match plugin version")
    require("experimental" not in source.lower(),
            "manual still labels the documentation experimental")
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
    require(
        output_html.read_bytes() == plugin_html.read_bytes(),
        "plugin Documentation HTML is stale",
    )

    prepared = output_html.read_text(encoding="utf-8")
    require(re.sub(r' height="\d+"', "", prepared) == source,
            "generated HTML differs from the current manual source")
    prepared_images = re.findall(
        r'<img\b[^>]*\bsrc="([^"]+)"[^>]*\bwidth="(\d+)"[^>]*\bheight="(\d+)"',
        prepared,
    )
    require(
        len(prepared_images) == 10,
        f"expected 10 explicitly sized HTML diagrams, found {len(prepared_images)}",
    )
    for name, width_text, height_text in prepared_images:
        expected = (int(width_text), int(height_text))
        plugin_image = PLUGIN_DATA / name
        require(plugin_image.is_file(), f"missing plugin image: {name}")
        with Image.open(plugin_image) as image:
            require(
                image.size == expected,
                f"plugin image {name} is {image.size}, expected {expected}",
            )

    docx = OUTPUT / "Celestial_Navigation_Manual_v2.docx"
    with zipfile.ZipFile(docx) as package:
        media = [name for name in package.namelist() if name.startswith("word/media/")]
        relationships = package.read("word/_rels/document.xml.rels").decode("utf-8")
        document = ET.fromstring(package.read("word/document.xml"))
        text = "".join(document.itertext())
        require(edition in text, "DOCX cover does not match plugin version")
        require("experimental" not in text.lower(),
                "DOCX still labels the documentation experimental")
        word = "{http://schemas.openxmlformats.org/wordprocessingml/2006/main}"
        section = document.find(".//" + word + "sectPr")
        page = section.find(word + "pgSz")
        margins = section.find(word + "pgMar")
        available = (int(page.get(word + "w")) -
                     int(margins.get(word + "left")) -
                     int(margins.get(word + "right")))
        for table in document.iter(word + "tbl"):
            width = table.find(word + "tblPr/" + word + "tblW")
            require(width is not None and width.get(word + "type") == "dxa"
                    and int(width.get(word + "w")) <= available,
                    "DOCX table extends beyond the printable page")
    require(len(media) == 10, f"DOCX should embed 10 diagrams, found {len(media)}")
    require(
        not re.search(r'relationships/image"[^>]*TargetMode="External"', relationships),
        "DOCX still has externally linked images",
    )

    pdf = OUTPUT / "Celestial_Navigation_Manual_v2.pdf"
    require(pdf.is_file() and pdf.stat().st_size > 100_000, "printable PDF is missing or empty")
    bundled_pdf = PLUGIN_DATA / "Celestial_Navigation_Manual_v2.pdf"
    require(
        bundled_pdf.is_file() and bundled_pdf.read_bytes() == pdf.read_bytes(),
        "bundled PDF is missing or stale",
    )
    pdf_text = subprocess.check_output(["pdftotext", "-bbox", str(pdf), "-"])
    pdf_tree = ET.fromstring(pdf_text)
    xhtml = "{http://www.w3.org/1999/xhtml}"
    for page_number, page in enumerate(pdf_tree.iter(xhtml + "page"), 1):
        for word in page.iter(xhtml + "word"):
            require(float(word.get("xMin")) >= 0.0 and
                    float(word.get("yMin")) >= 0.0 and
                    float(word.get("xMax")) <= float(page.get("width")) and
                    float(word.get("yMax")) <= float(page.get("height")),
                    f"PDF text is clipped on page {page_number}: {word.text}")
    print(
        f"Validated 10 figures, {len(abbreviations)} abbreviations, "
        f"{len(glossary)} glossary terms, embedded DOCX images, scaled offline "
        "HTML and bundled PDF."
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"manual validation failed: {error}", file=sys.stderr)
        raise SystemExit(1)
