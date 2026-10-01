# Bundled How to Guide (2.8.x)

The desktop main window groups four documentation actions:

- **How to Guide...** opens Bob Bossert's altitude/coastal and lunar worked
  examples in the plugin's offline HTML reader.
- **How to Guide (PDF)...** opens the same guide in the system PDF viewer.
- **Reference Manual...** opens the comprehensive HTML reference.
- **Reference Manual (PDF)...** opens the PDF reference, with an HTML fallback.

The practical guide is excluded from Android, whose controls differ. Android
retains its own quick guide and reference manuals. A failed practical-PDF launch
reports an error instead of opening a different manual.

## Source and retained corrections

The approved combined guide was assembled from Bob's training PDFs attached to
[issue 319](https://github.com/rgleason/celestial_navigation_pi/issues/319).
The original 66 pages and 28 bookmarks are retained, with the original
screenshots and worked examples. The Definitions appendix brings the guide
to 73 pages and adds section bookmarks.
The incorporated corrections remain:

- Page 7: the new lunar sight's total UTC search span is **86,400 seconds**.
- Page 9: the clipped/overlapping Hawaii heading is repaired.
- Page 20: **18 degrees 51.6 minutes N**, **16 July 2025**.

Source PDFs:

- [Altitude/coastal, 28 September](https://github.com/user-attachments/files/32808960/Celestial.Navigation.Plugin.Altitude.and.coastal.sight.Training.28.sept.2026.PDF.pdf)
- [Lunar, 27 September](https://github.com/user-attachments/files/32808978/Lunar.Distance.Use.Case.1.Sun-Moon.Sodus.Bay.version.27.Sept.2026.v2.8x.release.PDF.pdf)

## 2.8.15 presentation corrections

The HTML reader has a grouped contents page, descriptive headings, compact
source-example captions, consistent Previous/Contents/Next navigation, actual
bullet lists, and a readable 12-point base font. Basic HTML tables and font
attributes support the embedded reader; CSS additionally supports browsers.

`docs/build_practical_html.py` preserves 53 complete illustrated compositions
rather than splitting screenshots away from their annotations, arrows, other
windows, or diagrams. Separate reading-size PNG previews fit the window. Single
forms use less width than large diagrams. Selecting a figure opens a full-size
lossless PNG with a return link and selectable transcription. Instructional
prose remains selectable in the main guide. Source screenshots may include
intentionally overlapping windows; the complete authored composition is retained.

The pages declare UTF-8 with HTTP-EQUIV, recognised by older Windows wxWidgets,
and encode non-ASCII text as numeric HTML entities. This avoids mis-decoding
angular symbols, primes, smart quotes and other characters through a Windows
locale. Python/PyMuPDF is required only for authoring, not installation or CI.

## First-page references

The original combined PDF and Bob's latest source PDF had only one active
hyperlink. The definitions and lunar references were styled text without link
annotations. `docs/repair_practical_pdf_links.py` adds their destinations:

- **Accuracy, Goals, Precision and Testing** retains the existing online link to
  issue 131, which contains the April 2025 historical testing material.
- **Celestial Navigation Definitions** jumps to the complete definitions
  appendix on page 67 of the same PDF.
- **Lunar Distance Use Case** jumps to page 32 of the combined guide.

Both definitions and lunar references are internal PDF page jumps. They need no
external-file permissions. The HTML reader includes the same complete definitions
appendix with contents links and return navigation. The historical accuracy link
and the definitions author's additional reference open online pages; no remote
images, scripts or fonts are loaded by the guide.

The 2.8.16 appendix resolves macOS Preview's refusal to open the separate HTML
file reported in issue 319. The filename was correctly plural; this was an
external-file permission failure. The full definitions were not in the original
66-page guide.

`docs/guide_definitions.py` reads the complete bundled source for both formats.
`docs/repair_practical_pdf_links.py` uses ReportLab and PyMuPDF to append it,
retaining the source revision, author credits and terminology. All 107 source
text blocks are verified in the appended PDF; the original 66 pages retain
identical text and rendered appearance. The PDF has only internal page links
and the two explicit online references, with no external-file actions.

Regeneration does not duplicate the appendix or change an already-current PDF.
Package integrity checks include the appendix HTML, every worked-example page,
full-size image and preview. Validation evidence is in
`validation/practical-guide-2.8.16/`.
