# Bundled practical guide (2.8.x only)

The main window provides three grouped documentation actions:

- **How to Guide (PDF)...** opens `data/Practical_Guide.pdf`, Bob Bossert's
  practical altitude/coastal and lunar worked-example guide.
- **Reference Manual...** opens the existing comprehensive HTML reference.
- **Reference Manual (PDF)...** opens the existing PDF reference. Its HTML
  fallback remains available if no desktop PDF viewer is installed.

All three documents are bundled and work offline. Android exposes the same
choices on its scrollable Tools page and uses its existing in-process PDF
reader. The Android quick guide remains available too. Failure to open the
practical PDF reports an error; it does not silently open a different manual.
No offload/download mechanism is introduced. The 2.9.x branch is not changed.

## Source and retained corrections

The 66-page combined guide is the user's approved Documents copy, assembled
from the altitude/coastal and lunar training guides attached to
[issue 319](https://github.com/rgleason/celestial_navigation_pi/issues/319).
The original worked examples and screenshots are retained without image
recompression. There are 28 bookmarks. The incorporated local corrections are:

- Page 7: the new lunar sight's total search span is **86,400 seconds**.
- Page 9: repair the clipped/overlapping Hawaii heading.
- Page 20: Bob's corrected **18 degrees 51.6 minutes N**, **16 July 2025**
  example (from his 28 September altitude PDF).

Source PDFs:

- [Altitude/coastal guide, 28 September](https://github.com/user-attachments/files/32808960/Celestial.Navigation.Plugin.Altitude.and.coastal.sight.Training.28.sept.2026.PDF.pdf)
- [Lunar guide, 27 September](https://github.com/user-attachments/files/32808978/Lunar.Distance.Use.Case.1.Sun-Moon.Sodus.Bay.version.27.Sept.2026.v2.8x.release.PDF.pdf)

Bundled SHA-256:
`825283aeda51dfd6448abecd6c7e0222552892ed03aa7f1af43c938a864428ab`.
Size: 5,625,940 bytes. This is practical documentation, not an additional
ephemeris data set or a change to the navigation engine.
