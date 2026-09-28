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
position and epoch before showing the fix on the chart.
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
UTC/local basis, residual/error display and chart centring. Result provenance
identifies the input observations and motion mode. Chart output survives Close.

Plan offers **Sun & Moon / best sights**, **Find Body** through the observation
editor, **Lunar pairs**, **Voyage almanac** and **Eclipses**. Planning context
chooses manual, boat, chart cursor, selected DR, last fix or waypoint/place where
the baseline supports it. Events, body recommendations, sky plot, GHA/Dec tables,
CSV, noon/Polaris and Create sight remain explicit actions.

The planner opens on a separate **Context** page with a single scrollable
column for position, time, display basis and motion. Explicit calendar and
fractional-second selectors are the Android default; nautical text entry
remains available. **Events** presents full names, UTC and chosen display
times, true bearings and observer positions in wrapped results, followed by
Moon information. These fields must never depend on horizontally clipped
desktop columns. Changing context recalculates the existing shared models.

Tools offers **Lunar sessions**, **Saved lunar solutions**, **Coastal sextant**,
**Sextant check**, **Time & clock correction**, **Ephemeris / DUT1**,
**Display**, **Manual**, **PDF manual** and **About / storage**. Frequent time
capture remains in the header. GNSS freshness and system time uncertainty must
not imply synchronisation that was not measured. Chrony is a desktop-only
service; Android reports that limitation rather than pretending it ran.

## Editors and reports

### Planner results refinement, 27 September 2026, before implementation

**Bodies and best sights** uses an explicit Sort selector and ascending or
descending choice, a selected-body summary and Create selected sight action.
Full-width selectable cards show body, Hc, true azimuth, GHA, declination,
magnitude, score and the complete recommendation reason. Card selection occurs
on a stationary release; swipes scroll without changing selection. Hc limits,
the recommended pairs/triads report, magnitude filter, below-horizon toggle and
sky plot remain available on **Recommendations & sky**. Context refresh preserves a
selected body by its catalogue name where that body remains visible.

**Almanac** has an explicit Export CSV action above scrollable cards containing
the complete UTC instant, body and every existing table column. The actual CSV
continues to come from the shared AlmanacRow model. **Noon and Polaris** stacks
workflow, precise corrected Ho, Solve latitude and the full wrapped result.
Invalid context clears all outputs and gives guidance on every result page;
creating a sight or exporting a table cannot use an invalid or stale context.

Refinement before the next implementation at 22:20 BST: physical testing exposed
competing nested scroll areas. **Bodies** and **Almanac** therefore use a single
native list occupying the remaining page height, with their sort/selection or
export actions fixed above it. They have no surrounding scrolling form.
**Recommendations & sky** is a separate form with one scrolling content panel.
All body fields, filtering, recommendations and chart actions remain available.
Hidden desktop display tables are not populated on Android; shared result
models remain unchanged. Refresh duration is measured again before deciding
whether numerical work also needs a cancellable background worker.

Refresh refinement before implementation at 22:30 BST: one owned planner worker
snapshots the validated context, computes the existing full horizon/phase/body/
almanac models, and publishes only the newest completed context. No widget or
form is accessed by that worker. A fixed progress row identifies the current
stage and offers Cancel; context edits invalidate old outputs immediately and
cancel obsolete work before a coalesced replacement starts. Cancellation is
checked at ephemeris evaluation boundaries. Closing joins the cancelled worker
before plugin unload. The worker stays alive while the planner is open so its
verified thread-local optional-data cache is retained. Numerical algorithms,
sampling, precision, optional-data verification and desktop behavior stay intact.

Landscape refinement before implementation at 22:50 BST: the full-height list
scrolls, but stacked sort/direction and selection/Create rows leave too little
height for a complete body card. Place sort and direction side by side, then
selection and the short **Create sight** action side by side, retaining 48dp
minimum control heights. The persistent header Close replaces the redundant
bottom Close. Invalid body results use a concise selection status; complete
validation guidance remains on Context and the relevant result pages. Retest
actual final-card fields after rotation and font scaling.

Android CSV exports retain nonzero milliseconds in the ISO UTC column while
keeping the existing columns and numeric serialization. Whole-second rows
retain their legacy representation. This is an Android presentation/output
adapter over the shared AlmanacRow formatter; desktop CSV behavior stays the
same. The stored UTC instant, rather than a local calendar, supplies this field.

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
FixDialog::OnGo centres the chart; the 2.8 baseline has no fix-waypoint creation command. Waypoint selection in planning/coastal tools remains supported. No new optional pack is bundled automatically. DE440s, lunar orientation and
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
# Eclipse card gesture refinement, 27 September 2026

