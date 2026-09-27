# Celestial Navigation 2.8.13 Android audit — 27 September 2026

Status: development in progress; **not accepted for release**. See [acceptance matrix](android-acceptance.md). Pending features are not passes.

## Scope and provenance

Branch `android/celnav-2.8.13` in `/home/paul/src/OpenCPN/.worktrees/celnav-android-2.8.13`. Baseline upstream `8b7b95078b98e0cc87f5602e1710c4f6435fea60` (2.8.11.0) plus local documentation commit `4435de5088666933fb46ee9663a36fbb42baea87` (2.8.12 manuals). Upstream 2.9 development is outside the requested 2.8 scope. No 2.8.13 tag was found at initial remote inspection. Existing checkouts and their unrelated changes remain untouched.

Desktop behavior must remain at the baseline: documentation and release version only. New runtime behavior, time adaptations, layouts and GLES repairs are Android conditional.

Design and function mapping were committed before implementation: `eb5fd64`. Four task pages: Observe, Fix, Plan, Tools; focused sheets with visible Save/Cancel and section selectors.

## Physical device and access

Samsung SM-X210, Android 15, serial `R9TY601V4BA`; natural 1200×1920, density 240 (48 dp = 72 physical pixels). Landscape 1920×1200 tested initially. Host `org.opencpn.opencpn.dev`, 5.14.0 / code 128, arm64-v8a, plugin API 1.21 available, DEBUGGABLE. Launcher `org.opencpn.opencpn.dev/org.qtproject.qt5.android.bindings.QtActivity`. Plugin currently requests backwards-compatible API 1.18. Current timezone Europe/London.

ADB launch, taps, screenshots inspected locally, full logcat/crash evidence, last ANR inspection, private `run-as` reads/writes, direct plugin deployment and disposable-file backup/restore were exercised. Fine/coarse location granted; legacy storage and Bluetooth runtime grants absent. Other plugins, charts and profile were preserved. xGRIB 0.3.2 and xWeatherRouting 1.18.6 remain installed/enabled, confirmed visually.

## Backups (private; never publish)

Retained under `artifacts/android-audit-20260927/` (git ignored) in this worktree, with original working copies in `/tmp/celnav-android-20260927/`.

- `private-backup.tar`: app private `manPlug`, shared preferences and files, SHA256 `cf43548ddb7856f91492a61b8565de5b08f8ab59b4a37be26b4bd6f467a2e179`.
- `profile-backup.tar`: external OpenCPN configuration, navobj.db, plugin user files and GRIBs, SHA256 `f4768aae6aa55da8590f25d2ee41fc65b166da562a7af3d6b3739706e7e07182`.
- No existing Celestial binary was present. Development deployment adds its library/assets; the original backup therefore restores absence of Celestial.
- A disposable private file was restored to SHA256 `577a9daa3bfdaeb6c426706898847b70b79fd7e58518db64843b03bfa8f5ad9a`, then removed.

## Build and shared regression evidence

NDK 26.1.10909125; Qt 5.12.2/wxQt support v1.2 extracted freshly from the archive, not a previously repaired shared cache. Core source `91f3b674366068a6ecd61a5e9aba204bba85f57e` (5.14), matching locally built core library used for linkage. Missing public Qt vector headers restored from checksum-verified unmodified upstream sources; see `ci/android-qt-headers/README.md`.

Full configure/compiler output retained in `/tmp/celnav-android-20260927/`, not inferred from a truncated console. AArch64 identity, SONAME and dynamic dependencies inspected. Packaging/install through Plugin Manager is pending; current deployment is a development copy.

Shared test suite: 207 passed, six opt-in GUI cases skipped in the full numerical run. Verified optional fixtures supplied: DE440s, lunar orientation and LOLA64 terrain. Then each GUI case ran alone on the real local desktop display and passed: LunarUiSmoke, FindBodyUi, FixUi, CoastalUiSmoke, HorizonEventUi and AlmanacUi. Initial missing-fixture failures and display-sandbox failure retained alongside successful reruns. These are desktop checks, not tablet feature acceptance.

## Tablet iteration evidence

Screenshots, inputs, XML, logs and exact installed/unstripped libraries retained in `/tmp/celnav-android-20260927/`; a retained workspace copy is maintained under the ignored artifacts directory.

