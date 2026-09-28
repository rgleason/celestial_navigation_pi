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
**Android quick guide**, **Offline manual**, **PDF manual** and clock status.
Ephemeris/DUT1 are reached through planning/data sheets; observation Display
contains appearance controls. Day/dusk/night uses the host toolbar action.
Frequent time
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


### Preserve angles during editor construction (28 September)

Physical99's saved disposable azimuth changed from179.995837 to
179.99583666666666 degrees when reopened and saved. The small difference is
0.000020arcmin but violates exact unchanged-input persistence. Source review
finds notebook-page events can call RecomputeDMM during Android decoration,
before precise decimal fields are installed; it parses the desktop formatted
minutes and mutates the model even though Recompute itself is guarded. Guard
this entry point until construction is ready on Android. Leave desktop behavior
unchanged. Retest a newly typed high-precision bearing, page changes, unchanged
Save, Cancel and cold reload against independent XML bytes.


### Waypoint selection and choice focus (28 September, before implementation)

Physical101 Find Body's waypoint picker retains desktop columns in a shallow
list: names and both coordinate columns are clipped at system font scale1.3.
On Android give this dialog a full-height native card list with wrapping names
and complete labelled coordinates, scaled text and at least48dp rows. The list
owns its scrolling viewport; do not wrap the whole dialog in another sheet.
Retain the existing normalized waypoint model and stable model-index/GUID
selection, filtering names and coordinates, explicit Use Waypoint and Cancel.
Show a clear nonselectable empty-result message. Filtering must retain a
selected GUID when it remains visible; duplicate names must not merge marks.
Rotation must recompute wrapped card heights. Read host waypoints only.
Keep the desktop list construction and behavior unchanged. Test real filtering,
no results, selection, keyboard/rotation, Back and applying exact coordinates.

Physical101 also shows a native selection handle over the section-choice header
after a numeric edit. The custom stationary-release combo action opens its
popup without moving focus away from the previous editor. Before opening a
noneditable choice, commit the input method, clear the old editor focus and hide
the keyboard. Hold the combo with QPointer across focus callbacks. Do not
change editable combo input or desktop behavior. Recheck numeric precision,
page changes and popup Back/rotation on the tablet.


### Reject invalid Find Body positions (28 September, before implementation)

Physical102 manual latitude containing malformed text displayed N/A but
Use position still closed Find and applied a nonfinite DR position to the
parent editor. The disposable edit was cancelled without Save. On Android
parse latitude and longitude with their existing coordinate kinds and finite
range checks before calculating or accepting. Retain the previous valid model
coordinates while invalid text remains in the fields, display unavailable
results, disable estimated-Hs copy, and explain the allowed ranges on attempted
Use position. Valid input restores results. Commit pending numeric text before
acceptance. Add a final finite/range DR check at Android Sight Save as a guard
against invalid imported or stale model state. Desktop behavior is unchanged.
Exercise malformed text, ±90/±180 limits and out-of-range values, correction,
Reset, Cancel and exact valid saved waypoint coordinates on the tablet.


### Calendar months aligned with the first weekday (28 September)

Physical103 March2026 shows a complete previous week (February22–28) above
March1 because the month begins on the calendar's first weekday, Sunday. The
custom calendar tap adapter currently assumes March1 occupies that first row;
a tap on the displayed22 therefore selects29, one week later. The spring UTC
entry persisted the intended29 only because automation targeted the preceding
row. This is a calendar hit-mapping failure, not accepted selection behavior.
When the month starts on the configured first weekday, account for the native
calendar's full preceding week by using a7-day offset. Preserve all other
month offsets and the firstDayOfWeek setting, ranges, native selectedDate and
owned viewport callback. Desktop behavior is unchanged. Test actual displayed
March22 and29 taps, a nonaligned month and rotation, then Save/reopen/cold read
with exact UTC milliseconds through the spring DST date.


### Host day/dusk/night palette (28 September, before implementation105)