Physical testing of the complete 1850–2100 result list showed that Qt selected
the card underneath the initial press before recognising a swipe. Selection
must occur only when a stationary tap is released. A drag must move the native
list without changing the event used by Local circumstances or Plot. Use the
existing Android touch filter with an explicit native scroll viewport; consume
the press and synthesised mouse sequence, and select the card only in its tap
callback. Programmatic selection remains available to the shared model. Apply
the same rule to the native file list so a folder cannot open during a swipe.
Verify actual list movement, unchanged selection after repeated swipes, ordinary
tap selection, and access to the final card. Desktop controls remain unchanged.
# Body popup refinement, 27 September 2026

A physical catalogue swipe activated Acamar on release instead of scrolling.
Extend the Android combo adapter to own the popup viewport gesture as well as
the collapsed control. A drag scrolls the popup; only a stationary release
sets the model index, closes the popup and emits the existing wxQt activation
signal once. Preserve native/programmatic selection and 48 dp delegate rows.
Verify catalogue movement, tap selection and Back cancellation on the tablet.

# Popup Back lifecycle refinement, 27 September 2026

Physical Back on a combo popup left a persistent black surface with the host
PID still alive. The sheet must dismiss an owned combo through QComboBox's
hidePopup lifecycle, which resets the combo's internal popup state, rather
than hiding its private container directly. Record the combo owner on its
popup, stop active scrolling, consume both Back halves, and keep the editor
visible with uncommitted values unchanged. Other native popups close through
their normal close lifecycle. Retest popup Back, editor Cancel, Save/reopen,
and rotation with the popup open.

Rotation with a native choice popup open leaves Qt 5.12's cached portrait
popup height beyond the landscape screen. On screen geometry change, close
that popup through its owner and retain the editor's selection/unsaved fields.
The navigator can reopen a correctly placed popup in the new orientation.
Do not commit a highlighted row during rotation.

# Observation uncertainty wording, before implementation

Android's angular validation error incorrectly says arcseconds although the
shared model and generated desktop control use arcminutes. Label the Android
field Angular uncertainty (arcminutes) and correct its validation wording,
including Moon/body altitude uncertainties. The Android lunar time label also
says half-span while the engine searches plus/minus half of the entered total.
Use Total UTC search span (seconds), consistent with the preserved 2.8.12
manual; 86,400 seconds means 12 hours on either side. These are Android
presentation corrections only; keep numerical values, engines and compatible
saved records unchanged.

The observation section picker uses Measurement, Time (UTC), Motion, Display,
Corrections and Calculations. Replace generated Config/Parameters labels only
on Android so navigators can find the corrections without knowing desktop
implementation names. Keep the same controls and handlers on each page.

### Tablet review refinement: multiple labelled fields (16:15)

The lunar timing and running-fix screenshots show horizontal desktop rows
clipping the course/speed labels, and the uncertainty units following their
entry. Android form rows with meaningful labels will stack vertically, while
colon-separated hour/minute/second rows stay together. Present observation
timing units above the uncertainty field. Keep exact controls/model values and
all desktop layout unchanged. Verify final motion controls by actual swipes,
not scrollbar movement alone.

### Lunar solution interpretation refinement (16:27)

After calculation, show each UTC candidate as a visible selectable card with
full UTC, additional correction, cleared distance, rate and time uncertainty.
Show position branches underneath with latitude/longitude, distance from DR
and horizontal RMS uncertainty. Selection remains explicit; Save lunar
solution stays reachable below the page. Retain the shared candidate lists as
the controller model but hide their clipped desktop columns on Android. Use
a focused labelled name sheet explaining that saving snapshots a derived
solution and changes neither recorded times nor global correction. Resize/
show must settle wrapped labels without requiring a preliminary swipe.

### Sextant check refinement, 28 September 00:14, before implementation

The physical page still has desktop multi-field rows: pressure/temperature/IE
labels and values are clipped, and its seven-column repeat list has only one
line of height. Keep this page inside the existing single form viewport, stack
the labelled prediction, measurement and profile controls vertically on
Android, and use model-backed selectable repeat cards with every value/unit.
Never derive these cards from wxQt column getters. Remove operates on the
explicit selected model index. Reflow dynamic prediction/profile text and
repeat cards after calculations, add/remove and rotation. Desktop controls,
engines and persisted profiles remain unchanged. Verify actual prediction,
index/residual arithmetic, add/remove, profile persistence, complete final
swipes and both orientations on the next committed binary.