1. Initial arm64 library built and loaded. Disabled unmanaged plugins with no imported metadata are removed from the host list; enabling the authorized development copy exposed the toolbar. Plugin Manager package installation remains to be tested.
2. Cold workspace appeared and accepted taps. New sight crashed in private wxColourDatabase initialization. Symbolicated retained binary identified the colour picker call; Android factory now initializes private wx stock lists. Retest New sight no longer crashed.
3. Touch swipe genuinely moved sight content to the final measurement/uncertainty fields. Exact measurement `35 15.1234` was entered. UTC seconds `12.345` were entered, keyboard dismissed, visible Save used. First Save crashed in GLES DrawPolygon (null/client attribute access); the observation had already persisted. Crash and binary retained; it is a failure, not a pass.
4. GLES polygons now use VBO storage, uniform colour correctly, and restore attributes/buffers/program/blending. Degenerate polygons are handled. Line/circle paths also routed through state-preserving drawing. Save retest retained PID 6511; XML independently confirmed measurement `35.252056666666668`, seconds 12 and milliseconds 345. Sun and Venus disposable records survive a cold restart (PID 7049).
5. Initial New sight displayed local 09:03 as UTC while device UTC was 08:03. Android time boundary now uses Qt epoch/UTC conversions. New sight retest displayed 08:09:28.670; entered 12.345 persisted as 08:09:12.345. DST gap/overlap and timezone-change tests remain pending; this current-clock check does not establish them.
6. Card swipes reached final Edit/Duplicate/Include/Delete actions without selecting a card. The calculation page remained blank despite native visibility. Native diagnostics found a zero-height content panel; fix is being retested. Calculation-report acceptance remains pending.

Historical Sept 26 host crashes in the initial crash buffer are distinguished from Sept 27 Celestial failures by PID/timestamp. Auto-restart occurred after crashes, so process continuity is checked rather than assuming a returned host is stable.

## Still required

Complete the early calculation/chart/save/reopen slice, then every mapped feature family, invalid/cancel/back cases, portrait/rotation/font scaling, host lifecycle, offline and optional-data workflows. Finish precise UTC/DST handling and touch calendars. Generate/inspect updated HTML/PDF/DOCX manuals. Complete both Android ABIs, root metadata with final URLs, reproducible CI builds and all existing platform checks, local commits and release handoff. Manual tester distribution and a downloadable archive URL have been requested after tablet acceptance. Main-catalogue publication remains outside authorization.

## Iterations 7–18: report, precision, documents and time (in progress)

- Iteration 7 repaired the zero-height wxQt content panel; calculation reports became visible. Iteration 8 confirmed the correct UTC report text, then exposed text selection instead of document scrolling. Iteration 9 made readonly report content move, but reopening fractional seconds showed 12.350 instead of the saved 12.345. Setting digits before the initial Qt spin value repaired that rounding.
- Iteration 10 reopened 12.345 exactly, but swiping the calendar changed September 27 to September 26. This was a failure; the affected Sun record is disposable. Iteration 11 consumes calendar swipes without selecting a day; a subsequent unchanged Save preserved every attribute of ClockError and both sight records (`slice11-before-save.xml` versus `slice11-after-save.xml`). This is a specific round-trip pass, not full observation acceptance.
- Find Body was a tiny unadapted floating window in iteration 11. Iteration 14 introduced a full scrolling surface, explicit Cancel/Use position and coordinate angle editors. A swipe over a readonly result opened the IME. Iteration 17 added native input/readonly drag handling; further Find Body gesture and Back retests remain required.
- Iteration 14 opened the bundled 44-page PDF through Android PdfRenderer, navigated from page 1 to page 2, visibly scrolled page 2, and returned to Tools with Android Back. PID 9987 was unchanged. Rendered pages and scroll positions were inspected (`slice14-pdf*.png`). This verifies bundled PDF viewing on this host, not generated PDF content, zoom, rotation or all pages.
- Iteration 17's clock sheet shows local BST and UTC separately, live RMC UTC, comparison age and system-minus-GNSS latency; final note and copy control are reachable. Device `date -u` agreed with the UTC display. Changing device timezone to America/New_York changed local display to EDT and retained UTC/GNSS; Europe/London was restored. Rotating the open sheet into portrait retained usable content; Android Back returned to Tools. PID 10593 was unchanged. Evidence: `slice17-clock*.png`. Mark/release/copy and stale/empty GNSS cases remain pending.
- Android adapter test uses independent POSIX epochs in UTC, London, New York and Auckland, fractional NMEA ZDA parsing, saved-time parsing, millisecond arithmetic, invalid dates and refusal of missing/ambiguous London DST wall times. All passed (`android-utc-boundary.log`). These are adapter checks; physical observation DST round trips remain pending.
- Shared numerical suite rebuilt after the worker adapters: 207 passed, six opt-in desktop GUI cases skipped (`shared-tests-after-workers.log`). The earlier six desktop GUI passes remain recorded; affected GUI checks will be repeated after changes stabilize.
- Lunar search now has an explicit action, a cancellable worker and complete-input cache to avoid search on every keystroke; file operations have an app-accessible browser; PDF writing has cancellation checkpoints and atomic commit. These new paths compile but are not yet physically accepted.

