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

Implementation checkpoint cf161ce06934b67f564c8f94a4d5aa64ed94a821 was committed
locally and pushed to the new android/celnav-2.8.13 branch without changing any
existing remote branch or publishing a release. Source is reviewable at
https://github.com/pob220/celestial_navigation_pi/tree/cf161ce06934b67f564c8f94a4d5aa64ed94a821.
At13:06–13:08 GitHub statuses had no CircleCI entries. GitHub Actions run
36317831149 passed only the existing dummy-check/echo ok; it is explicitly not
a build validation. Public CircleCI project API returned404; no authenticated
CircleCI tool/token or enabled browser is available (CUA inventory empty).
User was asked asynchronously to trigger the existing project with deployment
false or make CI access available. Physical acceptance continues independently.

## Horizontal coastal reference and clearing, 13:10–13:21 BST

Actual UI inputs from Bob revision 1, existing independent PDF-linked fixture:
left 43.9166666666667/-69.2616666666667, centre 43.965/-69.0733333333333,
right 43.7833333333333/-68.855, angles 70/107 degrees, index error -0.15
arcmin, uncertainty 60 arcmin, initial DR 43.8283333333333/-69.0766666666667,
observer motion disabled. Result 43°51.3588′ N, 069°05.3779′ W, residuals
+0.0000′/-0.0000′, formal 1-sigma 0.149 NM, condition 2.0 matches the
reference within 0.000002 degrees and displayed rounding.

Evidence slice29-horizontal-result.png, horizontal-reopen.png and
current-workspace.png. The initially named horizontal-chart screenshot caught
the preceding vertical chart before the asynchronous jump settled; it is not
horizontal evidence. The subsequent current-workspace screenshot shows Maine
basemap, both magenta/orange horizontal loci and the red fix marker. Chart
centres the solved position; editor close/reopen retains the result.

New observation > No retained result. Clear chart plots disabled the chart
action but retained the result/inputs; actual chart showed no coastal layers
(slice29-coastal-cleared-plots/chart). Confirmed New reset both pages' entries
and results; empty HSA solve reported the missing left latitude/longitude and
created no plot (reset/empty-invalid screenshots). PID 23529 remained continuous.
Dashboard was restored to hidden using its visible toolbar button.

Back on the stock Yes/No New confirmation was ignored (new-back screenshot);
No remained usable. Iteration 30 replaces Android coastal/file overwrite
confirmations with owned, cancellable sheets and explicit destructive labels,
while preserving desktop prompts. Physical retest is pending. Iteration 30 also
adds the visible Android quick guide and pins SDK tools SHA256
2d2d50857e4eb553af5a6dc3ad507a17adf43d115264b1afc116f95c92e5e258,
verified against Google's repository2-3.xml SHA1 for cmdline-tools;12.0 Linux.
Publication metadata now explicitly sets the approved Cloudsmith URL even when
the retained Android XML already contains a development URL. Local verification
covered that and legacy placeholders without publishing anything.

User is creating the CircleCI project through the website. Its setup preview
shows default-branch configuration with deployment true; instructions identify
android/celnav-2.8.13 configuration/checkout and deployment false for validation.
GitHub CLI already has authenticated pob220 repository/workflow access; no new
device login was initiated and no credentials were requested.

## Development 31 import and prompt regression, 13:25–13:30 BST

Archive f378a0eaab6d648e30a21120f027be2af8c731b4badd334ff2bb20dd36e6912e;
installed library 4ff8de8ea524e029f9a6f88351ee34cbaf72c6b5d80d8c79e7f60f6277954ec1;
unstripped fae29c49e2c211a85c33f169df440674cc51fe5dbd6e119fd4790ebca6b2f4b5.
Built above cf161ce with uncommitted iteration 31 changes. Exact binaries retained.
Public Downloads Plugin Manager import succeeded while a coastal window had
been retained in 29; installed run-as hash matched and PID 23529 stayed continuous.
The three saved sight cards reopened with fractional times/measurements intact.

