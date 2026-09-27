# Celestial Navigation 2.8.13 Android design

Design recorded 27 September 2026, before implementation. Baseline:
`8b7b95078b98e0cc87f5602e1710c4f6435fea60` (upstream 2.8.11.0), plus
`4435de5088666933fb46ee9663a36fbb42baea87` (local 2.8.12 manuals).
The isolated branch is `android/celnav-2.8.13`. The separate upstream 2.9 alpha
is outside this release. Origin main diverges before the recent 2.8 fixes;
starting from it would discard numerical, data and platform corrections.

## Navigator's journey

Prepare in **Plan**, check **Time**, mark the observation instant, then enter
the measured angle in **Observe**. Review body, limb, UTC/watch basis, units,
corrections and uncertainty before Save. A saved sight shows its measurement,
corrected altitude and calculation status. Include the observations to use,
open **Fix**, choose a stationary or moving solution and inspect the residuals,
position and epoch before returning to the chart or creating a waypoint.
All entry and results remain usable offline with analytical ephemerides;
optional high-accuracy data status is explicit.

## Workspace

An Android-only presentation over the existing controller has a persistent
header: **Celestial Navigation**, **Mark time**, **Chart**. Four visible task
pages are **Observe**, **Fix**, **Plan**, **Tools**. Chart hides the workspace
without removing saved geometry. Toolbar reopens it. Back first dismisses the
keyboard, then an owned popup, then the editor with Cancel semantics, then the
workspace. Both Back key events are consumed within the owning window.

Observe contains a short time/status summary, **New sight**, **Horizon event**,
an explicit sort selector and readable observation cards. Each card shows body,
type, measured angle, instant/basis, inclusion and calculation status. A tap
selects it; visible actions offer Edit, Duplicate, Include/Exclude and Delete.
No action requires a right click, hover, shortcut or long press. Delete all
requires a confirmation. Empty selection and an empty library explain what to
do next. The model, rather than hidden list-column text, supplies card values.

Fix offers **Calculate fix**, **Analyze sequence**, and selected sight results.
The focused fix editor retains all existing algorithms, manual/automatic initial
position, saved lunar clock solution, COG/SOG or per-sight DR shifts, epoch,
UTC/local basis, residual/error display and waypoint output. Result provenance
identifies the input observations and motion mode. Chart output survives Close.

Plan offers **Sun & Moon / best sights**, **Find Body** through the observation
editor, **Lunar pairs**, **Voyage almanac** and **Eclipses**. Planning context
chooses manual, boat, chart cursor, selected DR, last fix or waypoint/place where
the baseline supports it. Events, body recommendations, sky plot, GHA/Dec tables,
CSV, noon/Polaris and Create sight remain explicit actions.

Tools offers **Lunar sessions**, **Saved lunar solutions**, **Coastal sextant**,
**Sextant check**, **Time & clock correction**, **Ephemeris / DUT1**,
**Display**, **Manual**, **PDF manual** and **About / storage**. Frequent time
capture remains in the header. GNSS freshness and system time uncertainty must
not imply synchronisation that was not measured. Chrony is a desktop-only
service; Android reports that limitation rather than pretending it ran.

## Editors and reports

Editors are focused sheets with persistent explicit **Save/Apply** and
**Cancel** actions above scrollable content. Measurement, time, corrections,
DR/motion, appearance and calculation details are separate labelled sections.
Android task selectors replace dense desktop tabs where needed. All labels
include units; limb and sign/hemisphere are visible choices. Angle entry offers
degrees and decimal minutes while retaining the shared full-precision value.
Coordinate entry offers N/S/E/W. Free-form legacy formats remain compatible.

Dates use calendar selectors; time is 24-hour hours/minutes with fractional
seconds where the model supports them. Ordinary time uncertainty and lunar
total UTC search span have different labels and meanings. A local date is
converted through the actual zone; it is never relabelled UTC. Ambiguous or
nonexistent local instants require explicit resolution. Reopening and saved
XML must preserve values and precision independently of display formatting.