Exact installed/unstripped SHA256 pairs:

| Iteration | Installed | Unstripped |
|---|---|---|
| 14 | `8e9c4eab22bc5d2ba3c7f2171a6942f6b80655918d114e545c9fe95f56bda09d` | `f5dba2a2f901c7a5da08d397d50643c7062a485628b0343a0e7c025f82921325` |
| 17 | `421184ab1cc798354dd07480d953e160d7dafde37067859516d61a3429ca3fc6` | `ad57520f864b1519b8f082311bde5d62c8d9c3612197fedcd6f32da98cfc6fe1` |
| 18 | `3898bb8a110a9f0ff05ffbad0613831abf6a8cc82b8c552f1173c4ddbbf2f2c6` | `2d3cdd2a6ffad47ab5bbc4d12ec48e26b4a9dcc8719c17032fd91d29be343b01` |

The first angle-button tap in iteration 17 focused the parent field instead of opening the editor; iteration 18 is retesting the child touch filter. No pass is claimed yet. Device orientation is currently portrait as part of testing; original locked landscape settings are retained for restoration.

## Sun reference on iteration 18

Physical entry: Sun lower limb, UTC 2025-07-20 12:47:00.000, Hs 30 degrees, eye height 3.5 m, 10 C, 1010 hPa, index error +1.5 arcmin, DR 43.2366916667 N / -77.533415 E, analytical ephemeris. Find Body displayed Ho 30°09.4265′, Hc 30°10.9099′, Zn 89°23.8906′, intercept 1.483434 NM away. The existing independent Nautical Almanac/reference case in `test/altitude_tests.cpp` uses Ho 30.1571°, Hc 30.18183165°, Zn 89.39818° and approximately 1.4839 NM away; results are within its 0.1 arcminute tolerance. Evidence `slice18-reference-reduction.png`.

Use position followed by Save stored a third disposable sight. Independent XML inspection confirmed date/time, milliseconds, Hs, correction inputs, manual DR and DRBoatPosition=0 (`slice18-reference-saved.xml`). Settled card update selected the saved sight. PID 12364 remained unchanged. Cold reopen and chart geometry remain pending. The angle sheet opens and Apply commits 30 degrees; its portrait vertical position and action caption still need the queued iteration-19 corrections. Month chooser selects July 2025, but clipped long month names require correction.

Report swipes moved through correction steps, then a further swipe unexpectedly returned to the report start. Final-line scrolling remains a failure under investigation. Find Body independently showed the corrected altitude; that does not establish report scrolling acceptance.

## Iterations 19–21 (in progress)

Iteration 19 PID 14029: cold workspace visible with three persisted observations; reference Sun reopened at Hs 30. Repeated calculation-report swipes reached the final Ho line, another swipe remained at the end. Evidence `slice19-report-final.png` and `slice19-report-end-stable.png`. Unchanged Save after cold reopen preserved every XML attribute of all three records, independently compared (`slice19-reference-reopened-save.xml`). Specific report movement and three-record round-trip checks pass. All observation variants/uncertainties are not yet accepted.

