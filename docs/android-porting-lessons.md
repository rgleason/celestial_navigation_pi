# Celestial Navigation Android porting findings

Working audit, 27 September 2026. These findings supplement
[the OpenCPN Android porting guide](https://github.com/pob220/xweather_routing_pi/blob/db0b2c64b1846571f5162d9c26f0265aec55a957/docs/android-plugin-porting-notes.md).
Physical acceptance remains in progress; see android-acceptance.md and the dated
android-tablet-audit.md. Do not confuse an adapter fix with full feature acceptance.

## Celestial-specific failures found on the tablet

- **Static wxQt thread initialization:** creating wxThread in a plugin can reach
  an uninitialized wx module mutex even when the host itself uses wx threads.
  Eclipse verification crashed at wxMutex::Lock. The Android worker adapter uses
  std::thread/std::mutex with explicit joins; desktop retains wxThread and its
  existing runtime workaround. Exercise the real worker after import, during
  cold-start re-verification, and on cancellation/destruction.
- **Custom modal action IDs:** the pinned wxQt ShowModal turns every nonzero
  native result into OK, losing optional-data action IDs as well as Cancel.
  Read the explicit wx return code. Audit every custom chooser, not only forms
  that return Save/Cancel.
- **Visible list selection must drive calculation selection:** a native card
  view can highlight a new row while a separate wx report-list selection accessor
  retains the old row. Read the actual visible Android index, check its bounds,
  and verify the selected event summary and resulting calculation together.
- **Coordinate normalization can discard precision:** the shared DMM formatter
  is appropriate for result presentation but rounds editable decimal values.
  Android calculation entry must preserve the original text/precision across
  repeated operations. Validate parsed values independently of rounded labels.
- **Optional data are genuinely optional:** analytical navigation works without
  packs; eclipses require verified DE440s, and terrain refinement needs both
  orientation and LOLA. Preserve the published sizes, checksums, coverage and
  supported body centres. Successful status text is insufficient: hash the
  installed bytes and execute the data-dependent calculation.

## Scoped storage and lifecycle

Use an owned asynchronous ACTION_OPEN_DOCUMENT result receiver. Copy the
selected descriptor on a worker in bounded chunks with progress, cancellation,
disk-space checks and a temporary-file lease. Retain that lease across background
checksum verification, then install with an atomic commit. Delete only owned
staging files on cancellation or cold start. Native chooser Back must return
to the browser without installing anything. A rapid local file can finish before
an automated cancellation tap; record the screenshot stage and timing rather
than counting that as a pass. The 506 MiB copy cancellation was physically
executed while 6 MiB had been copied; the previous installed pack remained intact.

Delete Android-owned QTimers before their labels/widgets. Deinitializing a
plugin from Plugin Manager can otherwise leave callbacks accessing destroyed
controls. Retain every crashed library and its matching unstripped binary,
complete logcat and Android exit information. Track PID continuity: an explicit
cold restart and an unnoticed native crash are different events.

## Fresh CI and artifact provenance

- NDK 26 uses a unified layout. Ubuntu's CMake 3.16 looked for the obsolete
  platforms/android-21 directory and failed fresh ARMHF configuration. Both
  Android jobs now use Kitware CMake 3.31.6, verified against its published SHA256;
  the distribution retains its bundled licenses. Existing supported ABIs remain.
- Windows runners may provide python.exe or py.exe without a python3 alias.
  Artifact retention must select and verify Python 3 rather than assume a Unix
  command name. Both Windows builds reached retention before this was discovered.
- A build with OCPN_TARGET and an incremental CPack invocation can produce
  different filenames. Selecting an older wildcard match can package a stale
  library. Hash the library inside the selected archive and independently hash
  the installed library; retain source snapshots for dirty development builds.
- Use fresh support-archive extractions. The Qt 5.12.2 support archive omitted
  public vector source headers behind forwarding headers. Restore matching
  upstream headers with provenance, checksums and licenses, preserving complete
  configure/compiler logs as artifacts.
- The first repaired e7e0cf7 workflow ran all 19 platforms: 16 passed, ARMHF
  configuration and two Windows retention steps failed. Local configuration
  validation and local ABI builds do not turn those remote failures into passes.
  A new exact-source workflow is required after the script repair.

## Protect the navigator's host profile

Inspect the actual chart mode before automation. Android route-creation mode can
consume taps intended for a plugin toolbar icon and create host route points.
Stop that mode, identify only test-created GUIDs, retain a backup, and verify
all original database rows before cleanup. No user route, waypoint, chart or
other plugin may be removed as a shortcut. Restore changed rotation, font and
chart-follow settings after testing.

## Compare package runtime sections with the intended build

An incremental CMake build may use a different CPack filename from a fresh CI
configure with OCPN_TARGET exported. Never select a cached archive by guessing
its suffix. Celestial's local 39 import accidentally used an initial-slice
archive while rewriting metadata to a newer SHA. Installed-library hashes and
exact symbols exposed this error. Root metadata alone cannot establish binary
provenance. Require the intended unstripped build library and compare all
allocated ELF sections/ABI identity with the packaged stripped library; reject
before replacing output. Preserve rejected archives and their actual symbols.

## wxQt unload and Qt gesture delivery

A static private wxQt copy can put Destroy objects into the host pending list.
Collect only plugin-owned top-level windows, remove exact pointers from both
queues, and release wrappers/native roots while their vtables remain mapped.
The pinned wxWindow destructor posts DeferredDelete for parentless
wxQtShortcutHandler objects; a widget-tree walk reporting zero survivors
cannot establish unload safety. Drain already-posted Qt DeferredDelete before
dlclose without pumping arbitrary input, timers or worker callbacks. Retain
QPointer guards for roots a parent may destroy, and clear obsolete wx handler
properties before native destruction. Unregister and release owned route-menu
wrappers and native QMenu/QAction too.

Separately, Plugin Manager's direct touch checkbox handler calls DeInit while
Qt 5.12.2 is iterating gesture recognizers. Destroying QScroller there mutates
that iteration and can crash QGestureManager::getState. The narrow host fix
defers toggling one event turn, uses a native QObject context and wxWeakRef,
and disables the toggle until the operation returns. Verify both configuration
and toolbar/workspace behavior: host checkbox artwork alone can be misleading.

Exercise hot native tarball import after actual editor Save, clock Cancel and
result Close, acknowledge its success modal, then tap the reopened workspace.
Also exercise live disable/re-enable and a cold restart. Each operation has
revealed a different lifecycle failure here; success at one is not evidence
for the others. Record PID continuity and exact loaded binaries.

## Compact numeric text without losing precision

Raw %.17g reopening exposes binary tails such as0.10000000000000001 and
46.414999999999999. For Android editors, choose the first significant-digit
representation which parses back to the identical stored double. Validate
both parsers actually used by the form, preserve signed zero, and keep file
and calculation-log serialization unchanged. A short fixed decimal format
alone would silently discard entered precision. Desktop formatting is unchanged.

## Hidden desktop tables are not an Android data source

On the pinned wxQt, GetItemText(row, column) returned the first column for every
requested residual column. Keep Android selection independent of native
selection in a hidden compatibility table across task pages and modals.
Render Android results
from the calculation records and query card selection from the sight model.
Test selection across pages, filtering to a body with no eligible observations,
sorting, deletion and chart actions; a correct card caption alone is insufficient.

Pinned wxQt's wxCheckBox does not override native SetLabel. Its visible text
can stay generic while its wx label/model and actual action are correct. Supply
the final caption at creation (or explicitly update the native text). Exercise
the action before inferring disabled state from a stale caption or styling.

## Typed spin input needs actual calculation evidence

Typed COG/SOG and fractional seconds can commit native values without emitting
wxSPINCTRLDOUBLE. Subscribe to the actual QSpinBox/QDoubleSpinBox valueChanged
signals, coalesce notifications and run calculations after the input callback
returns. Own the timer by the edited control/dialog, guard its wx lifetime,
and suppress programmatic initialization/SetValue. Test the visible field,
result epoch, result and reopened serialized record independently. Explicit
Calculate actions must also interpret any still-active spin text before reading.

## Planner context and refresh evidence

A generic notebook adapter does not adapt controls outside the notebook. Move
a multi-column planning context into its own scrollable page before decorating
the dialog. Preserve hidden date-field visibility when replacing it with a
touch calendar button, and switch the sizer's replacement item when changing
entry format; showing the old retained native picker creates overlapping fields.

Verify resolved context and resulting events, not just entered text. On this
pinned host, planner wx timer refreshes left old context/results visible. Owned
QTimers plus native spin notifications passed the actual cold retest. Timers
are stopped/owned within the dialog lifecycle; desktop timers remain unchanged.
Use the calculated event records directly for wrapped reports rather than
reading hidden wxQt table columns. Inspect the actual keyboard before issuing
Back during automation, and confirm screenshot dimensions before counting a
rotation as landscape. A settling wait is not a measured performance result.

## Planner nested scrolling and numerical work

A native card list inside a wx scrolling form had two competing drag owners.
Registering the inner gesture or supplying native ScrollPrepare geometry alone
did not repair it. Give long result lists their own full-height page, with
actions above, and move supporting forms to a separate single-scroll page.
Verify actual different rows and the complete final record, not scrollbar movement.

Measure refresh dispatch and computation separately. Hidden display tables were
removed without changing models, but the physical refresh still blocked for
3.5seconds. An owned persistent numerical worker kept GUI dispatch to0–4ms.
Reject stale generations, cancel at numerical evaluation boundaries, keep all
widgets on the GUI thread, and join before unload. Temporary numerical Sight
objects must not read mutable UI configuration or advance the observation colour
cycle. Keep the worker's verified optional-data cache alive across refreshes.
Physical Cancel must remain cleared after the old calculation's completion time;
Close/Back must return to an actually tappable workspace with PID continuity.

The planner's horizontal orientation can show a scrollable list yet hide every
last-card calculation field below a shallow viewport. Put Sort/Direction and
selection/Create on shared rows, remove the redundant footer Close, and keep
the native list as the page's remaining-height owner. Test complete first and
final cards, including Hc/Zn, at normal and enlarged font on the tablet.

Run opt-in desktop GUI smoke cases separately when their fixtures say "run
alone". A combined process can execute a worker test without wxApp, then
retain wx image handlers and UI state across suites. The combined build71 run
failed Find Body's Hide Time lookup and segfaulted in Fix, while each of those
families passed in its own fresh process on the actual display. Retain the
combined failure and the isolating runs; neither compiler success nor a
combined run with skipped UI tests proves desktop GUI behavior.

Round-trip-safe compact number formatting can choose scientific notation for
ordinary integer coordinates: Qt's one-significant-digit `g` renders −20 as
`-2e+01`. Prefer a decimal representation when it still round-trips exactly;
retain scientific notation for very small/large values where fixed decimals
cannot do so. Exercise both the Qt and wx parsers, signed zero and adjacent
double values. This is an Android entry adapter; saved XML and numerical
calculations are unchanged.

AndroidSurface already wraps non-notebook forms in its own scrolling viewport.
Adding a second whole-form wxScrolledWindow can collapse that nested viewport
to one line on the pinned wxQt. Keep form controls directly in the dialog's
content sizer for Android; retain the original desktop scroller conditionally.
Verify actual swipes to bearing, conditions and the complete last result in
both orientations. The first Horizon repair compiled but failed this physical
check; the single-viewport repair passed. A larger system font setting alone
does not prove that Qt actually rendered larger text.


The pinned Android wx/Qt event loop can run a host wxTimer and complete a host
background transfer while leaving a standalone wxEvtHandler pending queue
undelivered. A real DUT1 download proved HTTP200/full bytes/host completion,
then falsely timed out in the plugin. Use a narrowly owned event bridge with a
Qt GUI poll, cancellation, queued-event disposal and owner-bound timer lifetime;
do not pump global pending events or infer network failure from a missing callback.
Physical retesting is required before accepting the repair.


The Android host's background download API can open a non-cancellable native
ProgressDialog. Its transparent window intercepts Cancel/Close/Back even when
the plugin page appears responsive. Confirm Android window/touch routing in
the retained logs before changing wx button handlers. An owned download page
can dismiss its own transfer's spinner through the existing activity method
while preserving host completion/cleanup. Physically exercise Cancel, Close
and both Back halves during active network work, then inspect detached routing,
temporary files, unchanged installed data and PID continuity. A completed
network transfer with stuck plugin events is not an active cancellation test.


### Verify every exported table row on actual ARM hardware

A representative page render can miss systematic clipping. Runtime93 fixed
invalid PDF exponent notation exposed only by ARM centring roundoff, but an
exhaustive raw-text comparison still found306direct-table pages losing their
last row. Runtime94 reserves the inter-table gaps and a rounding margin before
fitting row heights. Full comparison also exposed61rows missing in the earlier
A4 baseline: retain insertion-only corrections, not an incorrect assertion
that the baseline is complete. Compare every logical page across imposed and
normal physical output, check all footers once, and independently verify new
numeric rows. Synthetic host variants and real tablet parsing complement each
other; neither is a substitute for the other.


QTextBrowser only retrieves local help. Enable supported external-link handling
for remote references and physically verify the real Android browser, correct
URL/page and Back to the same document position. Local help/anchors stay owned
by the sheet; a blank remote page is a failure even if tap dispatch succeeds.

Guard formatting entry points as well as model recomputation during Android
editor construction. Notebook decoration can emit a page event before exact
number fields replace rounded desktop captions; RecomputeDMM then corrupts
the stored angle despite Recompute's guard. Assert actual unchanged Save bytes
after reopening/page changes, beyond displayed precision or numeric tolerance.

Parentless generic wx progress dialogs can crash during cold XML reconstruction
on pinned wxQt because no modal parent exists. Do not start nested modal UI
inside a saved-geometry constructor. Preserve exact bearing basis independently
of DR magnetic flags and explicitly test true/magnetic cold reconstruction.


Android waypoint selectors need full-height wrapping cards with labelled
coordinates; wider desktop columns do not solve clipping under font scaling.
Keep GUID/model identity in each card and retain it through filtering. Verify
actual movement after layout settles, both keyboard Back and dialog Back,
rotation and the independently saved coordinate values. Read the actual host
navobj.db schema; an adb command returning exit0 with a missing-file message
is not a successful XML read. Compare sight records by identity/content rather
than list order after sorting.

Invalid coordinate text must be rejected before Find applies a position, not
only presented as N/A. Keep the previous valid model while the user corrects
the field and disable calculations/copies until parsing succeeds. Add a final
Android save guard for invalid imported/stale DR state.


Match the native calendar's displayed leading week, including months beginning
on firstDayOfWeek. Qt shows a full previous week in that case. Test taps on
actual labelled dates, adjacent rows and a nonaligned month in both orientations;
a correct persisted date reached through a wrongly mapped tap is not a pass.
Read fractional UTC milliseconds from the saved format's separate attribute.

Deferred expensive work must also cover every baseline entry action. An
Android calculation button elsewhere did not make the Measurement Results
action complete: it opened an unprepared ephemeris. Centralize the cancellable
operation, preserve cancellation in the parent editor, and reuse validated
inputs. std::function copies retain captured this; callbacks copied from worker
temporaries need rebinding before GUI inspection. Invalidate every derived
result after raw-input changes, including serialized summary corrections.
Compare complete file records to detect stale fields, not only intended inputs.

Qt5.12 Android input reset sets internal handle mode without updating visible
handles. Read the public QInputMethod wrapper as well as the platform context:
update(ImCursorRectangle) emits a signal connected to the visible handle updater.
One unchanged-input replay can pass while changed-input/later popup focus still
fails; retain each result and test later text entry rather than generalize from
the first screenshot. Any deferred cleanup must be owned and conditional on
the same popup remaining open so it cannot interfere with a later editor.

Set numeric control ranges before restoring model values. Expanding a range
after SetValue cannot recover a silently clamped saved span. Keep generated form
definitions in sync, and exercise actual Save/reopen, independent XML reload,
Cancel and defaults. User-requested baseline f201326 is retained in the Android
branch with cherry-pick provenance; its isolated GUI test runs in a fresh process
with a temporary profile. Physical127 also checks wide saved spans and cold
restart without replacing protected observations or solutions.

wx numeric spins contain a native QLineEdit which wx child traversal does not
visit. Adapt both that internal editor and the QAbstractSpinBox border to the
same owned stationary-tap/drag behavior; preserve the spin's validator and
precision. Actual128 replay confirms focused numeric drags scroll without
reopening IME, while stationary input and keyboard rotation still work. Reinspect
coordinates after each page/keyboard/layout transition. A long scripted restore
hit UTC seconds instead of span; full-file comparison caught it and the visible
fields were restored before proceeding. Retain automation failures separately
from product failures and never infer restoration from a successful Save tap.


UTF8 profile131: use explicit ToUTF8 for stored user names, serials and
repeat notes; locale-based ToStdString can silently blank accented strings
on the pinned Android wx build. Seeded load alone is not Save acceptance: do
real two-repeat Build/save with identicalUnicode name, verify count/serial/
points/protectedprofile independently, then cold-reopen. Native Ctrl-C/V
worked for this touch test; Android COPY/PASTE keycodes did not. Also check
programmatic angle updates before Save: the wx text event did not refresh
the active advisory, despite correct repeat cards and saved numerical data.

Tie a reusable prediction to its semantic inputs. Native Qt text, spin and
choice changes can bypass wx notifications: use callbacks owned by the live
dialog and recheck synchronously before Add. Actual UTC and pressure edits
must invalidate a prediction; index error and repeat notes must remain usable
without a new ephemeris calculation. Test recovery after recalculation as well
as refusal. Compare serialized calibration endpoints with a margin limited
to their known rounding precision; do not change the correction algorithm.
An unpaced automation Back closed a dialog during replay133; paced replay134
and independent Save/cold readbacks established the actual recovered behavior.

Preserve existing XML CDATA when staging fixtures: serialize only the added
element into original raw bytes. Rewriting the entire tree stripped CDATA and
caused subsequent TinyXML load/save to collapse protected report whitespace.
Compare complete reports as well as observation attributes. A failed repair
guard must stop dependent UI automation; inspect layout after chart return
before Edit/Delete because those controls move. Saved observation persistence
does not prove saved defaults: verify immediate host config and a new event
after Android cold restart. Flush saved defaults explicitly on Android; Cancel
must leave them alone.

Audit explicit default actions independently from observation Save. The baseline
Set As Defaults changes preferences even when the enclosing observation is
cancelled; on Android flush at that action, not by implicitly persisting ordinary
draft edits. Check hot and cold New Sight, both mutually exclusive correction
flags, immediate exact config keys, and invalid-input refusal. Actual138 negative
pressure refusal preserves the entire config, not just the seven default keys.
Targeted cleanup removes only keys known absent in the pre-test snapshot and
retains every other current byte. Native keyboard rotation can pan the focused
field; dismiss keyboard and inspect final action movement before tapping it.

Check every baseline sortable column in both directions; a five-choice menu
can look complete while silently dropping seven working desktop actions. Use
predeclared varied fixtures, complete saved-record counters, and actual selected
card identity. Native popup positions change with selected row; locate current
caption bounds each time. Highlight after swipe is not activation: read actual
saved order after Back. Isolate destructive tests in a separately backed-up
library and restore raw original bytes, including nested reports. Count XML
solutions at their actual nesting level; a guard failure must stop before writes.

A visible native edit need not reach wx GetValue. Pinned wxQt SetHint uses a
wxEVT_TEXT-updated cache; consuming a bound edit event can leave that cache
stale while the actual field paints the new text. Trace field strings and parsed
epoch before blaming DST conversion. Allow owned edit events to propagate and
read after the deferred debounce. Verify independent date changes, invalid gap/
overlap refusal, and recovery on the real tablet; unit parsing tests alone miss
this defect. Remove temporary raw time diagnostics after establishing cause.
Keep validation hardening distinct from the proven repair. Rebuild CPack target
before package retention; a source/ELF guard must refuse stale archives.

Compare a suspect Android numerical result with actual desktop output before
altering shared mathematics. Moon-phase elongation approximation reproduces
all eight summer/winter Android times exactly on desktop but differs from USNO
FullMoon by623s in June2024. Record that limit rather than claiming phase
accuracy from provider parity. Keep the independent tolerance declared before
physical execution, and label approximations clearly. Font1.3 can reveal clipped
labels even when numeric entries and final outputs remain reachable.

Planner preferences have the same Android flush requirement as explicit sight
and horizon defaults. Actual Close writes the host config only in memory;
independent immediate file readback and real cold reopen must agree with the
last chosen coordinates, format, bases and motion. Flush after the whole existing
key group on Android. Distinguish intentionally unsaved Now/manual instant from
lost persisted preferences; restore only the known preference keys after tests.
