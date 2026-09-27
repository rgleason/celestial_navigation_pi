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