Iteration 20 PID 14644: Show selected sight on chart jumped near manual DR; Sun uncertainty band and central line visible with the workspace hidden, chart zoom, pan and rotation. Screenshots `slice20-sun-chart*.png`. US East Coast context and independently inspected host routepoint coordinates agree; host has retained routes in the UK and near New England. Navigation objects are SQLite `navobj.db` in this 5.14 host, not `navobj.xml`. No host objects were altered in this check. Overlay/reference geometry needs more precise pixel/location checks and coexistence/lifecycle regressions before family acceptance.

Iteration 20 almanac constructor opened responsively but portrait labels were invisible and date inputs clipped (`slice20-almanac-entry.png`). This is a failed layout check. Horizontal label/control rows collapsed labels to zero width; iteration 21 stacks those form rows. Native date control and Generate PDF caption corrections need physical verification. Coastal editor opened, but full calculation workflows remain pending; focused angle adapters and unchanged input precision corrections are added in iteration 21.

The new packaging helper created a development manual-import archive with exactly one root metadata.xml, expected API 1.18/Android arm64 identity, required VSOP87d asset and exact planned tester URL. SHA256 `6645139b6a56c1507492fd63b45313265a62a009a1f8c8197dad8915d3957cba`, `/tmp/celnav-android-20260927/import-development20/`. This is an uncommitted development build based on eb5fd64, not a release candidate or a published/downloadable artifact. Plugin Manager installation remains pending.

CI configuration retains all 19 original target jobs, defaults publication off, retains packages/provenance and includes an approval before optional reviewed publication. Fresh pinned Android builds and actual CircleCI execution remain pending.

| Iteration | Installed SHA256 | Unstripped SHA256 |
|---|---|---|
| 19 | `51f133e7a356d1c59997e9e5bbd69f27c68f15926063d1f779108f539a374879` | `cb94a708751028de42d14106f0f5c93307cd5c82a3e92cfb1a5fa7f3e455ace3` |
| 20 | `884bb9d0af665204450fb928e35047cd54c3fa480af66231c34c630cb38b168f` | `9e84aa022661f6b64d2a8cd33be7641ff8fc7caca3e209c6317a5afd7d50499c` |
| 21 | `7135ba27424a1bfc7f299f5e35f91ee91881b19fc7446f9c49247412f1719b44` | `c962cfa68a72c8810349972f1fadb4c11934edc7149a52cb21fb11fc6fa363b7` |


## Iterations 22–24: dates, modal transactions and generated PDF

Iteration 22 PID 15610: date inputs replaced by 74-pixel visible buttons (48 dp is 72 pixels here). Calendar sheet rotated to landscape with header actions and day rows visible. Applying 2026-10-04 updated the parent form. Selecting October 1 and pressing Android Back incorrectly committed October 1; a second isolated test committed October 30. This was a failed Cancel test, retained as `slice22-date-selected-before-cancel.png` and `slice22-date-back-actual.png`.

Disassembly of the exact linked wxQt implementation established that ShowModal maps every nonzero QDialog result to wxID_OK; EndModal stores the intended wx return code but passes that nonzero code to Qt. The Android ModalResult adapter reads GetReturnCode, while desktop calls remain unchanged. Related waypoint, horizon and saved-solution naming transactions use the adapter too; their physical regressions remain pending.

Iteration 23 PID 16289: from a confirmed visible workspace, opened the To date (2026-10-11), selected October 1, pressed Back; parent retained 2026-10-11 (`slice23-date-before-back.png`, `slice23-date-back-confirmed.png`). Specific date Back cancellation passes. An earlier automated attempt tapped before cold-start toolbar readiness and returned to the chart: `slice23-date-back-retest.png` is not acceptance evidence. Cold-launch tests must establish composition before tapping.

Iteration 22 generated a one-day 27 September 2026 A4 voyage almanac through the real Generate PDF action, then opened it with Android PdfRenderer. Independently pulled file: `slice22-voyage-almanac.pdf`, 148953 bytes, 28 pages, SHA256 `adac3e2ad1054efe359f1894aa0c27a55a9bbb5b15b97f14146a94b2888b966d`. Cover/version/date span, sources page and hourly Sun/Moon tables inspected with Poppler independently of the viewer. Sources explicitly report analytical bodies and 346 dated offline DUT1 evaluations. No optional kernel was installed in this check. Last-page entry failed: entering 28 caused per-digit rendering/focus loss and showed page 3. Iteration 24 adds a visible Go to page action and rotation-aware Fit width; retest pending. No full almanac-family pass is claimed.