31 extends the Android owned-sheet adapter to all plugin message boxes and the
existing custom-label message dialogs. Desktop names preprocess to the original
wx symbols/classes. Stock informational warnings also ignored Back in 29;
31 replaces these as well as confirmations. Coastal New > Back now returns to
the coastal editor without resetting it or exiting the host (new-prompt/back).
Android quick guide is visibly available in Tools, genuinely swipes to its final
paragraph, and Back returns to Tools (quick-guide-top/bottom). Other prompts,
unsaved custom labels, rotation and nonempty refusal need further physical checks.
Desktop build passed; CoastalUiSmoke, HorizonEventUi and LunarUiSmoke each passed
alone. The first HorizonEventUiSmoke filter matched zero tests and is not a pass;
it was corrected to HorizonEventUi and that real test passed.

CircleCI project creation completed through user browser. Pipeline #1 at
12:28:25 UTC targeted exact cf161ce and deployment false but stopped before any
workflow: inherited uncertified cloudsmith orb blocked by organization policy.
31 removes the unused cloudsmith/jq orbs and obsolete deployment command, fixes
legacy parameters on retain-code invocations, and retains all 19 build jobs and
publication approval gate. Existing official CircleCI CLI reports configuration
valid (circleci-config-after31.log); this is not a remote build pass. No stored
CLI token or CIRCLE_TOKEN/CIRCLECI_TOKEN is available.

## Coastal sea-horizon references and fresh local builds, 13:39–13:52 BST

Installed development 31, library 4ff8de8ea524e029f9a6f88351ee34cbaf72c6b5d80d8c79e7f60f6277954ec1,
PID 23529. Target 50.6616666666667 N, -1.5916666666667 E;
sea horizon to top, height 24 m, water level 0 m, eye 3 m, index error -0.15′.
Observed 2.9′ (0.0483333333333333 degrees) produced 9.676 NM and corrected
+0°00.0051′. With checked true bearing 65.22°, position 50°35.6333′ N,
001°49.3301′ W matches the independent Bob coastal fixture within 0.000002
degrees; range tolerance 0.0006 NM. The genuinely enabled bearing field was
verified before entry; earlier taps on the reference combo or disabled field
were automation errors and are not evidence of a successful bearing entry.
The actual chart shows the blue range circle and southwest fix marker;
2 NM scale is consistent with the 9.676 NM radius. New > Android Back on this
nonempty observation preserved its entered bearing, range and result.

Observed 2.8′ (0.0466666666666667 degrees), with all other inputs retained,
produced 9.797 NM and **-0°00.0949′**, agreeing with the independent 9.797358 NM
and -0.094945′ reference within displayed rounding. This verifies that a
negative corrected sea-horizon angle is preserved rather than clamped to zero.
Evidence: slice31-coastal-sea-bearing-entered/result, sea-chart,
nonempty-new-prompt/back and negative-result.png.

The remaining desktop GUI suites FixUi, FindBodyUi and AlmanacUi each passed
alone after 31; all six requested desktop UI suites now have actual executed
passes (logs/XML desktop-*-after31). No broader shared numerical rerun was
needed for Android-only adapters.

Fresh support-archive extraction builds passed for exact committed source
e7e0cf7ac6e6ddf30eaf5ec6cb202ba9957ad199, both ARM64 and ARMHF. Archive SHA256:
ARM64 93e41a8b2048e68c844171fea6e18657e9e87bba9b445a4ccf81c45343052c8a;
ARMHF f62836b9ef7c70a8a02af9499e5f17481e7846e6da8a416201cdce4881458e9c.
Artifacts/android-*/package include root metadata and exact source provenance.
These archives have planned, unpublished development URLs; neither is a public
download or final tablet acceptance package. The local ARMHF build has not
been device-tested. Existing verified core libraries were used; the support
archives were freshly extracted, and required support headers restored by
the reproducible checksum/provenance step. Full logs fresh-*-e7e0cf7.log retained.

The user's documentation worktree remains at 4435de5088666933fb46ee9663a36fbb42baea87;
its new untracked validation/stelian-study directory is untouched. Its study
identifies baseline analytical DUT1/light-deflection limitations. These are
not Android regressions and must be respected in reference tolerances; shared
numerical changes are outside this release's documentation-only desktop scope.

Iteration 32 addresses the genuine scoped-storage gap in the app folder
browser: visible Choose from device uses ACTION_OPEN_DOCUMENT, a Qt-owned
asynchronous result receiver, and a cancellable 256 KiB worker copy. No host
Java chooser copy executes. Staging files have leases; eclipse verification
retains its lease until verification/installation finishes. Android final
installation also uses a cancellable worker and atomic commit. Abandoned
plugin-owned staging files are removed at plugin initialization. Desktop file
dialogs and installation paths remain unchanged. Build and physical acceptance
of this adapter are tracked separately; no physical pass claimed yet.

