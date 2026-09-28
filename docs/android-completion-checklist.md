# Remaining Android completion work

28 September 2026. This checklist controls remaining work; the complete function
inventory and historical evidence remain in android-acceptance.md and
android-tablet-audit.md. Publication stays on hold. The port is not yet accepted
as finished.

## Carry forward completed work

Do not rerun a passed case merely because another case in its family remains
untested. Read the existing result before scheduling a test. Historical FAIL
entries are retained with their successful repair replay; they are not fresh
blockers. Headless GUI skips are not passes. Source inspection is not tablet
acceptance.

The latest two actual failures are closed on installed source3929527/runtime147:
all16planner preferences persist immediately and after an offline cold restart,
and the corrected-Ho caption fits both orientations at font1.3. Numerical south
noon output remains within the predeclared reference limit. The ignored nautical
edit failure was already repaired and physically verified on146. f201326 is
incorporated and its Android Save/reopen/cold/Cancel and isolated desktop checks
are already recorded. No repeat of these is scheduled.

The Moon phase discrepancy is a shared approximation, reproduced on desktop.
The Android result and guide explicitly qualify it. Do not repeatedly investigate
it as an Android time regression or alter shared desktop numerical behavior.

## Finite remaining cases

| Group | Inventory rows | Specific remaining evidence needed |
|---|---|---|
| Context sources | P01/P02/P08/T01 | Planner cursor/last fix/waypoint choices, unavailable retention and invalid clearing; range endpoints; positive last fix in Find Body and cold no-input GNSS. Boat/selectedDR/selectedtime/nautical→platform fractional UTC and stopped-input/reacquisition passed148; do not repeat them. |
| Body planning | P04 | Remaining sort keys/directions, recommendation limits including reversed range, magnitude/below-horizon sky controls. Carry forward selected-body Create/Save/cold/Delete and worker cancellation. |
| Observation variants | O01/O02/O04/O07/O10/O11 | Remaining body-entry branches, cold exact azimuth precision, angular uncertainty and per-sight true/magnetic shift; independent geometry and complete-record/atomic persistence cases. Carry forward limb/correction/sort/delete/report/calendar cases already recorded. |
| Fix integration | F02/F03/F04 | Matching-watch saved lunar correction actually applied to a fix, per-sight motion and invalid/DST epoch handling. Nonzero chart heading is covered once in the combined chart group below. Carry forward all four algorithms, running-fix and sequence references. |
| Horizon | H01/H02 | Sunset/remaining horizon quality and southern branch examples. Carry forward true/magnetic reference, defaults/cold/Cancel and northern chart cases. |
| Lunar and sextant | L02/L04/L05/S01 | Remaining contact/pair and stored-candidate branches, pair font/reopen and complete clipboard bytes. Carry forward known/joint/bias/outlier/moving session references and Unicode profile/default/cold/Cancel cases. |
| Coastal | C01/C02/C03 | Invalid/uncertainty vertical cases; magnetic/WMM/waypoint; sequential horizontal motion. Carry forward independent stationary examples, chart/reset/Back cases. |
| Almanac | A01/A02/A03 | Remaining named presets/form/paper branches. Do not rerun446/458-page generation, repaired booklet equivalence, cancellation or overwrite tests without a related change. Global annual memory rejection is already tested; do not claim unexecuted full global generation. |
| Eclipse/data | E01/E03/E04/T03 | Larger-font search, boat context, independent terrain comparison; checksum-invalid/download/out-of-coverage and analytical-only provider cases. Carry forward official hashes/import/cold/copy cancellation and NASA standard-duration reference. |
| Documentation | D01/D02 | Final artifact content/version alignment and required HTML/PDF/DOCX visual verification. Carry forward actual offline manual/links/anchors/guide/PDF-viewer tests where the underlying resource/control has not changed. |
| Combined final tablet regression | Cross-cutting; O10/F04/H02/C04/E02/D03 | One final-source sequence through relevant editors with recorded rotate/keyboard/Back/background, night/font, nonzero-heading overlays, plugin disable/re-enable and enabled xGRIB/xWeatherRouting. Retain exact PID, full crash/ANR logs and actual files. Reuse each shared control's previous evidence; repeat only affected or unresolved behavior. |
| Final packages | All targets | One exact committed-source19-platform CI run with fresh dependencies and publication disabled; independently inspect retained artifacts, ABI/API/root metadata/assets/hashes and install/cold-reopen final Android candidate. Earlier19-platform success is retained but is not a final-source pass. |
| Cleanup/handoff | Release | Guarded restoration of only task-owned records/preferences/chart/device changes, preserving original profile/charts/other plugins; retain exact binaries/symbols and short release handoff. Public tester hosting and publication remain pending explicit approval. |

Each group is removed from this list only when its specific evidence is recorded.
Do not convert generic “other variants/crosscuts pending” into an open-ended
request to repeat every successful workflow. New work requires an identified
untested branch, an actual failure, or a changed implementation.

## Build and test rule

Batch related fixes before the next install. A documentation-only audit update
does not require rebuilding/reinstalling the runtime. For code changes, run the
affected checks and the required build checks once; do not broaden/repeat after
they pass without new evidence. Run the final complete platform workflow once
the functional cases above are closed, not for every local iteration.