Physical104 host toolbar Change Color Scheme correctly dims the chart in dusk
and night, but the Android workspace stays bright white. Hardcoded native
button/report styles bypass DimeWindow. Use the host DILG0/2/3 and UIBCK colors
in a shared Android-only Qt palette/style adapter. Apply after construction and
on SetColorScheme, including owned child sheets, popup lists/calendars, inputs
and reports; retain touch sizes/fonts, disabled/selected contrast and original
styles without accumulating overrides. Preserve actual sight colour previews
and rendered PDF/sky images. No desktop palette/runtime or data change. Verify
actual day/dusk/night workspace/editor/report readability, rotation, popup and
Back, file identity and restoration of the host toolbar and original day mode.

Physical105 dims native controls but wxQt panels still erase with their stored
wx background, so Qt palette alone is insufficient. Update both wx colours
and native Qt presentation. For dusk/night use the host DILG2 black background
for panels as well as inputs: DILG3 grey ink has insufficient contrast against
DILG0 dusk grey. Day keeps host DILG0 panel background. Retest106.

### Android HTML reading size and low-light colours (28 September, before107)

Physical106's native browser is dark in night mode but the bundled HTML still
sets 10.7pt paragraphs, blue/black headings and bright warning backgrounds.
This fails readable help at the tablet's 1.3 font scale. Adapt HTML resources
inside an Android-only QTextBrowser subclass: retain native setSource/history,
fragment anchors, relative document/image resolution and external browser links.
Append reader CSS after the bundled head styles, use the configured Android
font size for body/table/caption text, retain larger heading hierarchy, and use
host ink/dark background for all low-light text and callout/table backgrounds.
Day retains the manual's colours. Do not modify desktop HTML rendering or
printed manual styles. Plain reports use the same configured base font.
Regenerate shipped Android quick-guide HTML from its current Markdown so its
chart action and added planning instructions match. Verify actual night/day/
dusk reading, scroll, anchors, images, browser return and rotation/offline;
failed106 screenshot remains evidence, not a help acceptance pass.

Physical107 appended CSS does not override Qt5's parsed original styles: small
paragraphs and bright boxes remain. Replace this approach with formatting of
the loaded QTextDocument on sourceChanged. Gather fragment ranges first, then
merge only minimum point size and low-light ink/background; retain bold/italic,
heading sizes, anchor attributes and all native source/history behavior. Dim
block/frame/table-cell backgrounds too. This is a revised Android-only design
before108 implementation; keep failed107 evidence.

Physical108 readable night manual/fonts pass, but chapter anchors land at the
wrong position when sourceChanged reformats the same document after native
anchor scrolling. Cache the adapted source URL without its fragment; format
only a newly loaded document, then schedule scrollToAnchor for fragment
navigation after layout. Keep callback owned by the browser. Verify exact
named chapter and distant reference headings, not merely scroll movement.

Physical109 fragment navigation reloads original HTML in this Qt5 build,
restoring small/dark styles despite an unchanged base URL. Remove the source
cache; adapt on every sourceChanged, retaining the owned deferred
scrollToAnchor after formatting. Verify both target heading and large dimmed
body text after navigation and rotation. Do not accept URL caching evidence.

Physical110 target anchors and dimmed typography now agree, but the first
stationary contents tap can be swallowed by native touch scrolling. Give the
browser viewport the existing owned drag filter used by reports. A stationary
release resolves anchorAt against source(): local/fragment links use native
setSource; HTTP/HTTPS/mail links use QDesktopServices. Drags never activate
links and synthesized mouse release is consumed once. Guard browser capture
with QPointer and preserve native URL history. Check single tap after a swipe,
repeat tap, external browser return, plain-text taps and long scrolling.

