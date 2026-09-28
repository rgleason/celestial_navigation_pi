# Android function map and acceptance record

Created 27 September 2026 before implementation. `PENDING` means unexecuted;
source reachability, compilation and a screenshot are not workflow acceptance.
For each row record binary SHA, inputs, independent expected result/tolerance,
actual output/files, screenshots, persistence, Cancel/Back and invalid cases.

| ID | Working baseline function / source | Visible Android destination | Physical status |
|---|---|---|---|
| O01 | New/edit altitude, Sun/Moon/5 planets/catalogue stars; SightDialog | Observe > New / Edit > Measurement | PARTIAL: Sun reference; Venus entry; f04d483 catalogue swipe and exact Polaris selection/Save/reopen retained all other XML fields. fc9a98e popup Back fixed in both orientations; d804ecd rotation closes popup, landscape reopening fits and swipes reach final catalogue row without model activation. Other bodies/variants pending |
| O02 | Celestial azimuth, true/magnetic; SightDialog | Measurement > Azimuth | PENDING |
| O03 | Lower/centre/upper limbs, lunar near/far/body contacts | Measurement > Limb/contact | PENDING |
| O04 | Angle and time uncertainties, lunar search span | Measurement / Time | PARTIAL: d804ecd actual 1.234567891234123 s input/save/reopen serialized identical double; angular and lunar variants pending |
| O05 | Eye height, temperature, pressure, index error, short dip, artificial horizon, saved defaults | Corrections | PENDING |
| O06 | UTC calendar/time, marked UTC application | Time; header Mark time | PARTIAL: d804ecd UTC calendar October 25 01:30:12.987 at autumn DST, New York device-zone reopen and cold persistence; 966c5a7 explicit Use marked UTC/copy round trip retained all 638 ms. Spring DST pending |
| O07 | DR position via Find Body; per-sight DR shift, magnetic shift | Position / Motion | PENDING |
| O08 | Colour/transparency; definitions and calculation log | Appearance / Details | PENDING |
| O09 | Duplicate/delete/delete final/delete all, selection, sorting | Observe cards / Sort / actions | PARTIAL: dcbb4ee Duplicate created identical fourth Venus record, independently read XML and 7f68c9f replacement retained it; d804ecd explicit one-at-a-time deletion through the final record, XML counts, stable empty selection and disabled edits; 966c5a7 Delete All Back retained all complete records; explicit Yes/No and sorting pending |
| O10 | Visibility/inclusion and chart polygons | Card Include/Exclude; Chart | PARTIAL: d804ecd Exclude saved Visible=0 and card Excluded; Include restored exact XML. Empty Chart gives selection guidance. Geometry variants pending |
| O11 | Automatic atomic Sights.xml load/save incl. all fields | Save / reopen / cold restart | PARTIAL: three-record cold reopen and unchanged Save identical (iteration 19); remaining variants pending |
| F01 | Fix algorithms, initial position, error/residuals | Fix > Calculate fix | PARTIAL: empty input gives N/A; iteration54 three independently entered worksheet Sun sights, Plane/Sphere/Cone within0.3NM of independent angular least-squares worksheet position. Legacy Cone2 failed;060fa55 Android derivative repair agrees within0.3NM from distant/nearby precise DR. Invalid91° disables output.0cae732 residual cards and typed COG/SOG live update pass; other cases pending |
| F02 | Running fix COG/SOG, per-sight DR shift, UTC/local epoch | Fix > Motion | PARTIAL:0cae732 worksheet+independent GeographicLib running reference, COG90/SOG5 gives43°17.7620′N77°06.1079′W/RMS18.08′ within0.1NM/0.1arcmin; live recalculation and UTC21:18:20 ↔ London22:18:20 same instant/result. 2207d08 typed seconds immediately recalculate; 0ccce32 shows .987 epoch and UTC→London local retains .987 and the same instant. Per-sight shifts/bad epoch/DST pending |
| F03 | Saved lunar solution selection in fix | Fix > Clock source | PARTIAL: d523bb2 actual native popup exposes all saved solutions; selecting public2024 correction with2025 Sun sights safely rejects Watch mismatch, N/A and disabled chart action with full12h guidance. Matching-watch positive case and applied geometry pending |
| F04 | Fix cross and chart centring (baseline OnGo; no waypoint creation command) | Fix > Show fix on chart | PARTIAL:060fa55 centre/red cross with surviving Sun/eclipses overlays, workspace hidden and PID23025 continuous. Nonzero heading/regressions pending |
| F05 | Repeated sequence scatter/bias/trend/outliers, selected-body filter, moving observer and plot | Fix > Analyze sequence | PARTIAL: 40bae1a stationary and moving statistics/outlier match independent worksheet/GeographicLib references; complete cards, both orientations, font scale 1.3, final-control swipes, invalid latitude clears results, keyboard-open rotation and nested Back passed; saved XML byte-identical. 0ccce32 actual Saturn caption/filter gives the expected empty-altitude validation; f4e53cc checked Sun filter retains all three correct results. Earlier disabled-filter inference was untested and retracted; broader regressions pending |
| P01 | Manual/boat/cursor/DR/last fix/waypoint context with freshness | Plan > Context | PARTIAL: 1a2a7af full scrollable manual Greenwich position, actual cold persistence of precise coordinates/format; other sources/freshness/invalids pending |
| P02 | Now/selected/manual time; UTC/local/ship zone; display UTC/local/LMT/fixed offset, auto zone | Plan > Time | PARTIAL: 1a2a7af visible calendar with actual year keyboard, typed hour/minute/.987 seconds immediately update resolved 2024-06-21 00:00:00.987 UTC; entry-format switch retains original .329. Other bases/sources/invalids pending |
| P03 | Sun/Moon rise/set, civil/nautical/astronomical twilight, transit | Plan > Events | PARTIAL: 1a2a7af Greenwich 2024-06-21 sea-level sunrise03:42:22/sunset20:21:27 UTC within existing published fixture5min tolerance; complete wrapped result fields and genuine final Moon-summary swipes in portrait/landscape. Other references/polar/motion/invalids pending |
| P04 | Bodies/best sights, Hc filters, sorting, sky magnitude/below-horizon plot, create sight | Plan > Bodies / Recommendations & sky | PARTIAL: 047ce08 complete body cards, Hc descending sort and Moon identity retained through actual swipes to final Adhara; 5846bf1 owned worker with active Cancel/Close/Back and exact file preservation. 8ea2e0b landscape full first/final Alioth cards, selected-body identity and Create sight editor body; Cancel preserved saved records. 1.3 font kept a complete first body card. 61b0066 physically created and saved a Venus sight from selected Sun2 context at 17:16:33.987 UTC with exact body, centre limb, measurement and DR in XML; cold reopen showed five cards, deletion restored the original four and exact XML SHA. Other sorting/filter/sky cases pending |
| P05 | GHA/Dec almanac, CSV export | Plan > Almanac > Export CSV | PARTIAL: aa861d3 worksheet Sun Hc/declination within0.1arcminute, real175-row/seven-body/nine-column CSV independently read; 5846bf1 active Cancel leaves empty results and Export disabled. Runtime70 .987 CSV independently read and six visible bodies' Hc/Dec within0.1arcmin of public USNO references. Explicit overwrite No/Back/browser Back preserved existing file. 8ea2e0b landscape last Polaris UTC/Hc/Zn visible at fonts1.15/1.3. 61b0066 explicit Replace of backed-up disposable CSV wrote a new independently read 175-row, seven-body, 25-instant, nine-column file with displayed Sun values and unchanged sights. Other contexts pending |
| P06 | Apparent-noon and Polaris latitude | Plan > Noon / Polaris | PARTIAL: runtime70 real northern noon and Polaris solved within0.0033/0.0029arcmin of independent formula/DR; Ho91 rejected. Southern/below-horizon cases and rotation pending |
| P07 | Time-tagged moving observer | Plan > Motion | PARTIAL: 61b0066 physical selected-Sun2 DR 43.2366916666667,-77.533415, reference 2026-09-27 22:34:02.431 UTC, true COG90/SOG10; moving nautical dusk/sunrise observer coordinates agree with independent PROJ WGS84 geodesic within 4.7/1.1 m. Motion off restored fixed DR at every visible event; mode and COG/SOG survived Planner Close/reopen, sight XML SHA unchanged. Other context/time/invalid/motion cases pending |
| P08 | Find Body manual/live boat/cursor/fix/waypoint, reset, estimated Hs, observed Ho, Cancel | Observe > Find Body | PARTIAL: manual Sun DR reduction agrees within 0.1 arcmin (iteration 18); other sources/Cancel pending |
| T01 | Local/UTC display, GNSS freshness/difference, device/system status, marked-time copy | Tools > Time; header | PARTIAL: 966c5a7 London local/UTC/fresh RMC with latency, chrony unavailable, held clocks/copy exact .638Z and explicit observation application; stale/offline variants pending |
| T02 | Manual clock correction, Apply versus Cancel, recalculation | Tools > Clock correction | PARTIAL: 966c5a7 +120 typed, Cancel/Discard retained complete XML; Apply persisted +120 and reopened; zero restoration returned parsed XML identically. Corrected geometry/nonzero cold persistence pending |
| T03 | DUT1 offline update/status/provenance and analytical/DE440 provider status | Tools > Ephemeris / DUT1 | PARTIAL: runtime84 shows bundled1972–2027-09-11 coverage, no downloaded update, explicit UT1=UTC fallback; actual local/native chooser Back safely returns without changing data. 689e2f6 actual validated online installation/repeat, active Cancel/Close/Back, invalid-size/date-MJD/older-coverage native imports, chooser Back, larger font/orientation and offline failure/cold coverage pass; provider variants pending |
| H01 | Sunrise/sunset observation UTC, source, uncertainties, height/weather/horizon quality | Observe > Horizon event | PARTIAL: 4145edc corrected whole-form viewport; actual swipes reach complete weather/quality/results in portrait and landscape. Saved Sunrise 2026-09-27 11:02:29.987 UTC with explicitly selected Other manual entry, default 2 s time uncertainty, eye2m/temp10C/pressure1013hPa/clear horizon/refraction10′. Independent XML preserves original four sights; cold reopen and unchanged Save retain identical bytes, deleting disposable sunrise restores exact original file. Manual typing does not automatically switch source in this build. Other variants/invalids/Cancel pending |
| H02 | Optional true/magnetic bearing, deviation/variation, position branches, edit, chart | Horizon event > Bearing / results | PARTIAL: 48bcd97 actual magnetic90+east5−west2 produced live93° true; both geographic branches and entire explanation visible after final swipe. Haze set20′ and warning; unsaved Back retained exact original XML. Independent branch geometry, true-bearing variant, Save/reopen/chart pending |
| L01 | Lunar distance/altitudes/limbs/uncertainties, separate measurement times, watch basis, motion | Observe > Lunar | PENDING |
| L02 | Known position or joint UTC/position single solution, candidates, logs, stored solution | Lunar > Results | PARTIAL: 966c5a7 Fiji Saturn joint solve, southern branch 0.7 NM from worksheet (1 NM tolerance), saved correction 7.0458984375 s matches existing regression; 93f423a complete candidate cards, actual northern/southern branch selection and correctly named stored XML, empty-name rejection/Cancel without mutation, and initial wrapping verified. Other variants pending |
| L03 | Session select visible/clear, time+position/time-known-position, earliest DR, robust/bias/motion | Tools > Lunar sessions | PARTIAL: 7b34fd4 complete model observation cards, Clear/Select visible and explicit one-observation rejection with safe Back; full controls reached by actual swipes. 03495ff cold-loaded public four-reading known-position mode solves within1s of independent USNO clock; d523bb2 all residual cards and fixed-position summary. Joint runtime85 failed convergence; b416119 actual four-reading joint solve now within60s/3NM/RMS0.5′ independent fixture tolerances. Robust off invalidates/re-solves consistently; robust outliers/bias/motion pending |
| L04 | Worker cancellation, candidate selection, stored session solution, reopen/details/copy | Lunar sessions > Results / saved | PARTIAL: d523bb2 native setting edit clears candidates/disables Save; named four-input public solution saves strict XML with intact UTF8 labels, exact−0.4465910566s, original observations/clock/three solutions unchanged. Cold reload/final report swipe/font1.3/Back preserve exact file. b416119 correct read-only Close and font1.3 landscape final residual warning/rotation; actual active-worker Cancel and Back clear candidates/disable Save with exact file preservation/PID continuity. 689e2f6 cold offline88 joint report/final residuals and actual clipboard paste endpoint pass with unchanged files; full clipboard byte readback and broader variants pending |
| L05 | Lunar pair planning, measured guidance, bodies and timing | Plan > Lunar pairs | PARTIAL: 7b34fd4 Greenwich historical physical Spica/Vega/Deneb distances/Hc within0.1′ of independent USNO, rates/sensitivity within displayed rounding; all11 fields and final Betelgeuse Magnitude visible by actual portrait/landscape swipes. Invalid latitude/date/hour clears/rejects safely; other variants/font reopen pending |
| S01 | Sextant body pair/contact prediction, index correction, repeats, remove | Tools > Sextant check | PARTIAL: 5b8267c full-width fields and model-backed complete repeat cards; real Deneb/Vega prediction (astronomical reference pending), raw=predicted with IE+1.50′ gives independently checked AfterIE/raw−0.025deg and residual+1.50′. Two repeats, selected and final removal, explicit fewer-than-two validation, safe Back/PID continuity; 7b34fd4 public Greenwich Deneb/Vega apparent centre prediction independently agrees with USNO within0.01106′ (0.1′ tolerance); other contacts/variants pending |
| S02 | Calibration profiles/name/serial, build/save/select/persistence | Sextant check > Profiles | PARTIAL: 5b8267c actual disposable named profile/serial saved and independently read in config, complete landscape final explanation. Two ±0.20′ identical repeats incorrectly saved ±0.00′ uncertainty; Android-only 1e0aae4 correction physically rebuilt the same named profile: Count1, persisted uncertainty0.1414213562′=0.2/√2, displayed±0.14′; cold restart/reopen retained name/serial/point. Original sights unchanged; duplicate names/other cases pending |
| C01 | Vertical-angle above/below-horizon modes, object heights/eye/angle/uncertainty, range | Tools > Coastal > Vertical | PARTIAL: waterline reference 0.974 NM; sea-horizon 2.9′/2.8′ references 9.676/9.797 NM, including negative corrected angle; retained reopen and reset checked; remaining invalid/uncertainty cases pending |
| C02 | Optional true/magnetic bearing, variation/WMM/deviation, waypoint/place selection | Coastal > Bearing / positions | PARTIAL: true bearings 75.22° waterline and 65.22° sea-horizon positions match independent references; magnetic/WMM/waypoint pending |
| C03 | Horizontal angles, three objects, observer DR, sequential times/motion, fix | Coastal > Horizontal | PARTIAL: Bob revision 1 second fix and 0.149 NM formal uncertainty match; chart/reopen/empty-input rejection checked; sequential motion pending |
| C04 | Range circles/arcs/fix overlay, New/clear observation, clear plots, hide/reopen | Coastal > Results / Chart | PARTIAL: vertical circle/bearing and horizontal loci/fix visible; device rotation, reopen, clearing without input loss, reset/refusal checked; confirmation Back ignored in 29 and fixed/retested in 31; chart heading rotation pending |
| A01 | Presets, route GUID/position/band/global coverage, dates, DUT1 | Plan > Voyage almanac > Coverage | PENDING |
| A02 | All content toggles, planning cadence, safety/calculator/paper self-contained modes | Almanac > Content | PENDING |
| A03 | Direct/universal tables, forms counts, paper sizes, booklet/signature, page estimate | Almanac > Paper / Forms | PENDING |
| A04 | Preview, in-process PDF generation, output/overwrite/cancel, viewer, actual bytes | Almanac > Preview / Generate | PARTIAL: real 28-page PDF, independently inspected bytes/pages/sources (iteration 22); page entry and last-page rendering repaired and exercised; cancellation/overwrite and other outputs pending |
| E01 | Year-span eclipse search/list/selection | Plan > Eclipses > Search | PARTIAL: committed dcbb4ee 2027 cards/selection, reverse-year rejection and 1850–2100 worker Back cancellation retaining previous result; 7f68c9f completed full-span search, background/resume, swipe without selection, final 2100 card and tap-to-Local selection; larger fonts pending |
| E02 | Path/partial magnitude contours, plot selected/clear, pan/zoom/rotation | Eclipses > Chart | PARTIAL: 7f68c9f August 2027 path/contours, chart centre/hide, real pan/zoom, device landscape/portrait, reopen and Clear preserving other geometry; nonzero heading rotation and plot option combinations pending |
| E03 | Local position/boat, contact circumstances, LOLA refine | Eclipses > Local | PARTIAL: standard duration 382.44 s versus NASA 382.6 s (1 s tolerance), real LOLA calculation, portrait/landscape final results; dcbb4ee precise decimal retention through repeat/reopen and latitude-range rejection; independent terrain accuracy/boat/remaining cases pending |
| E04 | DE440s download/import, optional orientation/LOLA, hash/format/coverage verification and cancel | Tools > Data / Eclipses > Data | PARTIAL: all three scoped-storage imports, independent official hashes, cold re-verification and effective 506 MiB copy Cancel; invalid-size rejection/native chooser Back retained previous file and cleaned staging; checksum/download/coverage cases pending |
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
Committed dcbb4ee ARM64 installed through patched-host Plugin Manager with actual library hash verified and PID continuity. The earlier clock teardown and verification-thread crashes were repaired and rerun. Final-source cold acceptance and full feature coverage remain pending.

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