### Sextant uncertainty refinement, 28 September 00:31, before implementation

Two identical ±0.20′ readings produced a saved correction-point uncertainty
of zero: the baseline uses only repeat scatter after using the entered
uncertainties as weights. On Android preserve the correction, binning and
repeatability, but bound the formal uncertainty of each weighted mean by
sqrt(1/sum(1/sigma²)). Equal independent ±0.20′ readings then give at least
0.141421′, even with zero repeat scatter. Keep desktop numerical behaviour
unchanged, explicitly test the Android-compiled engine branch with equal and
unequal uncertainties and nonzero scatter, and rebuild/retest the same
disposable profile on the tablet. Existing saved profiles are not rewritten.

### Lunar session and pair layout, 28 September 00:45, before implementation

The actual Lunar sequence page still uses two desktop columns: the sight
label is clipped horizontally and selection actions disappear below the
viewport. Pair planner retains an eleven-column wxQt table. Stack Android
settings/selection/result headers vertically in the existing single form
viewport. Represent lunar selection and result residuals with complete model
cards; keep explicit check state in the existing selection model and defer
card rebuild until the native callback returns. Pair outputs are complete
labelled cards made directly from immutable computed row values, including
all eleven fields, never wxQt table column getters. Hide desktop tables only
on Android. Preserve all shared numerical calculations and worker ownership.
Physically verify final swipes, selection/clear, earliest DR, invalid inputs,
worker Cancel/Back, candidate results and both orientations before acceptance.

### Cold-loaded lunar session preparation (28 September)

Physical runtime84's four public Greenwich observations reach Solve but return
`std::exception` with no candidate. Android Recompute intentionally defers
single-sight searches, so a cold-loaded snapshot has no LunarEphemeris callback.
A copied callback may also retain the original Sight rather than its snapshot.

Prepare an Android snapshot's callback explicitly without running the individual
search: set its corrected epoch and reuse RecomputeLunar's existing ephemeris
construction with an Android-only prepare flag, returning immediately after
assigning the callback. The owned session worker then invokes callbacks bound
to retained snapshots. Keep saved single-sight candidates as optional seeds;
do not require previous individual calculation. Desktop call signatures and
behavior remain unchanged. Validate cold saved inputs, known-position and joint
modes, cancellation, residuals and independent saved solution bytes on tablet.

### Session result identity and presentation (28 September)

Independent readback found U+0013 in saved labels: Android wxString::ToStdString
narrowed the U+2013 dash before the report was reconstructed as UTF8. Use explicit
UTF8 bytes for Android session labels, preserving real Unicode and valid XML.
In a known-position solve no position covariance is fitted: show 'Position held
fixed' instead of infinite NM. Tie Android mode/position/search/robust/bias/motion
changes to result invalidation so only the current solved snapshot can be saved.
Keep callbacks weak and defer native spin notifications outside Qt dispatch.

### Joint-session convergence at ephemeris resolution (28 September)

The independent Greenwich four-reading session also fails in the desktop
production model. An instrumented retained copy reaches clock−3.33s and
51.47868,0.01203 with stable weighted cost2.1809345, but an undamped correction
norm around1e−11 never satisfies the mixed-unit1e−12 threshold. The model
represents epochs at millisecond resolution and uses finite differences.
For Android use symmetric differences and assess the actual undamped changes
in their units: clock below0.01s, horizontal position below0.0005NM and fitted
index bias below0.0001′. These are far below the input uncertainty floor0.05′.
Never accept a small damped/rejected step or mere iteration exhaustion.
Retain desktop derivatives/convergence exactly. Test the Android engine branch
with production public USNO inputs plus convergence/exhaustion/outlier/motion
regressions, then physically repeat the same fixture and inspect saved bytes.

### DUT1 download ownership (28 September, before implementation)

Source review finds the optional DUT1 button still calls the synchronous host
download API on the GUI thread, contrary to the Android surface design.
Use the existing host background-download events on Android, with one owned
transfer, visible Cancel, a30s timeout, and cleanup on panel destruction.
Stack the three download/import/cancel actions at full width so translated
captions and larger fonts remain reachable in both orientations.
Keep the current validated atomic install and shared desktop download intact.
Cancellation/failure must retain the current offline data and remove only the
owned temporary file. Validate real online download/coverage, explicit Cancel,
Back/Close, offline failure, local invalid/older imports and cold provenance.

