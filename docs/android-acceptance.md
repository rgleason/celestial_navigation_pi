# Android function map and acceptance record

Created 27 September 2026 before implementation. `PENDING` means unexecuted;
source reachability, compilation and a screenshot are not workflow acceptance.
For each row record binary SHA, inputs, independent expected result/tolerance,
actual output/files, screenshots, persistence, Cancel/Back and invalid cases.

| ID | Working baseline function / source | Visible Android destination | Physical status |
|---|---|---|---|
| O01 | New/edit altitude, Sun/Moon/5 planets/catalogue stars; SightDialog | Observe > New / Edit > Measurement | PARTIAL: Sun reference; Venus entry. Other bodies/variants pending |
| O02 | Celestial azimuth, true/magnetic; SightDialog | Measurement > Azimuth | PENDING |
| O03 | Lower/centre/upper limbs, lunar near/far/body contacts | Measurement > Limb/contact | PENDING |
| O04 | Angle and time uncertainties, lunar search span | Measurement / Time | PENDING |
| O05 | Eye height, temperature, pressure, index error, short dip, artificial horizon, saved defaults | Corrections | PENDING |
| O06 | UTC calendar/time, marked UTC application | Time; header Mark time | PENDING |
| O07 | DR position via Find Body; per-sight DR shift, magnetic shift | Position / Motion | PENDING |
| O08 | Colour/transparency; definitions and calculation log | Appearance / Details | PENDING |
| O09 | Duplicate/delete/delete final/delete all, selection, sorting | Observe cards / Sort / actions | PENDING |
| O10 | Visibility/inclusion and chart polygons | Card Include/Exclude; Chart | PENDING |
| O11 | Automatic atomic Sights.xml load/save incl. all fields | Save / reopen / cold restart | PARTIAL: three-record cold reopen and unchanged Save identical (iteration 19); remaining variants pending |
| F01 | Fix algorithms, initial position, error/residuals | Fix > Calculate fix | PENDING |
| F02 | Running fix COG/SOG, per-sight DR shift, UTC/local epoch | Fix > Motion | PENDING |
| F03 | Saved lunar solution selection in fix | Fix > Clock source | PENDING |
| F04 | Fix chart geometry, waypoint output | Fix > Results > Chart / waypoint | PENDING |
| F05 | Repeated sequence scatter/bias/trend/outliers, selected-body filter, moving observer and plot | Fix > Analyze sequence | PENDING |
| P01 | Manual/boat/cursor/DR/last fix/waypoint context with freshness | Plan > Context | PENDING |
| P02 | Now/selected/manual time; UTC/local/ship zone; display UTC/local/LMT/fixed offset, auto zone | Plan > Time | PENDING |
| P03 | Sun/Moon rise/set, civil/nautical/astronomical twilight, transit | Plan > Events | PENDING |
| P04 | Bodies/best sights, Hc filters, sorting, sky magnitude/below-horizon plot, create sight | Plan > Bodies | PENDING |
| P05 | GHA/Dec almanac, CSV export | Plan > Tables > Export CSV | PENDING |
| P06 | Apparent-noon and Polaris latitude | Plan > Noon / Polaris | PENDING |
| P07 | Time-tagged moving observer | Plan > Motion | PENDING |
| P08 | Find Body manual/live boat/cursor/fix/waypoint, reset, estimated Hs, observed Ho, Cancel | Observe > Find Body | PARTIAL: manual Sun DR reduction agrees within 0.1 arcmin (iteration 18); other sources/Cancel pending |
| T01 | Local/UTC display, GNSS freshness/difference, device/system status, marked-time copy | Tools > Time; header | PENDING |
| T02 | Manual clock correction, Apply versus Cancel, recalculation | Tools > Clock correction | PENDING |
| T03 | DUT1 offline update/status/provenance and analytical/DE440 provider status | Tools > Ephemeris / DUT1 | PENDING |
| H01 | Sunrise/sunset observation UTC, source, uncertainties, height/weather/horizon quality | Observe > Horizon event | PENDING |
| H02 | Optional true/magnetic bearing, deviation/variation, position branches, edit, chart | Horizon event > Bearing / results | PENDING |
| L01 | Lunar distance/altitudes/limbs/uncertainties, separate measurement times, watch basis, motion | Observe > Lunar | PENDING |
| L02 | Known position or joint UTC/position single solution, candidates, logs, stored solution | Lunar > Results | PENDING |
| L03 | Session select visible/clear, time+position/time-known-position, earliest DR, robust/bias/motion | Tools > Lunar sessions | PENDING |
| L04 | Worker cancellation, candidate selection, stored session solution, reopen/details/copy | Lunar sessions > Results / saved | PENDING |
| L05 | Lunar pair planning, measured guidance, bodies and timing | Plan > Lunar pairs | PENDING |
| S01 | Sextant body pair/contact prediction, index correction, repeats, remove | Tools > Sextant check | PENDING |
| S02 | Calibration profiles/name/serial, build/save/select/persistence | Sextant check > Profiles | PENDING |
| C01 | Vertical-angle above/below-horizon modes, object heights/eye/angle/uncertainty, range | Tools > Coastal > Vertical | PARTIAL: waterline-to-top independent reference0.974NM and retained reopen; sea horizon, invalid and clearing pending |
| C02 | Optional true/magnetic bearing, variation/WMM/deviation, waypoint/place selection | Coastal > Bearing / positions | PARTIAL: true bearing75.22°, independently matching position; magnetic/WMM/waypoint pending |
| C03 | Horizontal angles, three objects, observer DR, sequential times/motion, fix | Coastal > Horizontal | PENDING |
| C04 | Range circles/arcs/fix overlay, New/clear observation, clear plots, hide/reopen | Coastal > Results / Chart | PARTIAL: real vertical circle/fix, scale/bearing correct, Chart hides workspace and retains geometry; horizontal/clear/rotation pending |
| A01 | Presets, route GUID/position/band/global coverage, dates, DUT1 | Plan > Voyage almanac > Coverage | PENDING |
| A02 | All content toggles, planning cadence, safety/calculator/paper self-contained modes | Almanac > Content | PENDING |
| A03 | Direct/universal tables, forms counts, paper sizes, booklet/signature, page estimate | Almanac > Paper / Forms | PENDING |
| A04 | Preview, in-process PDF generation, output/overwrite/cancel, viewer, actual bytes | Almanac > Preview / Generate | PARTIAL: real 28-page PDF, independently inspected bytes/pages/sources (iteration 22); page entry and last-page rendering repaired and exercised; cancellation/overwrite and other outputs pending |
| E01 | Year-span eclipse search/list/selection | Plan > Eclipses > Search | PENDING |
| E02 | Path/partial magnitude contours, plot selected/clear, pan/zoom/rotation | Eclipses > Chart | PENDING |
| E03 | Local position/boat, contact circumstances, LOLA refine | Eclipses > Local | PENDING |
| E04 | DE440s download/import, optional orientation/LOLA, hash/format/coverage verification and cancel | Tools > Data / Eclipses > Data | PENDING |
| D01 | Offline HTML manual/definitions, long scrolling and links | Tools > Manual | PENDING |
| D02 | Bundled PDF manual, current version, actual viewer | Tools > PDF manual | PARTIAL: 2.8.13, 42 pages, last page rendered and Next disabled; keyboard-only Back retained viewer. More cross-cutting cases pending |
| D03 | Host colour scheme, sight display controls, workspace/chart visibility | Tools > Display; Chart | PENDING |