Shared numerical/regression rerun after iteration 21: 207 passed, six opt-in desktop UI cases skipped (`shared-tests-after21.xml`/`.log`). Separate earlier GUI passes remain documented above.

Fresh CI-style arm64 plugin build completed using a new extraction, checked vector headers and matching existing pinned core binary. Fresh armhf core and plugin build completed using its own source clone and freshly extracted dependencies. Initial armhf sandbox DNS failure retained; network-enabled rerun succeeded. These are local CI-script checks, not remote CircleCI validation or armhf device acceptance. Both archive packages include root metadata; source is an uncommitted development state based on eb5fd64, not a release candidate.

| Local development archive | SHA256 |
|---|---|
| arm64, fresh CI script (before iteration 22) | `ee4281b45bcc5710f88b923e90c6f7ceb8bf23a67c3008f669f78b2e2bd036c3` |
| armhf, fresh core/plugin (through iteration 22) | `45836180abb08569742d8e4b873f7432b5d0e8a5d4a60d64cceec95d8f5e4afd` |

| Iteration | Installed SHA256 | Unstripped SHA256 |
|---|---|---|
| 22 | `2324646b5bc6815733f0ea08197463226046f6c4a75484c8ec5b1f638437e229` | `71231b34ce87e325af53aeb7afa4a5973af6321736622e4b92270924b9dc2d00` |
| 23 | `7908af0e8df85f623fadb53ccfe849c574ed2b65394b89143fd52b715679d19d` | `b6937809dbc20f08c0fa36f9fd4b060d7930260ef2aaa63c9c48c94702d372a7` |

Iteration 23 also opened a new Horizon Event and cancelled with Back. Returned to Observe; PID 16289 unchanged. Independently inspected saved XML contained the same three records and every attribute matched iteration 19 (`slice23-horizon-cancel.xml`). This verifies empty/new horizon cancellation, not horizon computation acceptance.

## Iteration 24 import failures and isolated host investigation

Iteration 24 PDF Go action accepted multi-digit page 44, disabled Next on the
last page, and retained page 44 across Fit width landscape rotation. PID 16779
was continuous. Evidence `slice24-pdf-page44.png`,
`slice24-pdf44-landscape.png`. This is a focused PDF navigation check.

At 11:33:48 Plugin Manager's native Select tarball file chooser timed out on
folder navigation after prior Qt text input, before selecting an archive.
Captured ANR main thread was blocked in Qt finishComposingText. Complete ANR
and logcat evidence retained privately in the audit artifact directory.

A fresh host launch PID 17546 accepted chooser scrolling and selected
`celnav-development24.tar.gz` at 11:41:37. Assets, install records and library
were extracted, but the process segfaulted in libgorp wxObject::UnRef called
by PlugInData destruction during ReloadPluginPanels. The retained plugin icon
is created through virtual GetSubBitmap; with private static wxQt, its ref-data
can contain a destructor in the subsequently unloaded plugin. The original
stack/build ID and exact binaries are retained. This is a diagnosed lifetime
candidate, not a successful import or proof that every Android host is broken.
Cold restart loaded the extracted library SHA256
`cd08bdf3674b69de3458de8774b84a6baa658a0c362f637716ff777e094433d7`.

User subsequently authorized a narrow core fix, physical verification, fork
and upstream PR. Isolated master branch `fix/android-tarball-import` starts at
`1bf728e17`; a separate 5.14 deployment worktree applies the same functional
changes to the retained host base. Changes keep Qt dispatch active during the
legacy JNI wait, await actual SAF filenames, and retain host-created Android
bitmap data. Desktop bitmap handling is unchanged. Tests remain in progress.

