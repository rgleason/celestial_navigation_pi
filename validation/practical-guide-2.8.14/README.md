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
