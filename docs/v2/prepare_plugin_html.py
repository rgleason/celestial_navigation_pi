#!/usr/bin/env python3
"""Prepare the HTML manual and its images for wxHtmlWindow.

wxHtmlWindow applies an HTML image width without reliably scaling the image
height. The printable editions therefore retain the high-resolution diagram
masters, while the plugin receives Lanczos-resampled images at their exact
display dimensions and matching explicit width/height attributes.
"""

from __future__ import annotations

import re
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parent
SOURCE_HTML = ROOT / "Celestial_Navigation_Manual_v2.html"
OUTPUT = ROOT / "output"
PLUGIN_DATA = ROOT.parent.parent / "data"


def prepare_image(match: re.Match[str]) -> str:
    prefix, relative_name, width_text, suffix = match.groups()
    source = ROOT / relative_name
    width = int(width_text)

    with Image.open(source) as original:
        height = round(original.height * width / original.width)
        resized = original.resize((width, height), Image.Resampling.LANCZOS)
        destination = PLUGIN_DATA / relative_name
        destination.parent.mkdir(parents=True, exist_ok=True)
        resized.save(destination, format="PNG", optimize=True)

    # Explicit dimensions also make the generated HTML robust if it is moved
    # away from the pre-scaled plugin images.
    return f'{prefix}{relative_name}" width="{width}" height="{height}"{suffix}'


def main() -> None:
    source = SOURCE_HTML.read_text(encoding="utf-8")
    pattern = re.compile(
        r'(<img\s+src=")'
        r'(images/[^"]+\.png)"\s+width="(\d+)"'
        r'([^>]*>)'
    )
    prepared, count = pattern.subn(prepare_image, source)
    if count != 10:
        raise RuntimeError(f"expected to prepare 10 diagrams, found {count}")

    OUTPUT.mkdir(parents=True, exist_ok=True)
    plugin_html = OUTPUT / "Celestial_Navigation_Information.html"
    plugin_html.write_text(prepared, encoding="utf-8")
    (PLUGIN_DATA / plugin_html.name).write_text(prepared, encoding="utf-8")


if __name__ == "__main__":
    main()