Exact original host APK backup: `/tmp/celnav-android-20260927/host-original-5.14.apk`,
SHA256 `e53ccadd201ecb2df591d3bb650e91ba16180ebb492f219efcc33b46e470967b`.
Patched APK SHA256 `7a00925a2168fa69ba353890d1f4d7ace571bd35f51080442d88565eed70ff77`;
embedded libgorp SHA256 `a597df7b6685ee3468eed9caf6e13dbc720d53d44b05170278171486da828e30`.
Both APKs have certificate SHA256
`e6d6796c1bd4efd101c4b16845254328dceb491192f40cfc8c658796ad8f0b10`.
Independent ZIP comparison found only libgorp and three signature files changed;
DEX and all other native libraries/assets were identical. In-place install
succeeded; no uninstall or app-data clearance was performed.

Iteration 25 direct-deployed installed library SHA256
`e91c69b9eb789f73519ae93fd159ed98bdce67f05ee42b5af3d47ead1d46a6ce`,
unstripped `f10aa60172808985a904ba0372345f06131c96ff7b04a5a871dcf62900a0a7d9`.
One original-host cold launch was black until rotation; PID 18681 was alive
and initialization logged completion. This is a failed cold-composition check,
not a pass inferred from visibility flags. Rotation restored the chart, and
Celestial opened with all three stored observations. Coastal combo Back
closed its popup and retained the editor. Full Coastal acceptance remains pending.

Development iteration 25 manual archive with regenerated 2.8.13 manuals:
SHA256 `f0ad71be5213d604c5f919a01cec6e940a66d7e4c748b06bc551ef7ebe5de155`.
This uncommitted development archive is not a published release candidate.

## Patched-host import and manual artifact verification, 12:06–12:11 BST

Plugin Manager selected development25 through its native chooser after multiple
folder taps and genuine list swipes. It displayed Installation complete at
12:06; PID 19670 remained continuous. Actual installed library SHA256
`765e51f7c3a8546565b4e5da7256a988d3601a4c1b50f22efe3366b328e894e3`
matched the library extracted independently from the archive. Stock greyscale
Celestial icon was visible. Cold restart PID 20288 showed the chart and reopened
all three observation cards with their original angles and fractional timestamps.
Screenshots `host-import25-result.png`, `host-import25-cold.png`,
`host-import25-workspace.png`; full logs `host-import25-logcat.log` distinguish
prior original-host crashes from this successful replacement.

The default app-owned directory does not provide a tester workflow for public
Downloads under scoped storage. An additional Android-only change to
GetImportInitDir selects androidGetDownloadDirectory, invoking the existing
system document picker and its cache-copy callback. This path is still under test.
Second patched host APK `/tmp/celnav-android-20260927/host-import-downloads.apk`,
SHA256 `d4a96b2ee971c22599d915531319b9e985bc0cf5276cb761fa4a65cdfde6c332`;
libgorp `1e6e2524f46be0321313a5fcd49a0257f441d8279d93136a30e7ca638a40786f`.
Independent ZIP comparison again changed only libgorp and signature entries.
In-place installation succeeded; all user data was retained.

Regenerated 2.8.13 HTML/PDF/DOCX preserve the complete 4435de5 source text,
independently compared after normalizing only version and revision date.
Bundled runtime LibreOffice rendered the DOCX/PDF to 42 pages. All 42 pages were
inspected at full resolution, in addition to montages: no missing content,
overlapping text or clipped tables. Existing diagram/style content is retained.
Rendering evidence `/tmp/celnav-android-20260927/manual13-render/`; tablet opening
of the new 42-page PDF and Android-specific quick guide remain pending.

At 12:12 BST the public Downloads system picker selected `celnav-tester25.tar.gz`.
Its actual app-cache copy SHA256 was
`f0ad71be5213d604c5f919a01cec6e940a66d7e4c748b06bc551ef7ebe5de155`.
Import completed; PID 20808 remained continuous and installed library matched
`765e51f7c3a8546565b4e5da7256a988d3601a4c1b50f22efe3366b328e894e3`.
Repeated picker + Back returned to Plugin Manager without an import or restart.
Evidence `host-downloads-picker.png`, `host-downloads-import-result.png`,
`host-downloads-back-cancel.png`, full process log `host-downloads-import-process.log`.