Every page uses a viewport with a single laid-out content container. Native
font metrics determine wrapping after styling and rotation. Minimum target and
combo row height is 48 dp (72 physical pixels at this tablet's 240 dpi); this
must be measured on the actual device. Keyboard resizing keeps actions visible.
Swipes beginning over controls scroll without committing an accidental click.
Portrait stacks content; landscape may show list and detail side by side.

Long HTML reports/help use Qt QTextBrowser on Android. PDF generation is already
in-process in the baseline, and must stay so. Android file operations use an
app-accessible touch browser or the host chooser on a JNI worker; no blocking
GUI-thread Java wait. Save validates before opening and asks before overwrite.
Long generation/search shows stage, progress and effective cancellation, with
bounded worker admission based on physical headroom. Cancellation leaves
coherent inputs and does not silently lower numerical/data quality.

## Shared implementation contract

Keep Sight, NavigationAlgorithms, lunar/coastal/calibration engines, almanac
generation, eclipse core and compatible XML shared. Android adapters are
conditional. Preserve desktop commands and all 19 existing catalogue targets,
including armhf. The physical acceptance target is OpenCPN 5.14.0 arm64,
Samsung SM-X210, Android 15. API 1.21 host capability and linkage must be verified;
the baseline plugin declares API 1.18. Do not raise desktop requirements merely
to style Android controls.

## Baseline exclusions, not regressions

`CelestialNavigationDialog::OnDRShift` is compiled out (`#if 0`); per-sight DR
Shift and running-fix modes work and must remain. 2.8 has automatic Sights.xml
persistence, not the 2.9 managed backup/import UI, sight names or GUIDs.
Duplicate-name tests apply to host waypoints, routes and calibration profiles.
No new optional pack is bundled automatically. DE440s, lunar orientation and
LOLA retain verification, coverage checks and honest fallback/error messages.

## Delivery order and evidence gates

1. Prove device/storage/UI/log access; back up and verify restore using a probe.
2. Build/install a small Observe slice; create, calculate, plot, save, reopen
   and cold start. Prove committed values separately from displayed text.
3. Complete function mapping in `android-acceptance.md`; implement remaining
   sheets, reports and adapters. Run desktop/numerical checks after shared edits.
4. Execute every mapped family on the physical tablet, including invalid, empty,
   cancellation, rotation, keyboard, scrolling and lifecycle regressions.
5. Build from fresh support archives with provenance repairs, preserve full
   logs, validate metadata/ABI/assets/hashes, and test Plugin Manager import.
6. Recheck local/remote HEAD, rebuild exact committed source, rerun affected
   workflows, retain binaries and evidence, and prepare the full CI candidate.
   Publication stays behind the repository's explicit release approval gate.

Tester handoff extension, 27 September 2026: after physical tablet acceptance,
provide an independently installable Android tarball and download URL, with
root metadata, tested manual import and verified hosted checksum. Keep this
manual tester channel separate from catalogue publication.

## Additional authorized host and tester work

The user requested a downloadable manual-import Android tarball URL after tablet
acceptance, with root metadata.xml and no catalogue publication. On 27 September
2026 they also authorized an isolated minimal Android core import fix, physical
verification on this tablet, a pob220 OpenCPN fork and a focused upstream master
PR if the fix succeeds. Preserve the existing Celestial greyscale/black icon;
no replacement artwork or jigsaw fallback is required.

## Eclipse refinement after physical testing, 27 September 2026

The tablet exposed clipped event dates and coordinates in the inherited eclipse
table. Before further implementation, the Android eclipse sheet is divided into
three visible task pages using the existing section selector:

- **Search & chart:** full-width starting/ending year fields, Find eclipses,
  selectable event cards showing the complete UT1 instant, type, magnitude and
  separate latitude/longitude lines; central-path and contour choices, Plot
  selected, Clear plot. A selected-event summary makes selection explicit.
- **Local circumstances:** the selected-event summary, full-width latitude and
  longitude fields, Use boat position, the optional LOLA choice, Calculate and
  a touch-scrollable results document. Coordinates keep the shared precise
  parser and formatting; no numerical precision is reduced for presentation.
- **Data:** verified/missing/invalid status, DE440 coverage and analytical
  fallback explanation, separate full-width download/import actions, optional
  lunar data and the visible installation-cancellation action.

The close action stays in the sheet header. Search and calculation retain the
existing cancellable worker and unchanged shared engines. Event selection must
remain consistent across pages, repeated searches, and chart output. Native
cards have readable multiline text and a scrollable list; all pages require
physical portrait/landscape, font-size, final-control and Back checks. Desktop
layout and business handlers remain unchanged behind Android guards.

Chart handoff refinement, before implementation at 14:59 BST: **Plot selected
on chart** computes the selected path/contours, then centres the host chart on
that event's greatest position at a regional scale and hides the workspace.
Cancellation or failure retains the previous plot and open inputs. Reopening
the eclipse sheet retains its event/coordinates/results; Clear plot removes
only eclipse geometry. This follows the existing Android sight/fix chart
journey and avoids requiring a navigator to pan from an unrelated chart area.
Desktop Plot selected retains its existing behavior.