Physical111 first tap still swallowed, second reaches correct Chapter2 and
relative figure. Native QScroller gesture recognition competes with manual
handleInput in the viewport drag filter. Use only the explicit filter for
reader input; initialize QScroller without grabGesture, retaining QTextEdit's
native scroll event handling. Also map bundled HTML5 figure/figcaption tags to
Qt-supported div/paragraph blocks during resource loading:111 caption wraps
around the inline image. Keep original files and all URLs/images unchanged.
Verify one tap after scrolling plus a caption below its image.

Physical112 still ignores first stationary link tap. Add temporary scoped
help-viewport event/hit diagnostics (public manual URLs only) to distinguish
filter release from anchor hit testing; retain exact diagnostic binary/logs,
then remove diagnostics after the cause is established. No acceptance claim.

Physical113 logs prove first tap hits #circle and changes source; second tap
has empty anchor at the same point in the newly laid-out Chapter2 while the
first screenshot still showed contents. Navigation is processed but painting
is stale until another input. Force document layout before deferred anchor
scroll, then repaint the owned viewport. Retain temporary callback/scroll logs
to verify timing and remove them after the actual single-tap capture passes.

Also stop the reader's QScroller before changing its document/anchor: queued
scroll motion from the release must not restore the old viewport after a
programmatic jump. This is scoped to reader source changes and external-link
activation; it does not alter form/report gesture behavior.

Physical114 logs show SOURCE immediately but no deferred ANCHOR callback until
another input. Zero-delay Qt callbacks do not complete this navigation while
the Android wx modal loop is idle. Override native virtual setSource: call the
base loader, then synchronously force layout/position/repaint after it returns.
Use sourceChanged to apply document styles and handle native history changes
synchronously too. Remove the deferred callback; retain native URLs/history.
Verify first screenshot without a second input, including distant anchors.

Physical115 synchronous path also logs SOURCE without finishing ANCHOR before
capture. Earlier attribution to deferred callbacks alone was incomplete.
Instrument boundaries around scroller stop, document adaptation/layout, anchor
scroll and repaint to identify the blocking operation. Keep this diagnostic
scoped to the reader; no new acceptance or causal claim until measured.

Physical116 quiet capture proves completion without another input: SOURCE/
STOPPED06:59:24.569, ADAPTED06:59:30.122 (5.553s), then immediate layout/anchor/
repaint. Prior apparent ignored taps/deferred stalls were premature captures
of slow visible-document formatting, not missing input. Group all fragment,
block, frame and cell changes in one QTextCursor beginEditBlock/endEditBlock
so visible layout recalculates once. Measure time to ADAPTED and first capture;
retain corrected timing interpretation and remove diagnostics after validation.
### Calendar colour follow-up, 28 September 2026

Physical runtime118 dusk testing found Qt's explicit weekend foreground black
on the dark calendar and an almost invisible selected date. Before changing
code, the intended correction is to apply the current host ink to all seven
weekday formats and the calendar header, and give low-light selections a dim
grey background distinguishable from black. Keep the existing minimum touch
rows, preceding-week date mapping, calendar value and desktop implementation.
Retest March Sunday/Saturday taps, selected day, month changes and rotation;
Cancel must preserve the complete observation file.

Physical119 calendar repair passes actual Sunday/Saturday/month/rotation taps
and Cancel byte-identical XML. Native section popup still uses bright cyan
selection in NIGHT, and a stray cursor handle appears after scrolling the
Time fields. Before repair: reapply the host theme to the live combo popup
after showPopup (Qt initializes it there), including QListView selection rules.
On a genuine control drag crossing the movement threshold, commit and clear
focused text input and hide its IME before scrolling; stationary releases
still focus editable fields normally. Retest night popup selection/rotation,
field drag versus tap/type, keyboard Back, reports and Cancel persistence.

### Iteration121: complete lunar results entry (before implementation)