## Cross-cutting acceptance (every applicable editor/family)

- Valid full workflow; invalid/empty inputs; Save versus Cancel/Back; reopened
  model and independent saved file agree on units/sign/precision/identity/epoch.
- Both orientations; rotation with sheet/popup/keyboard; actual final-control
  content swipe; 48 dp targets including popup rows; font scaling; long output.
- Nested Back consumes press/release and preserves host PID; background/resume,
  cold restart, plugin disable/re-enable; no invisible input owner.
- Repeated taps/actions, swipe beginning on controls, deletion of final record,
  duplicate host names/profile names, overwrite refusal and interrupted work.
- Offline analytical fallback plus verified optional data; missing/invalid/out
  of coverage is explicit. Independent reference cases and existing suites.
- Coexistence with enabled xGRIB/xWeatherRouting; overlay state restoration,
  correct chart location and lifetime; inspect actual host outputs/files.
- Exact PID before/after, foreground package, full crash/ANR logs and memory.
  Retain exact binaries for symbolication; auto-restart is a failed continuity check.
- Root metadata, final URL/identity/version/API/ABI/assets/package hash; Plugin
  Manager import and cold restart; fresh dependency extraction; all platform CI.

## Access evidence recorded before implementation

ADB serial R9TY601V4BA; SM-X210, Android 15, 1200x1920 natural pixels, 240 dpi,
Europe/London. Host org.opencpn.opencpn.dev 5.14.0 (code 128), arm64-v8a,
QtActivity launcher; DEBUGGABLE/run-as works. Fine/coarse location granted;
legacy storage and Bluetooth runtime permissions not granted. App-owned files
are readable/writable through run-as; a disposable restore probe matched SHA256
`577a9daa3bfdaeb6c426706898847b70b79fd7e58518db64843b03bfa8f5ad9a`.
Screenshot and touch operation confirmed, initial PID 24355. API 1.21 host capability and actual plugin loading were subsequently verified.
Development builds are installed. Patched-host cold-state Plugin Manager imports passed; replacement after workspace use exposed an Android clock teardown crash, under repair in iteration27. Final-source import/cold acceptance remains pending.

Evidence and backups: `/tmp/celnav-android-20260927/` (contains personal profile;
do not commit/upload). private-backup.tar, 97 MiB, SHA256
`cf43548ddb7856f91492a61b8565de5b08f8ab59b4a37be26b4bd6f467a2e179`;
profile-backup.tar, 630 MiB, SHA256
`f4768aae6aa55da8590f25d2ee41fc65b166da562a7af3d6b3739706e7e07182`.
Existing charts and other plugins must be preserved. Initial crash and ANR
buffers were retained, not treated as evidence about this new plugin.

## Tester distribution requested 27 September 2026

After tablet acceptance, provide a downloadable Android manual-import tarball
for testers on their own tablets. It must include root-level `metadata.xml`,
matching plugin/version/ABI/API identity, and the exact final archive URL.
Exercise Plugin Manager import and cold restart, verify hosted archive contents
and SHA256, and retain the URL/checksum in the release handoff. This is manual
tester distribution; publication to the main OpenCPN plugin catalogue remains
outside authorization. Status: **PENDING**, dependent on tablet acceptance.
