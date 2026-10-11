# 2.8.16 self-contained definitions appendix

Base: released upstream master `28608a59c5ae9d308d093f044e177b8cbdcb3f0d`
(2.8.15). Branch: `fix-howto-inline-definitions`.

## Report and correction

Bob's comments `5921447410` and `5921478952` in issue 319 show macOS Preview
refusing permission to open `Celestial_Navigation_Definitions.html`. The
resolved filename is correctly plural. The full definitions were absent from
the original 66-page practical guide.

- Append the complete bundled definitions as seven readable PDF pages, 67-73.
  Keep the source revision, both author credits, wording and all 107 text blocks.
- Change the first-page Definitions action to an internal GoTo on page 67.
  Lunar remains an internal jump to page 32; Accuracy retains its online URL.
  The appendix retains its author's additional online reference. No external
  file-launch or remote-PDF action remains in the guide.
- Add twelve appendix bookmarks while retaining the original 28 entries.
- Include the same definitions in the HTML guide, with topic anchors, links
  from contents/introduction/final page, and return navigation. Use the existing
  numeric-entity/legacy UTF-8 encoding and reader typography.
- Keep the original 66 pages' text and rendered appearance identical. Their
  illustrations and calculations are unchanged. Android keeps its own guide;
  the desktop guide and its appendix remain excluded from Android packages.
- Bump to 2.8.16.0 because 2.8.15 has already been merged and published.

## Validation

- Release build and CPack archive passed; every packaged guide/asset matches
  source, including the new appendix HTML.
- CTest: 11/11 groups passed. Guide/archive checks: 10/10 passed.
- Native HTML reader: 3/3 tests passed. All 66 worked-example pages load; the
  appendix's complete text, topic-anchor scrolling, return links and resizing
  at 600/900/1200 pixels pass without horizontal overflow.
- All seven appendix PDF pages rendered with Poppler and visually reviewed.
  Headings stay with their first entries; definitions do not split across pages.
  Representative final PDF and native-reader captures are included here.
- All 107 source text blocks are verified in extracted appendix PDF text. An
  independent HTML parser check verifies the entire source text is present in
  the HTML appendix. Every contents anchor resolves.
- The writer checks all PDF links, their in-document page bounds and the two
  explicit online URLs. Both local references resolve inside the same PDF.
- `qpdf --check` passed. Regeneration keeps the same hash and does not append
  another copy. PDF SHA-256: `52b153e37673d3c08540daba627b8a0805db811116f2c28c5c0f5df0f9b125bf`.
- The inherited online definitions reference is reachable and its `#a2` anchor
  is present. The local-file failure requires no reader permission change.

No physical Preview acceptance is claimed. The new reference uses the reader's
ordinary internal PDF navigation instead of the external-file action shown in
Bob's error. Follow-up CircleCI qualification and Rick's publication approval
are separate release steps.

## Thread recheck

Issue 319 was rechecked before commit: 78 comments, through Bob's
`5921478952`. No further related report had been added.