## Scoped-storage imports, worker crash and eclipse pages, 13:53–14:23 BST

The first iteration-32 import archive accidentally used the older CPack filename
with the repeated target suffix. Its installed library was 95ee6d956cdf64deb85e38ac578d8d8b780d82370f1457b188ded6c3286c3728,
not the newly built adapter. Missing Choose from device exposed this packaging
mistake. No new-adapter pass is attributed to that archive. The corrected 32b
archive used the plain incremental CPack filename and actual installed library
c35158cc02422e1b4f46f0abf7e76b34d1730faa97c049bd0fc72b9207dfeaa0.
Its matching unstripped file is slice32-unstripped.so (SHA256
c9b565f2479abad6d51e542d807df94fe6e50e68d5ad040b7f7aa2746bff9f71).

32b copied DE440s successfully but crashed at 13:58:24.983 in
EclipseVerificationWorker::Start. Symbolication traced wxMutex::Lock to the
wxThread constructor: the statically linked wxQt thread module had an
uninitialized global mutex in this plugin. PID 23529 died. This was a plugin
adapter defect, not a host import restriction. Complete logcat and Android
exit information are retained as slice32b-import-crash-logcat.log and
slice32b-import-exit-info.txt. Explicit app recovery created PID 29230.

33 uses std::thread/std::mutex for the two Android background verification and
lunar-session workers. Desktop aliases retain wxThread and wxCriticalSection;
shared numerical algorithms are unchanged. Android and desktop builds passed,
and eight real desktop worker regression tests passed, including verification
of the official DE440s file. The rebuilt test target was executed under Xvfb;
desktop-workers-after33.log/xml retain the results.

33 installed through Plugin Manager at 14:02 and matched library
243eaa0085130f9792d149555362a5155295d54600654c2f6617c76f2d0e4324.
DE440s then imported through Choose from device, verified, and independently
matched official SHA256 c1c7feeab882263fc493a9d5a5b2ddd71b54826cdf65d8d17a76126b260a49f2.
The 2027 search found annular magnitude 0.9281 and total magnitude 1.0790, but
the inherited table clipped dates and coordinates, so it was not accepted as
a readable result. The optional-data custom modal action was also lost by wxQt
ShowModal; 34 uses the explicit wx return code on Android.

The design document was extended before implementing three Android eclipse
pages: Search & chart, Local circumstances, Data. 34 built on Android and
desktop and installed through Plugin Manager at 14:16; actual library SHA256
e5a0862a78dea6645933eda831b16c1ee484f67df66d9483192e252079a04d14.
Visible 2027 cards showed complete UT1 dates and coordinates. Font encoding
and selection synchronization defects were found: the highlighted August card
could leave the model pointing at February. 35 makes the visible native card
index authoritative for Android and fixes the UTF-8 labels. Neither defect is
marked passed until the rebuilt UI is exercised.

On 34, the repaired optional import actions opened the correct native picker.
Orientation and LOLA were imported from Downloads and verified/installed.
Independent run-as hashes of the actual installed files matched:

- moon_pa_de440_200625.bpc: 60cd55aa401ea2ea97360636f567554bfe4e37bb829f901b4460a455dfaf783f
- lola64-pa.bin: f59edf8437442b05525345b3c29b65f0f31af8fc96420abf2dd18af3480f7ff4

The plugin-owned celestial-imports staging directory was empty afterwards.
PID 29230 remained continuous. The first large-copy cancellation attempt was
too late: the screenshot already showed verification and installation completed.
This is a successful verified import, **not** a cancellation pass. Effective
copy/verification cancellation, invalid imports, cold-restart persistence,
terrain-refined contacts and orientation/font regressions still require tests.

34 and 35 are development builds from e7e0cf7 plus uncommitted adapters.
Their provenance JSON explicitly records the dirty worktree and saved source
snapshots source-development34/35. Proposed archive URLs are unpublished;
neither tarball is a final acceptance package or an exact e7e0cf7 binary.

## Eclipse reference, cancellation and cold restart, 14:24–14:42 BST