Physical120 reopened the touch-entered public Greenwich lunar copy, selected
Time on Measurement and obtained an empty recovery sheet; Check at entered UTC
then failed because no ephemeris had been prepared. Android deliberately defers
lunar searches, but this baseline action still assumes synchronous desktop
calculation. Rename the Android action Results and route it through the same
owned cancellable search used by Calculate lunar UTC, reusing a valid unchanged
cache. Cancel must leave the editor open and never open an empty results sheet.
Commit input and end the editor's text focus before the nested worker/results
surface so native insertion handles do not survive in its popup. Leave desktop
labels, calculation timing, numerical algorithms and accepted records unchanged.

Acceptance: reopen a saved public-reference lunar, one Results action produces
candidates without visiting Calculations; entered-UTC check evaluates the same
input; actual Cancel/Back during search keeps the editor and complete saved XML;
unchanged repeat uses cached results. Portrait/landscape/font1.3, final card/log
swipes, precision readback and independent reference tolerances remain required.

### Iteration122: rebind copied Android lunar ephemeris (before implementation)

Review of121 before installation found that the callback created by
RecomputeLunar captures this. Owned searches compute a temporary Sight and
assign it to the editor; its copied std::function otherwise retains the worker
object address after return. The same issue follows candidate inspection.
Android getter will track the callback owner and prepare a fresh callback on
first use after a copy, without repeating the search or changing provider-status
flags. Never dereference the previous owner. Preserve desktop implementation.
Actual entered-UTC branch/residual checks after worker completion and repeated
results must validate the repair; retain uninstalled121 binaries as provenance.

### Iteration123: invalidate derived lunar state on edited inputs (before code)

Physical122 Results/check work, but Save after changing centre/near to upper/far
retained TimeCorrection431s from the prior calculated inputs while its candidate
list was invalidated. FAIL independently observed in sights122-lunar-upper-far.
Track the retained input signature separately from the completed search cache.
When a subsequent signature changes, clear its derived integer time correction;
clear cached callback/position/error state on every deferred invalidation. Preserve
legacy stored correction on first load and unchanged editor Save. Do not alter
raw readings, clock correction, saved solutions or desktop calculation behavior.
Retest calculate→edit→Save exact changed-fields, cold legacy preservation,
repeated/cancelled search and entered-UTC check.

### Complete Android input-context cleanup before choices (before124)

Physical123 Sun contact popup repeats the stray insertion handle after earlier
text entry, despite120 commit/clearFocus/hide. Existing120 drag and popup code
commits composition but does not reset the platform input context. Matching
Qt5.12.2 QAndroidInputContext::commit only finishes composing text; reset clears
its composing state and handle mode. Use the public QInputMethod reset after
committing and clearing focus, for both control drag and native choice opening.
Recheck actual Sun contact/limb popups after text entry and with keyboard hidden,
dim popup/rotation/Back, stationary text/numeric keyboard and final-field drags.
Keep this in AndroidTouch's existing Android guard. No numerical/data/desktop
runtime changes; retain123 failures and complete saved-record validation.

Physical124 repeats the stray handle after an actual stationary text tap,
unchanged typed distance, keyboard Back and Moon-distance popup opening. Reset
alone sets QAndroidInputContext's handle mode to Hidden without dispatching its
updateSelectionHandles. Earlier review of platform update() missed the public
QInputMethod::update wrapper: matching Qt5.12.2 qinputmethod.cpp lines315–337
emits cursorRectangleChanged for ImCursorRectangle, which is connected to the
Android handle updater. After reset, notify the public input method of the
cursor rectangle change in both existing cleanup paths; keep focus/IME behavior
and desktop code unchanged. Retest the exact124 sequence and later entry,
drag, rotation and popup Back before accepting this correction.

125 passes the original unchanged-distance sequence and popup rotation/Back,
but the later changed Sun-altitude27.333246→Back→body-contact popup still has
a handle. The synchronous notification is insufficient for every popup-open
transition. Repeat reset/geometry notification once on the next GUI turn after
showPopup, owned by a QPointer combo and conditional on its popup still being
visible. Never reset after the popup closes, which could interrupt a subsequent
editor tap. Retest both sequences and rapid popup selection followed by typing;
retain125's successful subset and changed-altitude failure separately.

