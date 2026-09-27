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