Development 35 installed through Plugin Manager. Archive SHA256 is
efa76c3690d80acc2fbe07dc82c18739b358c12aef1b87b50bdb7152f07d7e74;
independently read installed library SHA256 is
eb17900c528d0890469e24fbd2ebb67bbbb1bba13b902971ceab05213b322781.
Matching unstripped binary slice35-unstripped.so has SHA256
d8e52464c5da43611d48a9d616690d1ce93f5fab9ced7850e3ce8ee1f277caa0.
These remain dirty development builds, with source-development35 snapshots.

The 2027 search returned two complete readable cards. Selecting August visibly
changed both the selected summary and Local circumstances calculation to the
August 2 total eclipse. At 25.505 degrees N, 33.18333333333333 degrees E,
standard totality was 382.44 seconds, within the existing independent NASA
fixture's 382.6 seconds ±1 second. The UI used its modeled ΔT of 76.06 seconds;
the NASA fixture uses 71.7 seconds. Thus absolute UT1 contacts/longitude are
not asserted to match a fixture with different ΔT. Standard contacts were
08:41:29.78, 10:03:19.93, maximum 10:06:31.37, 10:09:42.37 and 11:27:35.95 UT1.
Real LOLA refinement produced contacts 08:41:31.97, 10:03:18.28,
10:06:31.37, 10:09:43.86 and 11:27:35.83, duration 385.58 seconds.
Terrain-dependent execution is demonstrated; independent accuracy of those
terrain contacts remains pending. See slice35-local-nasa-unrefined.png and
slice35-local-nasa-lola.png. Rotation with the sheet open preserved selection,
inputs and results; a real landscape swipe exposed the final ΔT/terrain line
(slice35-eclipse-landscape-bottom.png). Other page/orientation cases remain.

Repeated Local calculation exposed normalization of editable coordinates into
rounded DMM text. Development 36 preserves the original Android entry text
and clarifies signed decimal versus DMM labels; desktop normalization is
unchanged. Android and desktop compilation passed. The corrected precision
behavior has not yet been installed or physically verified.

At 14:36, the 506 MiB LOLA import was cancelled during actual copying. The
before screenshot showed 6 MiB copied; the cancellation tap occurred 0.807
seconds after selection. The browser returned, the plugin-owned staging folder
was empty, and the independently read installed LOLA hash remained unchanged.
See slice35-fast-copy-before-cancel.png, slice35-fast-copy-after-cancel.png and
slice35-data-after-copy-cancel.png. Verification cancellation and remote-provider
interruption are separate pending cases.

Automation had accidentally activated native route creation and created one
disposable five-point route. This was not a plugin-output pass. Route creation
and chart-follow were turned off. With OpenCPN normally exited, the actual
navobj.db was backed up and only the identified test route, its five uniquely
referenced points and five links were removed. SQLite integrity passed, and
every original row across all eight tables exactly matched the original
baseline. Private evidence is host-object-cleanup35.json and
navobj-pre-cleanup35.db under /tmp/celnav-android-20260927; these user-data files
must not be uploaded. Cleaned database SHA256 is
abd5f9627eeaaa546d240cd29dfb56bfa92e4ac4ada2987bc9b6fcd46b932393.

PID 29230 exited normally (EXIT_SELF, status 0) at 14:31; explicit cold launch
at 14:32 created PID 31948. Saved observations retained fractional angle/time
values, and all three optional packs reverified successfully. Complete logs
collected at 14:33 showed no new fatal crash/ANR after the repaired worker was
installed. This is an explicit restart, not continuity across the two PIDs.

## First full remote CI results and repairs

The public CircleCI workflow 861ec2da-f3e8-42d6-bfc2-636c290c29dd ran exact
e7e0cf7ac6e6ddf30eaf5ec6cb202ba9957ad199: 16 of 19 platforms passed (ARM64,
macOS and all fourteen Linux targets). Both Windows jobs compiled/packaged but
failed artifact retention because python3 was absent. ARMHF built core but
failed plugin configuration: CMake 3.16 expected the obsolete NDK platforms
layout. CircleCI reports oss=true. Publication jobs did not run.

The repair selects an available Python 3 interpreter on Windows and pins
Kitware CMake 3.31.6 with upstream SHA256 for both Android jobs. Shell checks,
real Linux artifact retention, official CircleCI config validation and an empty
ARMHF configure using the pinned CMake passed locally. A complete new ARMHF
build and a new exact-source remote workflow are still required; these local
checks do not change the failed remote jobs into passes. Logs are retained
under /tmp/celnav-android-20260927/circleci-e7-* and
armhf-cmake31-configure36.log.