Current upstream master plus the three-file fix built successfully with freshly
extracted support archive SHA256 c4110c532e9a0bcf071bbd10fe6f7627d7e91380c803c52ac0e89ce5f993db9b.
Full configure/build logs retained. The fix was committed as
`9484b97c3741db591b3dd09fe02711acf6503248`, pushed only to a new branch
`fix/android-tarball-import` in the existing `pob220/OpenCPN-weather-routing`
fork and opened against master as https://github.com/OpenCPN/OpenCPN/pull/5457.
GitHub's fork command reused and renamed that existing fork; its original name
was immediately restored and verified before pushing. Existing branch contents
were preserved. PR check list was empty at 12:25; this is not remote CI approval.

Final JNI polling cleanup avoids the legacy helper's acquired-string lifetime.
Final patched APK SHA256
`019ec4a710e8619444fae86bb488942249c3f4e285507e1abeda248eafda0e45`;
embedded libgorp `e461169130ac69db83919cc18ce552569179e9f68dbd3a776fe1e8422423d6fb`.
ZIP comparison again changed only libgorp and signature entries. In-place install
succeeded. Cold PID 22095 showed chart and stock Celestial toolbar icon in portrait.
Updated manual showed 2.8.13 and revision 27 September 2026; entered page 42,
Go rendered its actual content, Next disabled. Subsequent swipe screenshot was
the Tools page: Go had already requested keyboard dismissal before Back. The
immediate Go screenshot captured the keyboard animation; it cannot establish
keyboard state at the later Back. That swipe is not a final-content pass.
Remaining disable/re-enable attempts did not change the host checkbox state;
therefore no disable/re-enable pass is claimed. All other plugins remain enabled.

Archive packaging now writes into sibling staging and atomically replaces only
a verified complete archive. Executed failures for traversal, device entries and
missing assets left an existing archive unchanged and removed staging; valid
replacement contained exactly one root metadata.xml. These are packaging checks,
not public-hosting verification.

Controlled keyboard Back checks, 12:27–12:29: native navigation-bar Back and
separately injected hardware Back while editing the page number each dismissed
only the keyboard and retained the PDF viewer, PID 22095. Go to page 42 explicitly
hides the keyboard; after settling the complete final references and footer were
visible (`back-repro-after-go.png`). No nested Back defect was reproduced in
these controlled checks. A speculative extra delayed focus-clear change was
removed without deployment because it could interfere with a subsequent edit.
Iteration26 diagnostic binaries are retained but were never installed or counted
as accepted. Existing iteration25 remains installed.

## Repeat import exposed plugin teardown failure, 12:37–12:42 BST

Final patched host PID 22095 accepted the public Downloads archive and reached
Installation complete, then SIGSEGV at 12:37:08.215. This run followed workspace
and PDF use. Exact retained slice25 symbols resolve PC 0x3e3680 to
CelestialNavigationDialog::UpdateTimeIntegrityPanel, line 621; Qt QTimer frames
follow. Full logs final-host-import-logcat.log show PID death and background
GPS-service auto-restart PID 23529. The unchanged installed hash and success
dialog do not make this an import pass. Earlier cold-state host-fix passes are
limited to those executed states.

Iteration27 explicitly destroys the Android clock timer before wx labels and
plugin teardown. Retained Android eclipse/coastal children are destroyed
synchronously during workspace teardown; desktop Destroy behavior is retained.
Unstripped binary slice27-unstripped.so and packaged slice27-installed.so retained.
Archive SHA256 18b1630e79613b16bdeef5dcddccb29073419e0bed8396d918bfeffa0b6de714;
packaged library 390bc252400319adcc67fede851cfbf628d83f748a9c9c4ca624a25e51bfbc75.
Reimport after opening workspace/PDF remains pending at this entry.

Shared suite after25: 207 passed, six opt-in UI cases skipped, 213 total. First
GUI attempt lacked sandbox display access. A combined GUI run violated the
tests' explicit run-alone requirement and failed wx initialization/state; it is
retained as invalid execution, not a desktop regression. Each of the six cases
was then run in its own process with graphical-session access and passed. Logs
desktop-after25-<suite>.log and XML retained.

At 12:42, first development27 import completed in recovered PID 23529, still
continuous from before this import. Actual app-owned library matched
390bc252400319adcc67fede851cfbf628d83f748a9c9c4ca624a25e51bfbc75.
Screenshot slice27-first-import.png. This is cold-state installation only;
workspace/PDF replacement regression follows.