### Numeric spin-field scrolling after127 (before implementation)

Physical bias case128, still running127/445cd67, types session search0.25h,
keyboardBack, then swipes upward beginning on the focused search spin field.
Content moves but the keyboard reopens and obscures the sheet header: FAIL,
resume128-bias-fit-controls.png. Further Back returns safely. Existing editable
QLineEdit drag adaptation only handles a wx control whose root handle is a
QLineEdit. wxSpinCtrl/Double root handles are QAbstractSpinBox, whose internal
QLineEdit is missed by wx child traversal. Apply the same owned tap-versus-drag
filter to that internal editor, preserving native validation and stationary-tap
keyboard behavior. Keep the change inside AndroidTouch's Android guard; shared
runtime/numerics and the requested f201326 correction remain untouched.
Retest the exact focused-field typed/Back/swipe sequence, real final-control
movement, stationary numeric entry, keyboard rotation, popup selection/Back,
Save/reopen/Cancel persistence and all protected records before acceptance.

### Sextant profile Unicode identity after128 (before implementation)

Physical131 seeds one owned profile named “RESUME131 – Sextant α” with serial
“ÉTOILE-131”, preserving the existing profile subgroup and complete config.
Cold runtime128/e0f5cd5 loads two profiles but displays blank active name/serial
and blank saved-choice caption: FAIL, resume131-profile-name.png. The config
still contains the exact UTF8 text. Profile load/save and repeat notes use
ToStdString, while display/storage expects UTF8. Use an explicit UTF8 conversion
for those user text fields on Android only; keep desktop conversion unchanged.
Retest the actual cold name/serial/choice, stationary keyboard and control-origin
drag, actual two-repeat Build/save and same-name replacement, full config readback
and protected profile preservation. A seeded load is not itself Save acceptance.


### Sextant active-profile advisory after prediction (runtime129 physical131)

Actual prediction sets observed angle157.69485075744367deg; IE+1.50′,
repeat cards correctly show afterIE157.66985075744367deg. Before explicit
save, however, the active-profile caption remains at0deg01.5000′ from
the previous index-error edit (resume131-save-ready.png). Explicit Save
refreshes to the correct157deg40.1910′. This stale advisory is a physical
failure, independent of the successful Unicode identity/repeat-note repair.
Android must refresh the active correction explicitly after programmatic
prediction changes the observed angle, through UpdateProfileCorrection
(which already queues the card refresh). Retain desktop event behavior.
Replay prediction before Save, then verify profile save/cold/Back unchanged.


### Sextant input identity and serialized endpoint (runtime130 physical132)

Runtime130 fixes the stale active-angle caption before Save: exact replay
09:25:36.025UTC/Greenwich/Sirius-Vega/IE1.50′ gives157deg40.1910′
(resume132-advisory-before-save). But saved10-significant-digit endpoint
157.6698508 lies4.26e-8deg above the identical calculated angle; strict
comparison falsely says outside tested range. Android warning comparison
will allow1e-7deg (below0.000006′, covering persisted rounding across
0..180deg); numerical correction/profile serialization stay unchanged.

Actual UTC changed25→26min without Predict. Add repeat incorrectly accepts
the old157deg41.6910′ prediction (resume132-changed-utc/stale-repeat),
so inputs and prediction lack identity. Android will capture the semantic
prediction inputs (UTC instant, position, two body selections, contact,
pressure/temperature) after successful Predict. Native text/spin/choice
changes queue an owned GUI-turn comparison, clearing stale prediction and
requesting Predict again. Add performs the same comparison synchronously
before accepting, covering any missed native callback. Observed angle,
IE, uncertainty, note/profile edits do not change pair prediction identity.
Callbacks scoped to live dialog, no deferred/detached thread. Desktop stays
unchanged. Physical replay must refuse UTC-change repeat, preserve existing
repeats, accept fresh prediction, and verify false endpoint warning absent.