Cold-reloaded saved session reports preserve all inputs and real Unicode, but
wxQt replaces the explicit read-only Close caption with native OK, causing the
surface to present Save. Set cnActionText=Close on this viewer's original button.
For future Android known-position records replace the unestimated infinite
position sigma with 'position held fixed' in the saved report as well as the
live summary. Keep existing saved trails unchanged.
The input trail still hardcodes solverVersion2.8.5.1; record the configured
four-component release version for new snapshots on all platforms. This is
version provenance only and must not alter desktop numerical behavior.

### Observation calculation log (28 September, before implementation)

Physical94 exposes the saved calculation log but swipes initially select text
and show a selection handle; its font remains visibly smaller than the19pt
editor at system scale1.3. The native Qt read-only state cannot be used as the
sole test for wxTE_READONLY: the supported wxQt creation path does not apply
that style to QTextEdit. Honour the wx style explicitly, disable selection and
keyboard focus, and use the existing native document scrolling. Apply the
scaled font through the QTextEdit stylesheet as well as its document font so
subsequent wx SetValue/SetFont cannot restore the small default. Wrap long
read-only report lines to the viewport. Keep editable multiline controls and
all desktop behavior unchanged. Repeat actual portrait/landscape log swipes to
the final Ho, page switching, definitions Back, Save/reopen and file checks.

Physical94 Display also has no visible colour picker and only a tiny desktop
slider handle. Replace the Android colour-picker presentation with a full-width
button and an owned colour sheet: common colours, exact RGB fields and a swatch,
with Apply/Cancel/Back retaining the existing RGB/alpha model. Keep the hidden
wx picker as the shared controller value. Enlarge the existing transparency
slider to48dp, show its alpha value and respect its inverse direction when
handling native touch input. All appearance changes remain within the sight
transaction until its Save. Test nested colour Cancel/Back, unchanged Apply,
typed RGB, slider endpoints, XML/reopen, larger font and rotation.

Physical97 successfully saves/reopens RGB13/79/201 and alpha114, preserves all
original sights, and reaches both alpha endpoints with the correct inverse
direction. Its swatch panel is not painted and the log retains a later local
16pt override. Make the swatch a styled label with a readable hex value, and
show that value on the colour button. Set the known report explicitly read-only
before decoration, remove its late font override and give Calculations one
full-height viewport (the existing native QTextEdit), with definitions and
lunar calculation actions outside its scrolling content. Retest on tablet.


Physical98 cold-start azimuth failure (28 September 04:51 BST): exact retained
symbols trace the crash to wxGenericProgressDialog parent discovery, called
from BuildBearingLineOfPosition while OpenXML constructs the main dialog.
The same saved sight was explicitly true in the editor but the XML has no
m_bMagneticNorth field; reconstruction therefore silently selects magnetic.
Persist the Android measurement bearing basis in optional
AndroidMagneticAzimuth (absent retains the historical magnetic default; only
false requires an attribute). Keep DRMagneticAzimuth separate. Do not alter
desktop serialization or numerical behavior. On Android omit the unowned,
parentless generic progress dialog from bearing polygon reconstruction; its
nested event loop is inappropriate during initial dialog construction. Test
saved true and magnetic variants, exact XML, cold reopen, responsiveness and
chart geometry before acceptance.

Physical98 report now fills its page and final Ho is reachable in both
orientations with scaled readable text. A native selection handle reappears
after Definitions Back/page switching. Install the existing owned drag filter
on each read-only QTextEdit viewport, forwarding directly to its own QScroller
and consuming synthesized mouse selection events. Editable text remains
unchanged. Repeat drag/tap/type/page-change/Definitions/rotation tests.


### External HTML references (28 September, before implementation)

Physical99 reaches the Definitions footer, but tapping its HTTPS reference
navigates QTextBrowser to a blank page. This widget renders bundled documents
and cannot retrieve remote pages. Enable its supported external-link handling
so HTTP/HTTPS references open the Android browser; preserve local relative
document links and anchors inside the owned help sheet. Back from the browser
must return to the same help position/editor without a save or data mutation.
Desktop help remains unchanged. Repeat external/internal links, offline help,
rotation and long-document final-content scrolling on the tablet.
