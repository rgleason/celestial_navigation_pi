# Celestial Navigation manual v2

This directory holds the shared source and generated editions of the plugin
manual.

- `Celestial_Navigation_Manual_v2.html` is the reviewable, version-controlled
  source used by the plugin's Documentation button.
- `diagrams/` contains deterministic SVG masters.
- `images/` contains PNG derivatives for wxHTML, DOCX and PDF.
- `output/Celestial_Navigation_Information.html` is the offline plugin edition.
- `output/Celestial_Navigation_Manual_v2.docx` is the self-contained,
  contributor-editable edition. All diagrams are embedded.
- `output/Celestial_Navigation_Manual_v2.pdf` is the fixed-layout A4 display
  and printable edition generated from the DOCX.

Run `./build_manual.sh` to regenerate diagrams, synchronise the plugin HTML,
embed images in the DOCX, produce the PDF and run structural/portability
checks. LibreOffice, Python 3 and Matplotlib are required.

The DOCX is deliberately provided so contributors can review and suggest
changes using ordinary word-processing tools. Accepted content changes should
also be applied to the HTML source before a release, so the offline, editable
and printable editions do not diverge.

## Diagram provenance and review

The figures are original technical drawings generated from explicit geometry;
they do not copy online artwork and are not AI-generated raster illustrations.
The concepts and notation were checked against:

- the current NGA edition of *The American Practical Navigator (Bowditch)*,
  especially its navigational-astronomy, altitude-correction and sight-reduction
  figures;
- US Naval Observatory celestial-navigation and altitude/azimuth references;
- *The Nautical Almanac* conventions.

The figure masters should be regenerated and visually inspected after any
geometry, label or font-size change. `validate_manual.py` checks structure,
links, figure numbering, glossary ordering and DOCX portability, but it cannot
replace visual or navigational review.
