# 2.8.14 documentation integration checks

Base: Rick's merged master `9150c6002d83f495713c2461034b674ae8eda37e`
(2.8.13, including Android and native Windows x64). Only the 2.8.x release is
changed; no files in the separate 2.9.x worktree are changed by this task.

- Local Linux release build passed.
- CTest: **10/10 groups passed** with local optional DE440s/PCK/LOLA test data.
  Main suite: **212 passed**, seven opt-in GUI tests skipped in the headless run.
- `FindBodyUi.IndependentActionsThroughAllThreeSightRoutes` passed separately
  with `CELESTIAL_RUN_UI_TESTS=1` on the laptop display. It verifies the new
  main-menu buttons at 1024x700 and 1200x800, bounds, minimum label dimensions,
  no overlapping guide buttons, and the existing Find workflows.
- `main-window.png` is the actual GTK rendering from that isolated test,
  visually checked for readable documentation captions and spacing.
- New Python release/guide tests passed: exact archives accepted; missing,
  corrupted and duplicate practical PDFs rejected; version read from CMake
  rather than a hard-coded previous release.
- Android arm64 release compiled against the existing pinned OpenCPN 5.14
  support and local host library. UTC/angle/download adapter boundary checks
  passed. Physical-device PDF acceptance for this new guide is separate from
  build qualification.
- `qpdf --check data/Practical_Guide.pdf` passed. Bundled PDF is byte-identical
  to the approved Documents copy, including page 7, 9 and 20 corrections.
- Existing reference HTML and PDF are unchanged from the merged base.

The normal CircleCI workflow retains all **20 build targets**, including both
Android ABIs and native Windows x64. Every retained tarball is now checked for
the exact bundled guide assets. Windows x64 requires the matching Preview host
SDK; no actual Windows GUI acceptance is claimed. Publication remains disabled
by default and is not part of this documentation PR.

## Illustrated HTML follow-up (same open 2.8.14 PR)

- Added a reflowable, selectable-text How to Guide with 66 linked pages,
  contents and previous/next navigation, and 103 lossless PNG illustrations.
  Full-resolution views have explicit return links. Only the current page's
  images are loaded, not the entire guide. The approved PDF hash is unchanged.
- Four desktop documentation actions use the existing ten-row layout. A full
  blank action row separates Generate Almanac from the documentation region.
  Button-bound/size/non-overlap and gap assertions passed at 1024x700 and
  1200x800. Updated GTK screenshot: `main-window.png`.
- Actual plugin HTML-reader test passed across all 66 pages. Relative links,
  full-size illustration navigation and return-to-contents passed. Representative
  reader captures: `howto-page-09.png`, `howto-page-43.png`,
  `howto-page-60.png`, `howto-page-62.png`. Screenshot captions are selectable
  text; illustrations are not JPEG-recompressed.
- The first combined GUI run exposed missing wxApp initialization in the new
  test harness, not a plugin fault. The harness was corrected and the final
  reader test passed independently.
- Six Python archive/HTML tests pass, covering exact guide assets, missing or
  corrupt/duplicate files, missing PNGs, all local links, no remote resources
  or scripts, and Android-specific guide selection.
- Android arm64 build/package passed. Android retains its own quick guide and
  reference manuals. Both desktop How To actions remain hidden and the desktop
  practical PDF, HTML and PNG assets are excluded from Android packages.
- No navigation-engine or 2.9.x files were changed.

## Android final-import packaging correction

Pipeline 532 at `c0eadda` passed all 18 desktop builds. Both Android libraries
also compiled and CPack created their archives, but the subsequent
`ci/package-android-import.py` verifier still demanded `Practical_Guide.pdf`.
That obsolete second policy contradicted the deliberate Android exclusion.

The final-import packager now calls the shared platform-specific guide checker
using the already-validated metadata target. It requires Android's quick guide
and both reference manuals, and rejects desktop practical guide files. Root
`metadata.xml`, URL, plugin identity/version and exact ELF runtime-section
checks are retained.

New end-to-end packager regressions exercise both ARM64/ELF64 and ARMHF/ELF32
fixtures, root metadata, a missing Android guide, corrupted reference PDF and
all three prohibited desktop asset forms. Failure preserves an existing output
archive and removes the incomplete staging file. The real local Android ARM64
CPack archive passed the final manual-import packaging step too. No installed
desktop plugin files or calculations are affected by this CI-only correction.