At 12:45–12:46, development27 workspace/PDF/keyboard were genuinely opened,
keyboard Back retained viewer, Close returned to Tools, Chart hid workspace.
Repeated Plugin Manager public Downloads replacement completed with PID 23529
continuous. Evidence slice27-pdf-before-import, slice27-pdf-keyboard-back,
slice27-warm-picker, slice27-warm-import-result and full slice27-import-logcat.log.
No subsequent crash/ANR appears in the retained log through this result; historical
12:37 failure remains present and separately dated. This passes the previously
failing clock/PDF replacement sequence for development27. Final committed
release, retained coastal/eclipse windows and cold restart are still separate checks.

## Coastal vertical reference on development27, 12:48–12:51 BST

Bob revision1 PDF fixture (existing test/coastal_navigation_tests.cpp), target
50.6616666667 N, -1.5916666667 E, visible waterline-to-top Hs 0.76 degrees,
IE -0.15 arcmin, target height24m, water level0m, eye3m, bearing75.22 true.
Actual tablet output0.974NM, corrected45.7500arcmin, position50°39.4516′N,
001°36.9841′W agrees with the independent published rounded values within
0.0006NM and0.000002degrees. Inputs, genuine final-control scrolling, result
and close/reopen retained result/screens are slice27-coastal-*.png. PID23529
continuous. Initial automation passed -- as text and entered only a minus sign;
corrected actual values were independently inspected before Calculate.
Chart geometry is calculated, but physical chart placement remains pending.
Current form repeats units on separate rows and needs too many swipes. Iteration29
combines units with labels on Android only and adds visible Show plotted range/fix
on chart actions using stored calculated geometry centres. Desktop forms unchanged.
Iteration28 additionally stops the eclipse Qt verification timer on destruction;
not separately installed. Iteration29 build/testing follows.

Development29 import at12:54–12:55 included a retained coastal window from27.
PID23529 remained continuous; installed library c3dcb0a8940dfe7bcd70b107da91a9fb42a8a0b8a5127c13d39f82913d2397ee matched independently extracted archive.
Archive15996faa5d4cb084514f2c08b8f8d7452fdd29924b48e1b87d25dbee1b0963b4;
unstripped29 e90c80ba4962fcaa38f01bcc42e9ddeccba9e95ee45e25029c94dc9375844ed4.
The compact form combines unit rows into labels without changing desktop forms.
Keyboard resizing can scroll focused entries into view, so subsequent automated
coordinates must be reacquired. An attempted bearing entry into a disabled field
followed by Back correctly closed the sheet; it was not a keyboard-only Back test.
Reopened entries were retained, then real enabled bearing field and inputs checked.

At13:00 the repeated waterline reference again displayed0.974NM and the same
independent position. Show plotted range on chart hid both tool and workspace,
centred target, and displayed a blue range circle plus bearing/range position.
Screenshot slice29-coastal-chart.png: circle radius about473px, chart0.2NM scale
about98px, agrees with0.974NM within displayed scale rounding. Fix marker offsets
about458px west/121px south from target centre, consistent with reverse bearing
255.22° and published fix. PID23529 continuous. Other plugins remain enabled.
Pan/zoom/rotation follows; detailed local charts are not available at this target,
so the world basemap is coarse. Desktop CoastalUi alone passed after29; Android
UTC/angle boundary suite passed.

At13:01 chart pan, zoom-out and physical tablet rotation to landscape retained
the coastal circle/marker with correct chart scale and reverse bearing.
Landscape screenshot slice29-coastal-chart-landscape.png:1NM bar about244px;
range-circle radius about236px, consistent with0.974NM at rounded screen scale.
PID23529 remained continuous. This is device orientation rotation, not proof
of a changed chart-heading rotation. xGRIB/xWeatherRouting stayed enabled.

An implementation checkpoint is being committed locally before continuing the
remaining physical matrix. This is not release acceptance: most lunar, planner,
fix, calibration, eclipse/optional-data and invalid/cancellation workflows remain
pending. All retained development packages so far were built from an uncommitted
work tree above design commit eb5fd64; a final committed-source build is required.
