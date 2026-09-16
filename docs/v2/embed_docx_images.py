#!/usr/bin/env python3
"""Replace LibreOffice's external DOCX image links with embedded media."""

from __future__ import annotations

import sys
import tempfile
import urllib.parse
import zipfile
import re
from pathlib import Path


R_NS = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"
IMAGE_REL = f"{R_NS}/image"
MAX_IMAGE_WIDTH_EMU = 5_850_000


def embed_images(docx: Path) -> int:
    with zipfile.ZipFile(docx, "r") as source:
        files = {name: source.read(name) for name in source.namelist()}

    rel_name = "word/_rels/document.xml.rels"
    document_name = "word/document.xml"
    content_types_name = "[Content_Types].xml"
    relationship_xml = files[rel_name].decode("utf-8")
    embedded: dict[str, tuple[str, bytes]] = {}

    for tag in re.findall(r"<Relationship\b[^>]*/>", relationship_xml):
        if f'Type="{IMAGE_REL}"' not in tag or 'TargetMode="External"' not in tag:
            continue
        id_match = re.search(r'\bId="([^"]+)"', tag)
        target_match = re.search(r'\bTarget="([^"]+)"', tag)
        if not id_match or not target_match:
            raise RuntimeError(f"could not parse image relationship: {tag}")
        relationship_id = id_match.group(1)
        target = urllib.parse.urlparse(target_match.group(1))
        source_path = Path(urllib.parse.unquote(target.path))
        if not source_path.is_file():
            raise FileNotFoundError(f"linked DOCX image is missing: {source_path}")
        media_name = f"image{len(embedded) + 1}{source_path.suffix.lower()}"
        embedded[relationship_id] = (media_name, source_path.read_bytes())
        updated_tag = re.sub(r'\bTarget="[^"]+"', f'Target="media/{media_name}"', tag)
        updated_tag = re.sub(r'\s+TargetMode="External"', "", updated_tag)
        relationship_xml = relationship_xml.replace(tag, updated_tag, 1)

    if not embedded:
        raise RuntimeError("no external DOCX images were found to embed")

    document_xml = files[document_name].decode("utf-8")
    for relationship_id in embedded:
        document_xml = document_xml.replace(
            f'r:link="{relationship_id}"', f'r:embed="{relationship_id}"'
        )

    def fit_inline_image(match: re.Match[str]) -> str:
        block = match.group(0)
        extent = re.search(r'<wp:extent cx="(\d+)" cy="(\d+)"/>', block)
        if not extent:
            return block
        width, height = int(extent.group(1)), int(extent.group(2))
        if width <= MAX_IMAGE_WIDTH_EMU:
            return block
        scale = MAX_IMAGE_WIDTH_EMU / width
        new_width = MAX_IMAGE_WIDTH_EMU
        new_height = round(height * scale)
        block = re.sub(
            r'<wp:extent cx="\d+" cy="\d+"/>',
            f'<wp:extent cx="{new_width}" cy="{new_height}"/>',
            block,
            count=1,
        )
        block = re.sub(
            r'<a:ext cx="\d+" cy="\d+"/>',
            f'<a:ext cx="{new_width}" cy="{new_height}"/>',
            block,
            count=1,
        )
        return block

    document_xml = re.sub(
        r'<wp:inline\b.*?</wp:inline>', fit_inline_image, document_xml, flags=re.DOTALL
    )

    # The HTML cover relies on CSS pagination which Writer/Web does not import.
    # Add the page break directly to the first real heading in the DOCX.
    about_text = "<w:t>About this manual</w:t>"
    about_pos = document_xml.find(about_text)
    if about_pos == -1:
        raise RuntimeError("could not locate the About heading for the cover page break")
    paragraph_pos = document_xml.rfind("<w:p>", 0, about_pos)
    properties_pos = document_xml.find("<w:pPr>", paragraph_pos, about_pos)
    if properties_pos == -1:
        raise RuntimeError("the About heading has no paragraph properties")
    insert_pos = properties_pos + len("<w:pPr>")
    document_xml = (
        document_xml[:insert_pos]
        + "<w:pageBreakBefore/>"
        + document_xml[insert_pos:]
    )
    files[document_name] = document_xml.encode("utf-8")
    files[rel_name] = relationship_xml.encode("utf-8")

    content_types = files[content_types_name].decode("utf-8")
    if 'Extension="png"' not in content_types:
        content_types = content_types.replace(
            "</Types>",
            '<Default Extension="png" ContentType="image/png"/></Types>',
        )
    files[content_types_name] = content_types.encode("utf-8")

    for _, (media_name, payload) in embedded.items():
        files[f"word/media/{media_name}"] = payload

    with tempfile.NamedTemporaryFile(dir=docx.parent, suffix=".docx", delete=False) as temp:
        replacement = Path(temp.name)
    try:
        with zipfile.ZipFile(replacement, "w", zipfile.ZIP_DEFLATED) as output:
            for name, payload in files.items():
                output.writestr(name, payload)
        replacement.replace(docx)
    finally:
        replacement.unlink(missing_ok=True)
    return len(embedded)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: embed_docx_images.py MANUAL.docx")
    count = embed_images(Path(sys.argv[1]).resolve())
    print(f"Embedded {count} images in {sys.argv[1]}")