### Horizon defaults after Android cold restart (physical135, before implementation)

The actual true60/magnetic57+5−2 saves retain both nominal branches and every
protected observation/report. However, after cold restart a new Horizon Event
shows variation0/deviation0 instead of the saved5/−2 (resume135-new-defaults-fields).
OnOK writes the five horizon defaults to the host wxFileConfig in memory but
does not flush. Android termination need not run the desktop shutdown flush.
Flush after those existing writes on Android only, before returning Save.
No default values, horizon inversion, saved observation format or desktop
runtime change. Verify independent config readback immediately after Save,
cold reload and actual new-event default fields; Cancel must not write defaults.

### Explicit observation defaults (physical137, before implementation)

Actual Set As Defaults with eye3.25m/12°C/1008hPa/IE−1.75′/dip distance2.5NM/
artificial horizon checked updates a hot New Sight, but cold New Sight returns
to2/10/1013/0/0/unchecked (resume137-hot-defaults versus cold-defaults). No
Default keys reach disk. Apply the Android-only flush after the existing seven
OnSetDefaults writes. This explicit action persists defaults even if the
observation is later cancelled, as the baseline does in memory; ordinary Save
or Cancel must not implicitly replace defaults. Artificial horizon clears short
dip by existing mutual exclusion; test both flag modes separately. Desktop and
correction mathematics stay unchanged. Verify exact config keys, hot/cold New
Sight fields, invalid-input refusal, landscape final action and targeted cleanup.

### Observation sort parity (physical139, before implementation)

Actual sort popup only offers Newest, Oldest, Body, Type and Measurement
(resume139-sort-popup). Source baseline sorts inclusion/type/body/UTC/measurement/
colour in both directions. Android lacks inclusion/colour and descending body,
type and measurement. Keep the single native choice and expose all12 explicit
orders: Newest/Oldest, Body A–Z/Z–A, Type ascending/descending, Measurement
increasing/decreasing, Excluded/Included first, Colour ascending/descending.
Use existing comparator and saved-record model; colour ordering is its existing
colour representation, not colour-name alphabetic order. No desktop changes.
Verify every actual selected choice against independently ordered disposable
records, complete record preservation, popup48dp rows, portrait/landscape/Back,
swipe without activation and selected-record identity. Delete All Yes/No/Back
must operate only on a temporarily isolated disposable library after verified
original byte backup; restore original library bytes before further workflows.

### Manual clock cold persistence and report geometry141 (before execution)

Use an isolated one-record public library while original12sights/5reports are
raw-byte backed up. Recorded Sun16:58:00 on14June2024 plus actual global+120s
must retain recorded epoch while report uses17:00:00. Compare report geographic
position against archived primary USNO SunDec23.307827/GHA74.886344 with
predeclared0.1arcminute limits. No chart-polygon sampling inferred. Verify
complete original fixture attributes remain unchanged except ClockError; actual
offline cold/reopened correction/report, keyboard rotation and rejected/Cancel
edits with full byte preservation. Restore protected original raw library only.

### Physical142 local daylight-saving gap (before implementation)

POBsoft (1985–2026). On installed135/a3c42bf, Europe/London local
2026-03-29 01:30:12.987 is nonexistent, but actual manual entry and explicit
Calculate both retain Results ready with resolved01:30:12.987UTC. Valid selected
UTC01:30:12.987 converts correctly to02:30:12.987local. The current adapter checks
the constructed Qt local object's own fields; Android Qt5.12 can retain these
fields even when its epoch resolves to a different wall time. Reconstruct the
local wall clock from the epoch before accepting it, and reconstruct alternative
epochs for overlap detection. Keep this exclusively in the Android adapter.
Re-run existing epoch tests, add independent valid local epochs and half-hour
transition cases, build both platforms and replay the actual failing tablet
workflow. Retain failure evidence; desktop calendar/numerical behavior unchanged.
