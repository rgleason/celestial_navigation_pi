# 2.8.15 How to Guide presentation and link correction

Base: released upstream master `1b53a713d0bd0773b980b83452b203c21a173dae`
(2.8.14). Branch: `fix/howto-html-presentation`.

## Changes

- Reworked all 66 HTML pages with descriptive headings, grouped contents,
  compact example captions, bullet lists and Previous/Contents/Next navigation.
- Preserve 53 complete authored illustrations, including window boundaries,
  arrows, labels and adjoining diagrams. The hub-and-wheel overview is one
  complete composition. Preview PNGs are distinct from the full-size images;
  single forms use less width than large diagrams. Full-size views provide
  explicit return links and selectable illustration text.
- Use a 12-point embedded-reader font. HTTP-EQUIV declares UTF-8 for older
  wxWidgets readers; all generated HTML uses ASCII with numeric entities.
  The official wxWidgets 3.2.8 parser source only recognises HTTP-EQUIV, unlike
  the 3.2.11 Linux runtime, which also recognises HTML5's short charset form.
- Restore the two missing PDF annotations: Bob's bundled definitions HTML
  (relative document-launch action), and lunar section in the guide (page 32).
  Keep the existing online accuracy/testing link to issue 131. Equivalent HTML
  links are active; online links launch the system browser.
- Navigation calculations, source observation values, Android guide selection,
  the comprehensive reference documents and the 2.9 branch are unchanged.

## Validation

- Local Linux release build and CTest: **11/11 groups passed**. Optional GUI
  cases remain skipped in the ordinary headless run.
- Embedded-reader GUI checks: **3/3 passed**, covering every source page,
  no horizontal overflow, overview resizing at 600/900/1200 pixels, Unicode
  plus/minus, full-size navigation and return, local definitions and lunar links.
- All 66 pages captured from the actual GTK/wx reader and visually reviewed.
  Representative captures are included here: contents, introduction/links,
  plugin overview, lunar search span, and Direct Triangle.
- Nine guide/archive checks passed, including missing/corrupt/duplicate assets,
  missing illustrations/definitions, offline relative destinations, the single explicit
  online reference, legacy-compatible encoding, complete overview/preview
  bounds, PDF audit hash, and platform-specific Android guide selection.
- PDF repair reopens and validates all three destinations. All 66 pages have
  identical extracted text and rendered pixels before/after annotation repair;
  all 28 bookmarks are retained. Re-running the repair produces the same hash.
- `qpdf --check` passed. The repaired PDF SHA-256 is
  `a8d24f988829f92db587ace741f21d94d372634d0dc7fe32440aa444561db6a3`.
- Full illustrated assets occupy approximately 26 MB (previously 18 MB),
  reflecting preserved complete compositions and separate previews. The final 2.8.15.0 desktop CPack archive
  passed verification of every guide asset and local definitions target. These assets remain excluded from
  Android packages.

No physical Windows or macOS GUI acceptance is claimed. The encoding fix is
compatible with the older parser, but Bob's machines remain useful final
acceptance targets. CI publication uses the existing maintainer approval gate. The user has
authorized the 2.8.15 builds and publication after the final thread recheck.

The verified 2.8.15.0 library and complete bundled data were installed in the
working Linux OpenCPN. The previous library/data were backed up, and the private
plugin data inventory was unchanged.

## Issue 319 recheck before version bump

Re-read all 73 comments on 30 September 2026, through comment `5920217930`.
Bob's further related reports were:

- `5919779738`: missing first-page PDF references; lunar URL unnecessary because
  the guide is combined.
- `5919911696`: the same HTML fragmentation and character problems on iMac.
- `5920040146`: Acrobat panels closed successfully; accuracy link opened after
  reader prompts; asks whether the PDF is local.

These are covered by the corrected presentation/encoding, active reference
links and the local-bundling explanation. No further related blocker was found. A second recheck before push still
contained the same 73 comments.
The PDF is installed in the plugin's local `data` directory; only the historical
accuracy reference requires an online page.

## Final pre-publication thread clarification

The last recheck contained 75 comments, through `5920350068`. Bob supplied
the local `Celestial_Navigation_Definitions.html` path from his older Mac
installation. That document is already bundled and is the HTML destination.
The PDF destination now opens that exact document, rather than the newer
reference manual's glossary. A PDF reader may prompt for document-launch
actions or restrict them according to its settings. No further related
HTML correction was requested.

The initial 2.8.15 revision passed all 20 CircleCI builds and retained-package
audits. Its publication gate must remain held: the corrected Definitions link
requires fresh builds from the final PR revision before publication.
