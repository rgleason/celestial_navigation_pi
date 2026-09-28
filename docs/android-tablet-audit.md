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

## Committed-source packages and UI checks, 14:50–15:01 BST

The repairs were committed and pushed as
dcbb4eee78737565e1a846e8c74c0cf46c334529. Both ABIs built and packaged using
CMake 3.31.6 and a fresh verified support extraction (verified existing 5.14
core libraries were reused). Full configure/compiler logs and binaries are
retained in committed-dcbb4ee/ and fresh-*-dcbb4ee.log under the audit root.

| Target | Import archive SHA256 | Installed-library bytes SHA256 |
|---|---|---|
| ARM64 | fb324b75d38d78810fb54a33f52e6a03652f587c046757354e10b33eda5cd78d | e6fd7bc5f0e4b45eafe97dadacaa9a85ccfdb99e4dab8c8ab796f32995a9bc62 |
| ARMHF | 171aa9fbaa1acfea255cebb91a94d5396000ca024e756af85325287970340406 | 0b15172ad4516aec347c455ac6ec3604a419f14352e4572a6d9e52a991095c1b |

ARM64 installed through Plugin Manager at 14:54 and the actual run-as library
hash matched. PID 31948 remained unchanged. ARMHF was built only, not installed
or device-tested. Planned metadata URLs remain unpublished. Matching ARM64
unstripped SHA256 is 63227fa13bf2f9554195b2211e197ebe91ff7dd4abd2f142295282aaba1483ed.
Shared tests passed 207/213, with six opt-in UI cases skipped in that combined
run. All six were then executed separately under Xvfb and passed: coastal,
lunar, running fix, Find Body, horizon and almanac. Their separate logs/XML are
desktop-*-dcbb4ee; no skipped test is counted as a pass.

The 66-byte disposable invalid DE440s file was rejected with expected/found
sizes; independently read installed DE440s retained its official hash and
staging was empty. Native chooser Back returned to the file browser without
installation. On the committed build, empty Plot selection, reverse year span
2028–2027 and latitude 91 degrees produced clear errors. Real year edits need
ADB input keycombination 113 29; sequential keyevent 113 29 is not Ctrl+A and
left the original spin value. That first automation attempt is not an invalid
span pass. Likewise, section popups align their selected row with the field;
the first long-search navigation attempt never started a search and is not a
cancellation pass. Inspected popup rows corrected the automation.

Actual 2027 selection/local calculation retained decimal entries
25.5050001234 and 33.18333333333333 through repeated calculation and sheet
close/reopen. Standard duration remained 382.44 seconds. A real 1850–2100
search displayed its running progress; Android Back cancelled it and returned
to the sheet with the previous August selection/results intact. See
dcbb4ee-local-precision.png, dcbb4ee-precision-reopened.png and
dcbb4ee-long-search-{inputs,running-real,cancel-real}.png. PID remained 31948.
Post-install full logcat contained historical failures but no new fatal/ANR
after worker repair; measured PSS 461718 KiB, RSS 523632 KiB.

The user started the next full CI workflow, 7e8e57ea-7603-4ee3-8d7c-1234df0da591,
on exact dcbb4ee. All 19 jobs were running at 15:01; results are pending.
The existing CircleCI run-ci PR-label trigger can be driven through GitHub:
draft development PR pob220/celestial_navigation_pi#1 and label were created.
Its actual automatic trigger still requires verification on the next revision.

The Android chart-handoff design was refined before implementation: Plot selected
on chart centres on the event and hides the workspace after successful geometry
calculation. Android and desktop compilation passed for this small conditional
change (iteration 37); physical chart acceptance is pending. Desktop plotting
and shared geometry algorithms are unchanged.
# Acceptance continuation: native lists and CI portability

Automatic triggering is now verified: adding run-ci to draft PR #1 started
workflow b568cb91-b510-4d1e-991d-7670277d7b1f on exact source
6346ba96df409629b9ef6cd869e48859d08dfdec. Subsequent reviewed pushes can use
the same label trigger without a manual CircleCI website action.
The dcbb4ee workflow passed both Android ABIs, both Windows targets and all
14 Linux targets. macOS compiled and packaged but failed artifact retention:
its Bash 3.2 rejects an empty array expansion under nounset. The wrapper now
branches explicitly for the Windows py launcher, avoiding that expansion.
Its real Linux retention run passed; the next macOS run must verify the fix.

The installed dcbb4ee full 1850–2100 search completed on the tablet, with PID
31948 unchanged. Results moved from 1850 to 1851 on a swipe, but Qt also selected
the card at the initial press. This is a defect, not a touch acceptance pass.
The next adapter consumes that press and selects only on stationary release;
the native file list uses the same rule and defers folder refresh until after
the callback. Compilation passed; physical regression remains pending.

Standalone eclipse acceptance tests were configured and built from this source
and executed with the official DE440s, lunar PCK and LOLA pack paths. CTest
passed in 17.76 seconds. Existing independent fixtures cover 571 NASA catalog
events over 1850–2100, published path/local circumstances and independent DE440
states; optional LOLA checks cover validation and plausibility. They do not
establish independent terrain-contact accuracy. Logs: eclipse-independent-*.log
in the retained audit directory. No shared astronomy algorithm was changed.

## 15:18–15:24: committed touch and chart acceptance

Installed exact 7f68c9f4a66fe99ca2aeeb4935343d3a2e571f81 through the patched
host Plugin Manager. Archive SHA256
00f249ca79278a6e5652da099715ccb9718165818c48cf2d8d954f0a75fc005d; actual
installed library 5f66db31d2a25e19167721a8c26714eb87e00e3fb086cb4b8760581aa7ab81ce;
unstripped 72801d695cd6396b939a62f6b4ee1a9450741f9480e78c1b35f42c33252a3292.
Matching binaries/logs in committed-7f68c9f/arm64. Before replacement, independently
retained settings, Sights.xml and old library with backup-before38.json hashes.
Original assets are also retained in the previous exact package and original
profile backups; optional packs were not replaced. Proposed URL is unpublished.

The complete 1850–2100 search completed. Sending Home and resuming while the
progress sheet was still running preserved PID 31948 and continued the work.
A swipe starting over the second card moved content to 1851 without changing
the selected February 1850 event. The scrollbar reached the final September
2100 event; tapping that card selected precisely 2100-09-04 08:45:55.93 UT1
on Local. Evidence full-search-*38, search-resume38, cards-swipe38,
cards-last38 and last-selected-local38. The file-list adapter remains untested.

A 2027 search and August selection plotted path plus magnitude contours, centred
the chart and hid the workspace. At settled rendering, the path crosses Egypt
and the Red Sea; pan and zoom retained alignment with the basemap. Portrait to
landscape briefly exposed host resize composition before the settled landscape
chart filled the viewport and retained geometry. Reopen kept the August
selection. Clear plot removed all eclipse lines while retaining existing sight/
coastal geometry. xGRIB and xWeatherRouting stayed enabled. Evidence eclipse-
plot38, eclipse-zoom-out38, eclipse-pan-course38, eclipse-landscape-ready38,
eclipse-reopened-after-chart38 and eclipse-cleared38. Compass mode was cycled
back to north-up; no nonzero heading rotation was demonstrated, so that case
remains pending. PID stayed 31948; full logcat retained as post-chart38-logcat.log.

Before replacement, a real Duplicate selected sight action created a fourth
record. Its body, measurement and fractional instant match the source Venus
record in independently read XML; package replacement preserved all four.
Deletion/Cancel/other sorting cases remain pending. Full automatic CI pipeline
5 checked out the exact 7f68c9f SHA. Pipeline 4 passed 18 platforms and failed
only macOS retention; its build itself completed. Pipeline 5 is still running.

## 15:25–15:28: observation Back and body popup defect

On installed 7f68c9f, changed duplicated Venus measurement to 42.1234567890.
First Back dismissed keyboard and retained editor/text; second Back cancelled
and returned to observations with PID 31948 unchanged. Independently read
Sights-after-cancel38.xml is byte-identical to Sights-after38.xml. Screenshots
edit-keyboard-back38 and edit-cancel-back38.

Body combo rows measured 72 px = 48 dp at actual density 1.5. However a swipe
from y1620 to y600 closed the popup and selected Acamar. The edit was explicitly
cancelled. This is a defect, not catalogue scrolling acceptance. After recording
the popup refinement design, the shared Android choice adapter now filters the
popup viewport: drag forwards to its native scroller; stationary release queues
one activation after the callback. Native file-location and PDF zoom combos
use the same adapter and minimum-row delegate. Android compile passed (39).
Physical catalogue/Back regression remains pending. Shared astronomy and
desktop controls are unchanged.

## 15:30–15:40: stale local archive rejected and provenance repair

An incremental CPack build generated the current library in the plain
`*-android-arm64-16.tar.gz` filename. My packaging invocation mistakenly read
a cached `*-16-android-arm64.tar.gz` from the initial vertical slice instead.
The resulting import-development39 archive SHA256
a376264d3f42c7fdfb6f86ff91cbecd2a30420722009a3e2f513d434a334bb1d
therefore does **not** represent eff1655, despite its rewritten provenance.
Its installed library was 9ea782d383fb2bb4ec23303f7c59a2b082b42c03bc75c7be944a440d2d7c038b.
No physical 39 acceptance is valid. The valid 38 evidence above remains valid.

At 15:30:33, that old binary crashed PID 31948 in Adreno memcpy from the old
piDC::DrawPolygon GLES path. Exact initial-slice symbols resolve plugin frames
0x8a88a8 / 0x48e130 / 0x48e28c / 0x538204 / 0x53848c to pidc.cpp:2088,
Sight::DrawPolygon:556, Sight::Render:589 and overlay calls:346/333. The host
automatically restarted to PID 5177; recovery is not a crash acceptance pass.
Retained crash39-logcat.log, crash39-exit-info.txt, recovered39-maps.txt and
both the old installed library and intended unstripped 39 library. XML retained
all four sights/numerical values; the old writer omitted eight zero-valued
Android lunar measurement-offset attributes. Nonzero offsets were not present.

The import packager now requires the intended build library and compares every
allocated ELF section plus ABI identity, allowing debug stripping but rejecting
old runtime code/data before replacing any output. Actual negative test rejected
the stale archive and retained an existing output byte-for-byte, with no staging
file left. Actual positive tests passed the current incremental archive and both
fresh arm64/armhf packages against their own unstripped libraries. CI supplies
this required library explicitly and provenance records its SHA256. This is a
packaging correctness check, with no desktop runtime change. Correct replacement
and physical popup regression remain pending.

CircleCI pipeline 5 completed all 19 platform builds successfully for exact
7f68c9f4a66fe99ca2aeeb4935343d3a2e571f81, including macOS retention and both
Android ABIs. Approval is on hold and publish-reviewed blocked. No publication
was approved. These are CI build passes, not completion of physical acceptance
or validation of the later local fixes. Latest job state retained as
circleci-pipeline-5-jobs-latest.json.

## 15:42–15:47: verified package and popup regression

Installed f04d483 through Plugin Manager, archive SHA256
9238b55d131d9f8e34208f33fd6a9fc657c9af82b32c0afbe878db834f38f1c7.
Independently read installed library exactly equals intended packaged bytes,
SHA256 5727e76fd48dd0db1a06b58014434074d9d8f2404c1f2180603e5af7cab0afc5;
unstripped fc87091105afb8acb5c361b2de957d07679a15fa60129a52438d16ca1f30753a.
The intended executable code is eff1655; f04d483 only added packaging/docs.
Exact binaries and root metadata retained in committed-f04d483/arm64.

Actual body-popup swipe moved Sun/planets/A-stars to Capella–Polaris without
closing or committing. A later visible row tap selected Polaris, Save and
reopen retained it. Independent Sights-saved40.xml compared with before38
changes only that duplicated record's Body from Venus to Polaris; all entered
angles, uncertainties and millisecond epoch remained identical and the eight
zero-offset attributes returned. First tap after the inertial screenshot chose
Pollux; the subsequent settled popup screenshot and tap y930 selected Polaris
exactly. Evidence body-popup-*40, body-exact-tap40 and body-reopened40.

Popup Back exposed a separate defect: its container hid but the whole rendered
app surface remained black for more than two seconds and subsequent captures.
PID 5177 and foreground QtActivity remained unchanged. Controls still accepted
a tap and restored the display; this is a failed Back presentation check.
A design refinement precedes an Android-only owner-aware hidePopup change,
which also stops active popup scrolling and retains the editor. Android build
41 passed. Physical lifecycle regression remains pending.

## 15:49–15:51: popup Back lifecycle repaired

Installed exact fc9a98eee2b3039a7345ec7eb8ee62c6e92cc823 through Plugin
Manager. Archive SHA256 305c32b2b54ac755d7060a7969fbf5b1707d5443d169ad153a7bfd10ec54048b;
actual installed library equals packaged bytes, SHA256
7221f53628798705c21df1a41920c52814e9380b391ca486b401fd469c9c6538.
Assets/settings/observations backed up before replacement; backup-before41.json.
Both portrait popup Back and landscape popup Back left the editor visible
with Polaris unchanged and PID 5177 continuous. No black surface remained.
Entering 42.1234567890 then keyboard Back retained text/editor; the next Back
cancelled the editor. Sights-after-cancel41.xml equals Sights-before41.xml
byte-for-byte. Evidence popup-back41, popup-rotation-back41, keyboard-back41,
editor-cancel-back41 and import-result41.

Rotation while the popup itself remained open exposed cached portrait popup
geometry extending below the landscape viewport (popup-rotation41). The next
Android-only refinement closes the owner popup on screen-geometry change
without activating a row, so it can reopen at correct new bounds. Build 42
passed; physical rotation/reopen verification pending.

## 15:53–16:00: rotation, precision, UTC and package provenance

Installed d804ecd0a88a838fa68581844d505d24883390e1 through Plugin Manager;
archive SHA256 3d435eb53483c438b8cf27027a0fa1744fe936d6a99eb2dadd9f59aa6da7c55c;
independently read installed library 1b4938fc2bba13d41dd63a661d44ded476a301220b0f6488d20171634ebf2d9d
exactly equals intended packaged bytes. Retained binaries/root metadata in
committed-d804ecd/arm64; backups/hashes in backup-before42.json.
Native import briefly showed a black transition frame; settled installation
confirmation was visible and PID stayed 5177.

Portrait popup to landscape rotation dismissed the popup, retaining Polaris
and the visible editor. Reopening fit landscape bounds. Swipes reached the
final Zubenelgenubi row. Native highlight moved during scrolling, but Back
retained Polaris, so no model activation occurred. Evidence popup-rotation42,
popup-landscape-reopen42, body-final-landscape42 and body-drag-cancel42.

Entered seconds 12.987 and time uncertainty 1.234567891234123 in the real UTC
editor, then saved/reopened. Independently read XML contains Milliseconds=987
and the identical double value (17-digit serialization 1.2345678912341229).
Selected October 25 on the calendar and entered 01:30:12.987 UTC at the autumn
DST transition. Independent Python UTC epoch is 1792891812.987. Temporarily
set device timezone America/New_York with automatic timezone disabled. Reopen
kept October 25, 01:30:12.987 and uncertainty, while device clock changed to
10:58. Restored Europe/London and auto_time_zone=1 in a finally block after
cold start. Settled cold PID 8481, workspace card retains exact UTC instant;
Sights-after-cold42.xml equals Sights-dst42.xml byte-for-byte. Evidence time-
edited42, utc-october-calendar42, dst-entered42, dst-new-york-reopened42 and
cold-workspace42. Spring DST and planner local-time variants remain pending.

Downloaded and independently inspected actual artifacts for all 19 successful
7f68c9f CI targets. Every build-provenance.json records exact full source SHA;
every XML has Celestial Navigation/2.8.13.0 and its intended platform identity.
Both Android archives have exactly one root metadata.xml. Hashes, actual
archives/XML, Android full logs and validation results retained in
ci-platforms-7f68c9f/verification.json. Desktop archives retain the existing
packaging format. None were published, and approval remains on hold.

Source review found Android-only uncertainty error text incorrectly naming
arcseconds and lunar time incorrectly naming half-span. The shared engine
uses arcminutes and total span, as the preserved 2.8.12 documentation states.
The next source corrects wording without changing numeric values, and gives
Android observation sections navigator-facing names. Build passed; physical
wording/lunar and invalid-uncertainty checks remain pending.

## 16:07–16:09: final observation deletion and empty selection

On installed d804ecd, successive explicit Delete actions removed exactly one
selected record each: four to three, two, one and zero. Independently read
Sights-delete1-42.xml and Sights-delete-final-42.xml confirm the final one/zero
counts. The empty screen displays guidance, disables Edit/Duplicate/Include/
Delete, and keeps New available. The visible Chart action gives an explicit
“Select an observation first” message. Empty Fix shows N/A results and disabled
Go. PID remained 8481. Evidence delete1-42, delete-final-42, empty-chart42 and
empty-fix-result42. Restored the exact backed-up disposable four-record XML
with OpenCPN stopped and independently checked identical bytes before restart.
No original host objects, charts or other plugins were changed.

## 16:10–16:13: committed 5992863 wording check

Plugin Manager installed archive dc45d1ffe776c6f98983d98d059840681f58dd3c20ba8de69f021b2863e06990; installed library 50f72c3333e8378d15712b651dbbe3564d61ab994a982ac404003f7248441e46
was independently compared byte-for-byte to retained committed-5992863/arm64.
PID 10171 is the intentional cold restart after restoring test observations.
Entered -0.25 angular uncertainty. Save rejected it with an explicit arcminute
error; two Back actions dismissed warning/cancelled editor and independently
read Sights.xml equals backup-before43-sights exactly. Screens measurement43
and negative-uncertainty43. Actual section selector still had old names: Restore
consumes the notebook before the subsequent rename runs. Source moves the
rename before that transformation; do not count section names as passed yet.

Pushed exact 5992863444a5055d4f6186a6358138c6c797ff20 and cycled the existing
run-ci PR label. CircleCI pipeline 6 (5bf60f44-2592-48ad-9ff8-65ed050eda55)
confirms that exact checkout SHA. No browser trigger or publication approval
was needed. Full matrix results remain pending.

Real lunar type switching showed “Total UTC search span (seconds)” (lunar-
time43). A swipe reached both separate fractional time controls and the final
vessel motion row (lunar-time-end43), but course/speed labels were clipped in
that horizontal desktop row. The next Android-only adapter stacks meaningful
labelled rows and puts timing units above the entry, retaining colon-separated
time fields. These layout changes require physical regression verification.

## 16:18–16:22: readable sections, marked UTC and honest sort evidence

Installed 966c5a7 through Plugin Manager; archive 29d8d731ffb7432b9ebf3f2cdca41813f9fdf66c3c88f7748555709f3b790937 and exact independently compared library 049b0462f6219c6c6990f88054f2ee300a1c601ef2d6d81290e7faa27b1f3f66.
PID stayed 10171. Settled section selector displays Measurement/Time (UTC)/
Motion/Display/Corrections/Calculations (sections44-ready). A real swipe reaches
fully labelled COG true and SOG kn fields; units now precede the uncertainty
entry (lunar-time44/lunar-motion44). Static-box heading still shows Certainty
because that caption was transformed before its rename; harmless wording
cleanup remains.

Clock status reports London BST UTC+01:00, real UTC and fresh RMC with delivery
latency, and honestly unavailable chrony status. Mark held local/UTC while RMC
continued live. New sight initially uses creation time; the explicit visible
Use marked UTC button applies the hold. Copied instant is
2026-09-27T15:19:59.638Z; applied/saved/reopened XML independently contains
15:19:59 and Milliseconds=638. Screens marked-copy44/marked-applied44 and
Sights-marked-applied44.xml. Released the held clock afterwards.

Delete All Back retained all four complete records. A previous sort changed
their order, so byte equality against the pre-sort backup is inappropriate;
parsed complete attribute dictionaries match exactly. The attempted batched
sort coordinates selected different entries as the native popup moved around
its current selection. Screens show actual labels, and do not establish all
sort choices passed. Re-run each choice using the observed popup rows.

## 16:23–16:28: real lunar search and stored reference solution

Loaded a disposable compatible XML fixture from existing test/lunar_fiji_tests.cpp
(Use Case 7a screenshot inputs), with the previous five test records retained
in Sights-before-fiji44.xml, SHA256 47c582c19a5e613b7127d66815d8fc734420d25bb4ccaf429cf6f359e37d9d39.
This is fixture loading, not a claim all reference fields were typed through
the UI. Input XML/hash and explicit expectations are retained in Sights-lunar-
fiji44-input.xml and lunar-fiji44-expectations.json. Cold PID 11968.

Real Calculate lunar UTC completed with analytical fallback; the trail shows
5400 s total span as -2700 to +2700, 30 s coarse scan and 0.050 s refinement.
Results selected the southern branch near saved 20S/179.75E DR, distance 0.7 NM
from that independent worksheet position (1 NM tolerance). Explicit Save lunar
solution produced XML LunarSolution with additionalCorrectionSeconds=7.0458984375
and timeSigmaSeconds=18.832016803283018. Existing numerical regression expects
7.046 +/-0.2 s; that UTC number is a regression, not an independent reference.
Actual stored report/input snapshot retained in Sights-lunar-stored44.xml.
No global correction was applied. Screens fiji-calculated44/results-scroll44.

Initial explanation was narrowly wrapped until a swipe forced layout, and
desktop result columns clipped UTC/coordinates. Native name prompt lost its
explanation. These are failed usability checks. Designed the next revision
with selectable complete candidate cards, readable position cards, resize
layout settling and a labelled name sheet. Physical verification pending.

Independent saved-file comparison shows raw UTC/angles/limbs/uncertainties,
weather and DR unchanged numerically. Decimal strings expand to 17-digit
serialization; derived legacy TimeCorrection changes from 0 to 7 after lunar
calculation. Global ClockError remains 0. This derived field is not a changed
recorded observation time.

CircleCI pipeline 6 finished all 19 platform builds for exact 5992863.
Workflow 2f2b80fb-edba-4c39-8e53-57c09c132fe1 has approve-publication on hold
and publish-reviewed blocked. Newer local Android layout/result changes still
require their own final CI validation. No archive was published.

### 27 September 16:36–16:38 BST — clock correction persistence

Revision 44/PID 11968: typed +120 seconds, Cancel opened the explicit
Discard Changes/Keep Editing confirmation; Discard Changes retained the
original saved XML. Apply committed ClockError Seconds=120, and reopening
showed 120. Restoring zero returned the complete parsed XML, including
the nested stored lunar solution, to its original state. Recorded sight
inputs were unchanged. Evidence: clock-discard44.png, clock-reopen120-44.png,
Sights-clock-before44.xml, Sights-clock-apply44.xml, Sights-clock-reset44.xml.
This does not yet test corrected chart geometry, restart at a nonzero
correction, or GNSS status under stale/offline conditions.

### 27 September 16:39–16:45 BST — revision 45 failed hot import

Exact dea5511 imported through Plugin Manager; installed library matched
0e05a09da556416c378130dc4f44af4e0e75f2de63300d4340f993e73ad7fb5e.
Archive 0edd666b894b3e1f441211f6bcb4c1e4a0a5c1910c127d915a29812b4beaffea.
Acknowledging the success message crashed PID 11968 at 16:39:05, Qt Widgets
notify_helper frame, fault address in unloaded code. GPSServer restarted as
12925; this is not a Plugin Manager pass. Complete logcat45/crash45/exit-info45
and exact previous/new binaries retained.

The plugin has local hidden wxPendingDelete and wxTopLevelWindows symbols
(verified llvm-nm), distinct from the host idle loop. Closed modal dialogs
use deferred Destroy. Next revision drains that plugin-owned queue before
unload; physical proof pending. This is a plugin lifecycle defect until
resolved, not a confirmed external limitation.

Cold launch of revision 45 fixed initial label wrapping without a swipe.
Lunar card checks failed: the wx accessor repeated column zero in every
field, dynamic cards had not been styled because the styling helper
processes children, and a modal worker delivered refresh before position
rows were repopulated. Next revision reads native model cells, styles
the containing panels, and schedules a final refresh after branch commit.
Desktop lunar UI regression passed with display access (desktop-lunar45.log);
the initial sandbox run could not open the display and was not a plugin test.

### 27 September 16:49–16:55 BST — native route-action lifetime isolated

Revision 46 hot-import failed again at 16:49:59 (PID 13417); its deletion
queue was empty. This falsifies the proposed queue explanation. Removed
that unrelated queue drain. Diagnostic revision 47 enumerated Qt objects
whose vtables belong to this plugin after normal DeInit: exactly one
QAction parented to QMenu survived, vtable offset 0x8d6d50, symbol
vtable for wxQtAction. Full survivor log and diagnostic binary retained.

Core AddCanvasContextMenuItemPIM retains the provided wxMenuItem;
RemoveCanvasContextMenuItem deletes only its container. Pinned wxQt wxMenu
has no destructor releasing QMenu and wxMenuItem does not release QAction.
The route-menu wx object was a local temporary, leaving the Qt action alive
through plugin unload. Next revision retains the wx menu/item for the
registration lifetime and deletes both wrappers and its native QMenu after
removing registration. Desktop code stays on its original path. Physical
hot-import/disable checks still pending.

Revision 46 cold lunar cards now show the correct individual values,
complete coordinates, 4.37 NM horizontal RMS and 0.7 NM worksheet offset,
with correct initial wrapping. Explicit candidate taps did not change the
selection because programmatic wxQt list selection did not notify the
controller. Next revision invokes the existing branch controller directly
and stores its successfully committed selected index. Save and alternate
candidate evidence are still pending.

### 27 September 16:57–17:02 BST — real lunar selection/save; pending sheet

Actual revision 48/PID 14208 selection changed candidate 1 to northern
18°27.1624′ N / 154°49.9102′ E, 2733.7 NM from saved DR; restored candidate
2 southern 20°00.7274′ S / 179°45.0645′ E, 0.7 NM. Selection colour and
label matched, and positions/geometry changed. Independently saved
Fiji-North-48 correction -3.2373046875 s and Fiji-South-48 +7.0458984375 s,
with retained input snapshots/reports. Global correction and all saved
sight attributes were unchanged by saving either solution. Empty name
was rejected visibly; Cancel retained byte-identical XML. Evidence
lunar-results48-actual-candidate1.png, lunar-results48-south.png, name48.png,
name-empty48.png, Sights-north-saved48.xml, Sights-south-saved48.xml.
These are the analytical Saturn fixture, not acceptance of all lunar modes.

The initial batched cold navigation started before the app was ready and
never opened the workspace; lunar-results48-default/candidate1 images
are invalid attempts. Only the separately inspected ready workspace and
actual-* evidence above count.

Hot import released the route action: it no longer appeared in the survivor
scan. After actual observation Save, however, 125 native objects in a
closed QDialog still survived (one complete sight editor). wx Destroy
scheduling dispatches through the host wxApp, so the host queue can hold
plugin windows while modal import prevents idle deletion. Next revision
collects only plugin-local wxTopLevelWindows, removes those exact pointers
from both local and host pending queues, and releases surviving owned
windows synchronously after main-dialog deletion. The retained host
exports wxPendingDelete, verified llvm-nm. No other plugin/host windows
are selected for cleanup. This full hot-import sequence remains pending.

Revision 49 removed two wx window wrappers but still left 147 native objects
in the workspace and clock dialogs. At 17:06:01 PID 14892 crashed while
waiting at the import completion modal, without acknowledgement. Full
crash-before50.log and logcat-before50.log retained; replacement PID 15180
is not continuity. The wx-wrapper cleanup alone is insufficient. Revision
50 captures guarded native handles before destroying wx wrappers, clears
handler properties and synchronously deletes surviving owned native roots.
Real import and editor/worker regression are pending. Desktop 49 compiled.

### 27 September 17:09–17:13 BST — native roots released; Qt deferred helper

Revision 50 ab28c3e installed library ca7586891ba14f2ad97f7e6eda848242e9e9039713df848001071624b4e83da2;
archive 5e467b83c0b2432cbd7aea45b9ca06331be28714888f4a4015b7ce1b4718cf1c.
Backup-before-ab28c3e manifest retains original library/profile/sights hashes.
Actual lunar calculation/results, Sight Save and Clock Cancel ran before
native tarball import. DeInit now left zero native objects in widget trees,
but acknowledgement still crashed at 17:11:57, PID 15297 to 15808.
Sights-import50.xml retains one sight and all three stored solutions.
Qt notify_helper disassembly identifies a receiver's unmapped vtable.
Pinned wxWindow destructor disassembly calls deleteLater for the parentless
wxQtShortcutHandler as well as widgets. Such helpers escape the tree scan.
Next revision completes only Qt's already-posted DeferredDelete events while
plugin code is mapped, without pumping input/timers/workers. Desktop 50 and
its existing LunarUiSmoke regression passed. No hot-import pass yet.

### 27 September 17:17–17:20 BST — hot import passed; disable touch failure

Revision 51 406ceedd0dbed6683835c9457f5ff851d9de26ed library
83cf3e9deb282c04e72b98cabb44da6aaf51900bf9ca5463075f90073a1a1085;
archive 0feec3fb15bfab4755294eedfe9cc609a632b72de39c8ad9499d4789dafeb29e.
Real lunar worker/results, Sight Save and Clock Cancel preceded native import.
At 17:17:39 DeInit reported zero surviving native widget-tree objects.
Acknowledgement, Settings OK and actual workspace reopening succeeded with
PID 16013 unchanged. Independently read library matched and Sights-import51.xml
retained one sight and three named lunar solutions. Screens pm-import51.png,
pm-chart51.png, pm-reopened51.png and logcat-import51.log retained.
This is hot-import acceptance of this exact sequence on the patched host.

Attempting Disable then failed at 17:19:57: PID 16013 to 16556,
QGestureManager::getState +628, recognizer argument 0x16. Earlier taps only
selected/collapsed the host row; actual Enabled label tap triggered teardown.
Qt 5.12.2 source iterates its recognizer map while delivering touch callbacks;
synchronous native teardown removes QScroller recognizers during that loop.
Next host patch defers the Android checkbox apply to QTimer(0), with a
weak panel lifetime and disabled checkbox to avoid duplicate pending toggles.
Desktop callback is unchanged. Host 254045db1 compiled; physical retest pending.

## 17:26–17:37 BST — host touch toggle and import lifecycle regression

Host source `254045db1f84bcaca350288bd9e4cb2c45350369` defers the
Android Plugin Manager toggle to the next Qt event turn, after its touch gesture
recognizer iteration returns. The previous direct toggle crashed in
QGestureManager::getState when DeInit removed a live QScroller recognizer.
This is additional to the chooser and host-owned bitmap fixes. Desktop code
keeps the original synchronous path. POBsoft (1985-2026) comments identify the
three patched source files without replacing original licences/copyrights.

Final host52 APK SHA256
`b9dea2dc6559eae86dce7cdf38e6bec4c3ade54a4482857c0179a14169c90239`,
libgorp SHA256
`af5202aee93d5a110d6067b78cbcface4e616f79f26af565b23251171f65e916`.
Independent archive comparison retained every non-signature APK entry except
the intentional libgorp replacement; DEX, resources and other libraries match
the original. A preliminary package incorrectly stripped all META-INF files;
it was replaced before acceptance tests with the corrected package retaining
non-signature service/licence entries. No uninstall or data clear occurred.

Physical disable: PID 17432 remained, bEnabled=0 persisted and the Celestial
toolbar action disappeared. Re-enable: bEnabled=1, toolbar returned, workspace
accepted Edit, Save and Clock correction Cancel. Enabled xGRIB/xWeatherRouting
were retained. Native host checkbox artwork still appears checked for disabled
rows; model/config and toolbar state provide the toggle evidence. This visual
host discrepancy is recorded, not counted as a plugin checkbox pass.

Native Plugin Manager selected the current 406ceed tarball from Downloads,
completed installation, acknowledged the success modal and reopened the
workspace with PID 17432 unchanged. Installed library independently read back
SHA256 `83cf3e9deb282c04e72b98cabb44da6aaf51900bf9ca5463075f90073a1a1085`.
Cold restart intentionally changed PID to 18155; the full workspace appeared
and Sights.xml retained the single Fiji observation and all three solutions
identically. No new crash or ANR observed during these checks.

Evidence: host52-disabled*.png/conf, host52-reenabled.png,
host52-enabled-chart-ready.png, host52-workspace.png, host52-editor.png,
host52-clock.png, host52-chooser.png, host52-import-result.png,
host52-import-reopened.png, host52-cold-chart/workspace.png,
host52-enabled/cold-Sights.xml, host52-import.log, host52-crash.log and
host-toggle52-provenance.json in the private audit directory.

The broad QObject/vtable diagnostic walk used to investigate revisions 48–51
has been removed from the next production candidate. The exact owned-window
cleanup and already-posted DeferredDelete drain remain; the diagnostic walk
was neither required for cleanup nor proof about parentless shortcut objects.

## 17:39–18:03 BST — exact-source CI and unofficial host candidate

Pipeline 7: cf41ff6a-371d-426a-b518-6542ce8ff871, workflow
106a3f08-677c-4287-8a81-04f30882ba9c, plugin source
9db39657138374d1fea9f87af083a4a6e7c0b0a4. All 19 existing build jobs
passed. approve-publication remains on hold and publish-reviewed blocked.
Every target's retained provenance SHA, file sizes and SHA256 hashes were
independently checked after download; both Android archives contain root
metadata with matching source/version and expected ELF machine. Desktop
packaging retains its existing sidecar metadata format. Artifacts and full
Android logs retained in ci-platforms-9db3965; verification.json per target.

The local 9db3965 ARM64 binary was imported through the native chooser on
host52, acknowledged and reopened with PID 18155 continuous. Its installed
SHA256 is a0055d93689dd5a4fb929cb831515a41904c123bce969044d6d64df6adb555bb;
tar SHA256 0eee7e054d7af3c1cdb3d97ff3284d28baf25ce5f2da14d3d774699f22292b39.
The separate CI binary has different build flags and is being installed for
physical acceptance; local-binary acceptance is not silently transferred.

The requested 5.14.1-pob220-import-fix APK was built, version-labelled and
installed. See android-host-tester-install.md for final fresh-dependency hash,
exact host source, preserved file checks and remaining tester-flow requirements.
The broad plugin feature matrix remains incomplete. No new crash/ANR was
observed in the tested host52/129 import paths; previous crash evidence remains
in the retained buffers and is not deleted.

### 18:03–18:07 BST — CI ARM64 binary on final fresh host

The actual CI 7 tarball SHA256
82debc3f0ac151feae1d92ae8ddfdaa7a480baf52b293baf5555e1a10302ac68
was selected in the native chooser and imported on final host a4b40b4b4.
Installed library independently matched
0962daee98d4c15ea88673a17f3e11947d30b477544fac75d8fc906ce948bbf9.
Acknowledgement, full workspace input and Sights.xml were verified; the full
XML remained byte-identical. Touch Disable persisted bEnabled=0 and removed
the toolbar action; Re-enable persisted bEnabled=1 and reopened the full
workspace. PID 20434 remained continuous throughout, with no new crash-buffer
entry. Existing xGRIB/xWeatherRouting remained enabled. Host artwork continues
to show a checked image for disabled rows; model and actual action prove state.
The host's ShowActiveRouteHighway flag returned to 1 despite narrow restoration;
ToolbarX=4 persisted. Record this startup/display behavior rather than claiming
complete profile identity. The user's routes/charts/observations were untouched.
Evidence: ci7-device-*, ci7-host129-*, installed.so and Sights.xml.

### 18:15–18:25 BST — normal installer and CI7 Sun workflow

Final fresh host APK 05abd439… was opened from My Files > Downloads using
Package installer > Just once. Update prompted normally; Play Protect's
unfamiliar-developer notice offered More details > Install anyway. This
per-file choice completed installation without disabling Play Protect. Open
launched code129, version5.14.1-pob220-import-fix, PID23025. The host reached
the chart, retained xGRIB/xWeatherRouting toolbar icons, and accepted a real
Celestial workspace tap. Sights.xml matched the pre129 backup byte for byte.
Browser download and stock-package migration are still unexecuted.

The installed **CI7** plugin was used to create a new Sun lower-limb sight
entirely through the UI: 2025-07-20 12:47:00.000 UTC, Hs30°, uncertainty0.1′,
eye3.5m, T10°C, P1010hPa, IE+1.5′, no artificial/short horizon. Find Body
Manual set DR43.2366916666667,-77.533415, then Use position and Sight Save.
Existing independent worksheet fixture: test/altitude_tests.cpp, first
INTERCEPT_SIGHTS row (Open CPN testing Intercept Calculation worksheet).
Expected Ho30.1571°, Hc30.1818316468372°, Zn89.39818°, intercept1.483898810232NM
away. Verified DE440s/offline DUT1 reduction displayed Ho30°09.4265′,
Hc30°10.9206′, Zn89°23.9000′ and1.494102NM away. All differences are within
0.1arcmin /0.1NM; this fixture supports navigation tolerance, not a claim of
sub-arcsecond independent DE440 validation. Actual saved XML retained the
entered date/time, DR signs and precision, limb, corrections and uncertainty.

Reopened sight, changed Hs to31°, and explicit Cancel returned to the cards.
XML remained byte-identical to the saved30° observation. Show selected sight
on chart hid the workspace and drew its north-south LOP just west of DR,
consistent with the approximately1.49NM away intercept and eastward Sun.
Real zoom and pan moved/rescaled the line, PID23025 continuous. Duplicate
created a third card retaining the original sight. Screenshots ci7-sun-*;
saved/cancel XML independently retained in the private audit directory.

Reopening showed0.10000000000000001 for0.1 uncertainty. Android-only display
formatting now seeks compact significant-digit text which parses back to the
identical double. XML/report serialization and desktop formatting are untouched.
Boundary tests verify named decimal examples, adjacent-double precision,
signed zero, extreme normal values and3000 deterministic wide-range doubles
through both the Qt angle and wx numeric parsers. UTC boundary tests pass in
four zones including DST gap/overlap refusal. Physical revised-build
round-trip verification follows below when executed.

### 18:27–18:41 BST — iteration54 exact entry and independent Sun fixes

Committed92a9e8f was imported through Plugin Manager on final host129.
Installed library SHA256986f430c0dbab527c9be0f6949c27ac97eb393aca950a0d63b76854833f29216;
tar SHA2566d343b14f60a7c32a510cba2699f1bcc2f20d41603a0c615a2cca41ddd128f6c.
PID23025 remained continuous. Reopened lunar46.415°/0.2′ and Sun uncertainty0.1′
now display compactly. Unchanged Save, and unmodified67°1′ DMM Apply followed
by Save, each retained the entire pre-operation XML byte for byte. Precise DR
43.2366916666667,-77.533415 reopened without binary display tails. Evidence:
fix54*, post54-roundtrip.xml, post54-dmm.xml; personal files stay private.

Sun2 and Sun3 were entered by editing duplicates through the real UI:
2025-07-20 17:16:33 UTC/Hs67°1′, and21:18:20 UTC/Hs35°5′, with the same
DR/eye3.5/T10/P1010/IE+1.5/uncertainty0.1 as Sun1. Independent worksheet
INTERCEPT_SIGHTS rows2 and6 expect respectively Ho67.1934/Hc67.2761482927961/
Zn179.99607/4.964897567766NM away, and Ho35.2452/Hc35.194154591663/
Zn265.67311/3.06272450022NM toward. Displayed actual reductions: Sun2
Ho67°11.6024′,Hc67°16.5692′,Zn179°59.7990′,4.966866NM away; Sun3
Ho35°14.7129′,Hc35°11.6390′,Zn265°40.3976′,3.073866NM toward. Both within
0.1arcmin/0.1NM of independent fixture. Long-list content actually moved and
the final Delete action was reachable. This is not acceptance of every body.

Using worksheet GHA/Dec/Ho alone, a separate SciPy angular-altitude least-squares
calculation gives43.31832278,-77.5903735 (worksheet-three-sun-independent.json).
Tablet stationary solutions: Sphere43°19.0572′N77°35.4026′W;
Plane43°19.3002′N77°35.3679′W; Cone43°18.8682′N77°35.3489′W.
Each is within0.3NM of the independent worksheet solution, allowing for different
legacy objectives/provider. Cone2 gave46°10.3360′N77°01.3802′W from boat
seed53,-2 and45°47.4588′N77°06.4454′W even with nearby seed43,-78: FAIL.
Source baseline derivative omits cross terms of dot(body,X)/length(X).
Android-only correction uses the full mathematical gradient, tested independently
against finite differences at1000 general positions. Desktop solver remains
unchanged by user instruction; physical repaired-build evidence follows only
when executed. Also found editable fixed-algorithm selector opens the keyboard,
integer-only initial DR and clipped running summary/7-column residual table.
Android replacements are prepared; none is marked passed before tablet retest.
The baseline OnGo only centres the chart; corrected the inventory/design's
incorrect reference to fix-waypoint creation. No working command was removed.

### 18:44–18:50 BST — iteration55 Cone2 and precise seed retest

Exact060fa55c892fe3696553f1cfa7247317c34b02f2 imported via native Downloads
chooser. Tar SHA2569ee72b7bc574ef494f976a07eec1937f135e2061ab6ef84c1ed21aaaec5118a9,
actual installed library52a79efd628f1b591bc6f896ffaeaacca3cccd3bedee9619092c709911248078.
Acknowledgement and Fix input accepted; PID23025 continuous. Full algorithm
field tap opens four72px/48dp popup rows without keyboard. Repaired Cone2
now43°18.8682′N77°35.3489′W,Error0.000329 from distant boat seed
53.17950631666666,-2.858162016666667; nearby precise43.2366916666667,-77.533415
seed gives same fix. Within0.3NM of independent worksheet result. Invalid91°
latitude gives N/A, explicit Invalid DR position and disabled chart action.
Latitude DMM opens with43°14.201500000002′ and unmodified Apply retains the
exact seed text. Summary wraps visibly, final clock-source control reachable.
Residual presentation FAIL: wxQt GetItemText(row,col) returned the first-column
time for every requested column. Corrected Android presentation is prepared
to read observations/residuals directly, not depend on hidden list text; next
physical build remains pending. Shared seven-check ctest suite and explicit
existing desktop FixUi regression passed; initial xvfb-run unavailable, actual
available display run succeeded. No desktop runtime formula changed.

### 18:51–18:53 BST — motion-event failure isolated

On installed55, calendar selected20 July2025 and time21:18:20.000 UTC.
Typed COG90.0/SOG5.0 appeared correctly but the old zero-motion result stayed
visible: FAIL. Toggling running mode forced recalculation using those exact
values and produced43°17.7620′N77°06.1079′W,18.08′RMS. An independent
worksheet GHA/Dec/Ho calculation, GeographicLib2.1 WGS84 backward travel
and SciPy angular least squares gives43.2960015966,-77.1015520798,RMS18.07617156′.
Agreement within0.1NM and0.1arcmin. These stationary observations deliberately
contradict the imposed5kn eastward motion; large residuals are expected, not
a claim that this is a good navigation fix. Source issue: pinned wxQt typed
double values do not emit wxSPINCTRLDOUBLE. Android native valueChanged now
schedules a coalesced, dialog-owned recalculation after input callbacks, installed
only after programmatic initialization. Physical rerun is pending.

Show fix on chart hid both sheets and centred the red cross at the computed
position; Sun loci and prior eclipse geometry remained visible alongside
xGRIB/xWeatherRouting. PID23025 continuous. Memory and crash buffers retained
as memory-fix55.txt/crash-fix55.txt. Final import candidate57 combines the
residual-model workaround with the motion-event fix;56 was built/packaged
but not installed or counted as passed.

### 18:55–19:01 BST — iteration57 physical fixes and sequence review

Exact0cae732 imported through Plugin Manager; tar SHA256
4f73069b8bb994dcd39b8062fa79ab7a30716b6b95d99191d9fd7a08ddda0c7e,
actual installed library b25215efa1e9632446e12ca0475a35df2b2352ff01d53da2b4284d9ae96e9d8b.
PID23025 continuous. Each residual now displays its own full date/time with
milliseconds, Hc and signed Ho-Hc, plus actual saved shift/bearings when used.
Stationary residuals +0.95′,-0.06′,+0.96′ agree with independent worksheet
least-squares calculation to0.1′. Typed COG90/SOG5 immediately changes RMS
to18.08′ and residuals to+22.09′,-1.46′,+22.14′ without a trigger workaround.
UTC21:18:20 converts to local22:18:20, date20 July2025; summary UTC epoch
and fix unchanged. All last controls and labels visible after genuine swipes.
Sights-fix57.xml independently confirms the four observations' retained values.
Desktop57 full build and existing FixUi smoke passed.

Sequence analysis: three saved-DR Sun sights, mean-1.13′/SD4.03′/median-1.49′/
MAD3.47′/trend+0.51′h. Independent worksheet intercepts give-1.12869/4.02558/
-1.48390/3.480999/+0.50856, within0.1′(and0.1′h). Moving checkbox with the
earliest saved DR, COG90/SOG5 and explicit Analyze gives mean+13.04′/SD28.09′/
median-1.49′/MAD3.30′/trend-0.73′h; final sight45.4′ marked outlier and red
in plot. Independent GeographicLib+worksheet residuals-1.48390,-4.79264,45.41056
match0.1′, robust threshold excludes final sight and remaining trend-0.73649′h.
Complete reference calculations retained as worksheet-*-independent.json.
Physical table truncates dates/Ho/Hc: FAIL presentation. Android vertical
result cards and explicit numeric commit/label relayout prepared for build58.
No sequence inputs changed the saved observations.

### 19:02–19:16 BST — sequence cards and two event/selection defects

Iteration 58, source `40bae1a`, was imported through the native Plugin Manager.
The tarball SHA256 is
`b65135a35956acc106d6d33ecf8d4ac1fd1cfa43f02643838413cdbebb61edf1`;
the independently read installed library SHA256 is
`6cc51c115788dd48b9c9c066ae0fc83eb58aeab4585935994339fb8b7da20ad9`.
PID 23025 remained continuous through import acknowledgement, reopened input,
stationary and moving analysis, font changes, rotation and nested Back.

The three result cards now show complete UTC dates/milliseconds, Ho, Hc,
signed intercept and assessment. Stationary and imposed-motion statistics and
the final outlier agree with the independent references above. Genuine content
swipes reach the final card and Close in portrait and landscape. At temporary
font scale 1.3, closing/reopening the sheet visibly increases its fonts; wrapped
text and the final control remain readable. Changing font scale does not
restyle an already open sheet. Rotation with the latitude keyboard open retains
the entered precise value. First Back dismisses the keyboard and retains the
sheet; second Back closes the sheet and leaves the workspace usable.
Latitude 91 produces a clear validation message, clears old cards and clears
the old plot. Saved observations remained byte-identical:
`Sights-fix57.xml` and `Sights-analysis58.xml` both have SHA256
`030d4193395b4cb8bddce684d00fb1477c00e56bc0661a70f2af81cd9bcc1020`.
Original font scale 1.15 and portrait rotation 0 were restored.

Selected-body filter presentation FAILED: tapping the Saturn card shows
“selected,” but opening Fix then Analyze still shows a generic filter caption.
It was initially inferred to be disabled; activation was not tested at this
stage, so that inference and the proposed selection-loss cause were unproven.
Android selection queries were changed to use the shared sight selection flags;
desktop queries remain table-based. Physical action testing follows below.

The earlier running-fix epoch test also exposed a stale summary: typed seconds
20.000 appeared in the field while the summary still showed 42 until a motion
edit triggered recalculation. NauticalTimeCtrl now coalesces native spin and wx
notifications through a control-owned timer. Programmatic SetValue suppresses
notifications and cancels pending input notifications. Android and desktop
builds passed; actual typed-time/calculation/persistence checks are pending.

### 19:17–19:27 BST — iteration 59 live time retest

Source `2207d08c7feda526f45aa2e918904a298d47425f` imported successfully.
Tar SHA256 `5d2f6b18fc18ec8f3c71c050ed0aa1af011f7545d90aa1609b2ea5a1cc10340d`;
actual installed library
`20387bc2220beabb7cbbcd514a2e1dc87e366e27a03be4231c1a93839d9afdf0`.
Hot import retained PID 23025. All seven desktop/shared checks passed in
53.29 seconds; explicit desktop FixUi and Android UTC/angle boundary checks
passed. These checks do not establish physical acceptance of all feature families.

The generic selected-body caption persisted after an intentional cold restart
(new PID 31713). Returning to Observe still displayed the selected Saturn card.
The action was still not exercised; this does not establish a broken filter.
Android now ignores hidden-table selection/deselection callbacks so cards own
selection. This ownership change is committed as `3f2d60e`, built and packaged
but not installed. Do not count an unexecuted action as a pass or a failure.
Hot process maps retained five deleted prior plugin mappings as well as the new
one; a cold process has only the current mapping. Record this residual lifetime
finding and use cold-start evidence to establish candidate behavior.

On the cold iteration 59, the visible calendar selected 20 July 2025. Typed
hours 21, minutes 18 and seconds 20.987 immediately changed the common-epoch
summary from 18:23:32 to 21:18:20. COG 90/SOG 5 immediately changed RMS from
0.78 to 18.08 arcminutes. This passes the stale-time event regression. The
summary omitted milliseconds; Android summary formatting is being repaired
to expose the full result epoch. Saved observations remained byte-identical
after closing (`Sights-fix59.xml` matches the preceding snapshots). Crash buffer
retained as `crash59.txt`; PID 31713 continued through these operations.

### 19:29–19:35 BST — actual filter activation and caption cause

Iteration 61 (`f4e53cc`) imported through the native chooser. Tar SHA256
`6380de3ccfd985e6d485d150c8b39bbd8ef26c53123afe7b071e0bddbbd59dd5`;
independently read installed library
`4a9a78383b67e9d51f0758accaf89bb8dba7cba78629e9999c4cd6f48980e191`.
The first automated attempt had remained on Display because the tab tap was
too early; library verification rejected that attempt (still iteration 59).
The subsequent confirmed Plugin Manager import succeeded with PID 31713
continuous. An intentional cold restart produced PID 945.

Actual filter activation on the selected Saturn lunar sight gives the expected
“At least two visible, calculated altitude sights are required,” clears the
plot and leaves no old cards. Unchecking restores all three Sun results. Thus
the earlier disabled-filter inference was incorrect. Pinned wxQt's wxCheckBox
header has no native SetLabel override: the generic constructor caption remains
visible after SetLabel. The Android constructor now supplies the final caption
containing the selected body. Caption retest is pending in iteration 62.

### 19:36–19:40 BST — iteration 62 caption and fractional epoch

Source `0ccce32c388a60e6150ac96682e4e1ad81e9c16e` was imported through
Plugin Manager. Tar SHA256
`dcec63d07dec145b200e6d58301eee1d5df3549ebb33dad3550eb82f13384f78`;
independently read installed library
`21f4b5de75485bdb988fb676f4ff3c28652b3cc163e1a4915523c7d257a7bf90`.
PID 945 continued through import and these operations. The actual caption is
“Only highlighted body (Saturn)”; checking it gives the expected insufficient
altitude-sights validation with no retained plot/cards. Actual Sun-filter
activation on iteration 61 retained all three independently verified Sun rows.

Typing 21:18:20.987 shows the complete resolved UTC epoch. Changing to Computer
local displays 22:18:20.987 while the summary retains 21:18:20.987 UTC and the
same result. The attempted return to UTC did not select that popup row, so no
return-to-UTC pass is attributed to this operation. Back closes the Fix sheet
and the workspace remains visible and usable (`fix62-closed.png`).
`Sights-final62.xml` remains byte-identical to `Sights-fix57.xml`, SHA256
`030d4193395b4cb8bddce684d00fb1477c00e56bc0661a70f2af81cd9bcc1020`.

Full logcat retains historical crashes; the latest fatal signal is the already
investigated 17:19 host gesture failure. No new fatal signal/exception appears
during this test window. `lastanr62.txt` reports none since boot. At this point
host memory is 501523 KiB PSS / 566064 KiB RSS; this is a snapshot, not a memory
stability pass. Font scale 1.15 and portrait rotation 0 are restored.

The independent three-Sun fixture calculation is now reproducible in
`test/worksheet_navigation_reference.py`: existing worksheet GHA/Dec/Ho
inputs, spherical astronomical triangle, GeographicLib WGS84 track, SciPy
angular least squares and independently calculated robust sequence statistics.
No plugin algorithm is imported. NumPy 2.5.2, SciPy 1.18.1, GeographicLib 2.1
reproduce the retained reference JSON. Dependencies are optional tools, not
added to the production or CI runtime. Tolerances and the deliberately poor
imposed-motion residuals are stated in the script.

### 19:44–19:49 BST — planning layout failure

Iteration 62 opens the planner after its calculations complete. A repeated
launch tap opened the position popup once the sheet appeared; this is an
automation timing issue, not proof of a launch failure. The actual portrait
layout (`planner62-context-retry.png`) hides longitude, time and speed beyond
the desktop grid and clips event table names/times. The planner family fails
touch-layout acceptance. A separate scrollable Context page and full event
results are being implemented; other planning workflows remain pending.

### 19:51–19:56 BST — iteration 63 context and event retest

`a99f033f4ec7c79f62a52630be0c9bbe82939981` imported successfully with PID
945 continuous. Actual `manPlug/libcelestial_navigation_pi.so` hash
`571a357659059dcd4faaadca8e1dab9dafe9a5ab52aca2f261e2a1290079846f`;
tar hash `3192670a6a66e53c98bb8b933f3e60f2bb244727a0b581899450f9fa682f681b`.
An initial verification command used nonexistent `files/manPlug`; its error
bytes were rejected and the correct `manPlug` binary was then read and matched.
The packaging identity check also rejected a stale tarball after an invalid
`tarball` build target; regenerating with CMake target `package` produced the
verified source-traceable candidate. Neither rejected artifact was accepted.

Context now scrolls genuinely and exposes position, longitude, time, input and
display bases, entry format, eye height and motion. Changing nautical to platform
format shows only the calendar button and hour/minute/fractional-second inputs.
The untouched instant remains 18:52:38.329. Double tapping the calendar year
opens the physical Samsung numeric keyboard (`planner63-year-keyboard.png`);
typing 2024 then opening month commits the year and dismisses the keyboard.
June and 21 June 2024 are selectable. Actual fields contain Greenwich 51.4779 N,
0 E and 00:00:00.987 UTC; date/time changes select Manual.

Live recalculation FAILED: resolved UTC and the events still show the previous
2026 boat context even after the new inputs and typed eye height 0. wx timer
notifications are not reaching the refresh handler in this Android candidate.
An owned native QTimer replaces Android refresh scheduling; desktop wx timers
are retained. This is not yet a numerical reference pass. Events do display
complete names, UTC/display times, bearings and positions without table-column
clipping. Other planning pages are not yet accepted.

Desktop PlannerTime six cases passed; the initial UI invocation skipped by
default and is not counted. Explicit `CELESTIAL_RUN_UI_TESTS=1` invocation of
LunarUiSmoke (including planner) passed in 6343 ms, with no runtime desktop
changes. Android and desktop builds passed before packaging.

### 19:58–20:09 BST — iteration 64 cold live refresh and Greenwich reference

Source `1a2a7af8e1b3a2e2c540b99554b7b579aff6a81d` imported through the
native chooser with PID 945 continuous. Tar SHA256
`990897c6370467d4f6ab26ebde66fc07e91f6bb5240c243be102f3a9f81f15bd`;
actual installed library
`df7693acdfbb547d85b895434746cf9cea4ed221ae4b6d1aecdedd93dfc4706c`.
A deliberate cold restart produced PID 5462. Manual latitude 51.4779, longitude
0 and platform-entry format persisted. Date/time starts at Now when reopened,
as in the baseline; do not describe it as persisted manual time.

The first combined automated input/Back sequence closed the sheet and moved
the chart. This is not an accepted reference workflow. The repeated test
verified the keyboard before each Back and allowed each calculation to settle.
Native hour 0, minute 0 and seconds 0.987 select Manual and update the resolved
UTC immediately. The visible calendar transaction selects 21 June 2024 and the
summary shows `2024-06-21 00:00:00.987 UTC`. Typed eye height 0 recalculates
without pressing Calculate. This passes the stale-context regression. Detailed
responsiveness timing and cancellation are still pending; six/eight-second
automation settling waits must not be interpreted as measured calculation
durations or a responsiveness pass.

Actual Greenwich events show sunrise 03:42:22 UTC and sunset 20:21:27 UTC at
51.4779 N, 0 E, sea level. Existing independent civil-time fixture
`HorizonEvents.GreenwichSolsticeMatchesPublishedCivilTimes` expects about
03:43 and 20:21 UTC with a five-minute allowance for horizon/refraction. The
physical results satisfy that tolerance. Noon is 12:01:55 UTC; this value and
Moon/twilight results were inspected but are not independent numerical passes.

Full event names, UTC/display times, true bearings and observer coordinates
are readable; genuine swipes reach the final principal phase and Moon summary.
The first `settings user_rotation=1` did not rotate: the cold host had enabled
automatic rotation. That screenshot is not landscape evidence. `wm user-rotation
lock 1` produced actual 1920x1200 landscape; three swipes reached the final
Moon summary and visible Close (`planner64-landscape-final.png`). Portrait
was restored and the immediately observed pre-test `free` rotation mode and
user_rotation0 restored. Font scale remains1.15. Larger-font planner checks
are still pending. Back closes the sheet to a usable workspace.

`Sights-final64.xml` is byte-identical to the previous reference snapshot,
SHA256 `030d4193395b4cb8bddce684d00fb1477c00e56bc0661a70f2af81cd9bcc1020`.
PID5462 remains continuous through these workflows. Full `logcat64-all.txt`
contains no new fatal signal/exception in this window; the last fatal signal
remains17:19. Last ANR reports none since boot. Memory snapshot467276KiB PSS /
532216KiB RSS is not a sustained memory-stability pass. Explicit opt-in desktop
LunarUiSmoke (including planner) passed6914ms after the timer change. Other
planning pages, invalids and source modes remain open.
### 2026-09-27 20:15–20:17 BST: chart preference restoration

The profile comparison after the Planner rotation test found the host chart
orientation had changed from north-up to course-up during a rejected automated
tap sequence. The visible compass control was used to return to north-up; host
Settings > OK then saved it. Independent app-owned profile inspection confirmed
`Canvas/CanvasConfig1` has `canvasCourseUp=0` and `canvasHeadUp=0`. PID 5462
remained continuous. Private screenshots `chart64-north-final.png` and
`chart64-north-saved.png`, and `profile64-north-restored.conf`, remain in
`/tmp/celnav-android-20260927/`. This is preference restoration, not an additional
celestial calculation acceptance pass.

### 2026-09-27 21:55–22:17 BST: Planner body cards and real CSV output

Design refinement preceded implementation. Android-only revision
`aa861d3ce065f6d2edb579ba281d6ac8c35922b2` replaced the clipped Bodies and
Almanac tables with complete cards, explicit sorting and native selection,
wrapped pair/triad recommendations, and invalid-context output clearing.
Desktop controls and calculations remain unchanged. Android and desktop builds
passed; explicit desktop LunarUiSmoke plus six PlannerTime cases passed in
6416 ms. Candidate tar SHA256
`30407aa06f91ce2b60696bfb6956b043897397c9a206a6b35ce12830a5429292`;
installed library SHA256
`bc186431732579bf4cb3856ced9ad439cecc9d227b160565ac12874bfaa117fb`.
The real Plugin Manager import retained PID5462, and a deliberate cold restart
created PID9674. Both imports were independently checked against actual
`manPlug/libcelestial_navigation_pi.so` bytes. Backup
`before65-profile-sights-library.tar` SHA256
`c81f19ab93569787a419e0a46247406583e189a01e107a11c676c28ab4112823`
contains the pre-replacement binary, private profile and observations.

Selected the worksheet Sun record at2025-07-20 17:16:33 UTC, then selected
Planner **Selected sight DR** and **Selected sight** time. Actual coordinates
43.2366916666667 / -77.533415 were retained. Almanac Sun Hc displayed
67°16.5692′; CSV Hc67.276154° and declination20.512846° agree with worksheet
references67.27614828° and20.5129° within0.1 arcminute. The complete displayed
UTC, GHA, SHA, Aries angles, declination, Hc and true azimuth were inspected.
Exported through the real touch save browser into
`celestial-reports/planner-worksheet65.csv`, then independently read the file:
175 rows, seven bodies, nine columns,25 distinct UTC epochs separated by3600 s.
CSV SHA256 `fdf48d51f899d30a29187d0535cbeabf306f8890c5a3067473d5da9b66afc0d3`.
This verifies that worksheet Sun case and output serialization; it is not an
independent accuracy pass for every body. Keyboard Back retained the chooser,
then visible Save wrote the actual file. Overwrite refusal remains pending.

Body selection enabled Create sight. The full recommendations, final magnitude
choice, below-horizon toggle, sky plot and legend were reached by real outer
page swipes. The below-horizon toggle visibly added bodies at the horizon.
However, iteration65 card swipes moved the ancestor page instead of the native
list: **FAIL**, not a scrolling pass. Revision
`03f9cf68b1cb7ec50ad5a0c69ad5fcf1a0ef1dd9` registered the inner gesture;
tar SHA256 `0972098566f6447038d853da0e5bdc9c6c709b06d02e784d36abe6d247c71ddb`,
library SHA256 `d3311d3dcf37f30d23e468d342a926e0c20a2f6ad42b6d96a55ccd5a9204e67d`.
Actual import retained PID9674. This stopped ancestor movement but still did
not move list content: **FAIL**. Hc descending sorting retained the selected
Moon identity; adjacent Aldebaran39°23.9484′ and Moon39°21.0676′ were correctly
ordered. A later screenshot had a small left offset; sizing regression remains
open. Early navigation sequences which opened Polar or an unsaved New sight
were rejected automation; the unsaved sight was explicitly cancelled.

Revision `d88e38a097a54bd5ed25c01537a6ad2d33c7cbd2` supplies explicit native
ScrollPrepare/Scroll geometry and an Android CSV adapter retaining each
nonzero fractional UTC without changing shared numeric columns. The new
regression covers two different millisecond instants within the same whole
second, a whole-second row, negative values, empty output and unchanged legacy
numeric serialization. Desktop build and eight explicit UI/time/CSV cases
passed6349 ms. Candidate tar SHA256
`6a2aab6f0b4f9b85649605d0d0e314506e9428d0108b49f24058b1da69025fd0`,
actual installed library SHA256
`ef9b8ff01c1b28c57bb664a3ef5d410406ec4b91362b9778ed06e3edb549fc7d`;
Plugin Manager import retained PID9674. Physical scrolling and fractional CSV
verification of this revision are still in progress at this entry.

All binaries, full configure/compiler/test logs, screenshots and exported test
files remain under `/tmp/celnav-android-20260927/`. No release was published;
19-platform CI at9db3965 does not validate these later runtime revisions.

### 2026-09-27 22:17–22:40 BST: Planner scroll repair and responsive calculation

Iteration67 still failed native-card scrolling: the ancestor moved while body
cards stayed fixed. Actual refresh durations were3415/3538ms, so neither scroll
nor responsiveness was accepted. A design revision preceded iteration68:
Bodies and Almanac now own one full-height native list, while recommendation
limits, pairs/triads and sky are a separate scrolling task page. Hidden desktop
output tables are no longer populated on Android. Source
`047ce08` tarSHA256 `c0ec5e131c577fb4687ce06f1490094142afc9cdb9011d9b5b1d8c6262cabe37`,
actual installed librarySHA256
`5b27560f3a9c1304a55349b3275746d8c4c8cd1346a9ffeb1159a4d69ffa6bc0`.
Plugin Manager import retained PID9674; deliberate cold restart created12582.
Eight explicit desktop UI/time/CSV checks passed6355ms.

Real body-card swipes now moved Moon/Venus to Jupiter/Sun and through the final
Adhara card, with every final field visible. Selected Moon identity and the
fixed sort/Create actions remained unchanged. Screenshots
`planner68-bodies-before.png`, `planner68-bodies-after.png`,
`planner68-bodies-last.png` retain actual movement/final-row evidence. This
repairs the observed portrait failure; landscape/font acceptance is separate.
Refresh still took3408/3550ms, disproving hidden-table work as the main cause.

Design revision preceded `5846bf1`: one owned persistent worker snapshots
validated context and computes the unchanged horizon/phase/body/almanac models.
Android-only cancellation is checked at ephemeris boundaries. A calculation-only
Sight constructor avoids reading GUI preferences or changing the observation
colour cycle on this worker. Generation numbers reject superseded results;
widget updates stay on the GUI thread, and Close cancels/joins before unload.
New regressions verify cancellation scope isolation, obsolete-generation refusal,
175 almanac rows, exact987ms epoch and independent worksheet Sun Hc/declination
within0.1arcminute. Ten explicit UI/time/CSV/worker tests passed7888ms.

Iteration69 tarSHA256
`b9d66ac37c445848c4988bfcb2f9a6145fe2a6093694565b51c7d7ce1449bc50`;
actual installed librarySHA256
`e8f5e387b1a81489313edbe2c2f8ddb5161514f3420d2b1651c5a40aadb0e293`.
Import retained12582; deliberate cold restart created13480. The Planner appeared
while work was active. Log timestamps show full dispatch0ms and context dispatch
4ms; background calculation3550ms. Cancel was tapped during an active selected-
sight context calculation, before its normal completion time. The cancelled
Almanac remained empty with Export disabled after waiting another4s. Close and
Android Back during separate active calculations returned to a responsive Plan
workspace with PID13480 continuous. Files `planner69-progress.png`,
`planner69-cancel.png`, `planner69-cancel-almanac.png`,
`planner69-close-active.png`, `planner69-back-active.png`,
`logcat69-planner.txt` retain evidence. The initial progress ellipsis rendered
incorrectly; the full regression run caught the same UTF8-literal defect.
Six suites passed, one failed; the encoding repair's targeted check passed.

`Sights-before69.xml` and `Sights-after69-cancel.xml` both hash to
`030d4193395b4cb8bddce684d00fb1477c00e56bc0661a70f2af81cd9bcc1020`,
also identical to the pre65 reference. These tests did not change observations.
Remaining Planner families and final candidate lifecycle acceptance are open.

### 2026-09-27 22:40–23:05 BST: fractional Planner and independent references

On installed runtime70 (`761e83c`), the selected Sun2 context used DR
43.2366916666667,-77.533415 and manual 2025-07-20 17:16:33.987 UTC,
motion off and eye height 2 m. Entering 33.987 switched the time source to
Manual. The actual exported `celestial-reports/planner-fraction70.csv` has
175 rows, 25 hourly instants, seven bodies, nine columns, and every epoch ends
in 987000 microseconds. Independently copied SHA256
`9b472d65b2f29319bfaf00890d19596deb881d10519d56b1da0b5b94f52ce8d8`;
Sun Hc67.276152° and Dec20.512843°. Portrait swipes reached the final Polaris
2025-07-21 17:16:33.987 and all fields. Landscape Hc/Zn were still clipped
below the viewport on runtime70; the next layout requires retest.

Disposable `planner-worksheet65.csv` overwrite refusal used explicit No,
confirmation Back and browser Back. Independent post-action SHA256 remained
`fdf48d51f899d30a29187d0535cbeabf306f8890c5a3067473d5da9b66afc0d3`.
Explicit Replace/write remains pending. Public USNO JSON and hash manifest are
under `validation/android-usno-20260927/`. The fractional API request echoed
.987 but returned the preceding whole-second values; the independent check
therefore brackets at seconds33/34 and interpolates. Executing
`test/android_usno_reference.py` against the physical CSV passes Hc and Dec
for Sun, Moon, Venus, Mars, Jupiter and Polaris within0.1 arcminute. Saturn is
below the service's visibility threshold. GHA/Zn use different frames/time
conventions and are reported but not asserted equivalent. These data are
public references, not a plugin self-comparison.

Actual corrected Ho67.1934 noon input gave43°19.1667′N versus independent
90−Ho+worksheet Dec20.5129° =43°19.1700′N (0.0033′). Actual Polaris Ho43.404855°
gave43°14.2044′N versus independent DR43°14.2015′N (0.0029′). Ho91° reported
Invalid Altitude and Back dismissed it. Southern and below-horizon branches
remain untested. Evidence `planner70-*.png`, physical CSV and public reference
files are retained; these focused passes do not complete P01–P08.

### 2026-09-27 23:09–23:18 BST: resumed build71 layout acceptance

The prepared `8ea2e0b2b918805bb2a902fcebeb21a9e05329ed` archive hash is
`dce95a559ad4875160d60a88e4d15d403cc39f2ebe50144799d77a022e1e5e25`.
Before replacement `before-resume71.tar` captured the installed library.
The attempted absolute-path additions to that tar failed, so it is not a
profile/Sights backup; the earlier verified original backups remain intact.
After the workflow, `Sights-after-resume71.xml` independently matched the
pre69 snapshot byte-for-byte (SHA256 `030d4193395b4cb8bddce684d00fb1477c00e56bc0661a70f2af81cd9bcc1020`).
The profile was separately retained as `profile-after-resume71.conf`.
Real Plugin Manager Downloads import displayed success;
independently read `manPlug/libcelestial_navigation_pi.so` SHA256
`48142cf06de4d5c50c25cf77ebdfab8aaeae211004729043e12a57e6a1b340c7`
matches the intended package. PID14136 survived import acknowledgement and
Settings close. Deliberate force-stop/cold launch created PID16493, and the
chart and four saved observation cards opened visibly.

At font1.15, the Bodies landscape viewport contains the complete first Venus
card, unlike runtime70. Real swipes moved through the list to final Alioth,
including its magnitude, score and complete recommendation reason; fixed Sort,
Direction, selection and Create sight remained visible. A stationary final-card
tap selected Alioth and enabled Create sight; that action opened the observation
editor with Alioth as the body. Cancel returned without saving the disposable
sight. The Almanac landscape scrollbar reached its final Polaris 2026-09-28
22:13:04.202 row with GHA, SHA, Aries values, declination, Hc and true azimuth
visible. At temporary font1.3 the complete first body card and final Polaris
almanac fields remained visible. Screens `resume71-{bodies-portrait,bodies-
landscape,bodies-last,body-selected,create-preview,almanac-last,font-bodies,
font-almanac-real-last}.png`. The current Now-based instant changed between
opening contexts; no cross-screenshot numerical equality is claimed.

Two desktop enabled-display failures from the earlier combined eight-case run
were investigated using the tests' required separate fresh processes on actual
DISPLAY=:1: `FindBodyUi.*` passed (54.807 s) and `FixUi.*` passed (0.286 s).
The combined run had first executed a worker test without wxApp and a Lunar
UI test before Find Body; its missing Hide Time and later SIGSEGV are retained
as an invalid combined-harness result, not erased or asserted to be a desktop
runtime regression. Exact logs/XML: `desktop71-{find,fix}-alone.*` and
`ui71-desktop-display.log`. All seven CTest checks had passed earlier on build70;
the isolated build71 runs here cover only these two GUI families.

Physical context showed latitude `-2e+01` for selected Fiji DR −20°. This is
an exact value but poor navigator-facing text. An Android-only decimal-display
change and explicit −20/100 boundary assertions now pass the Android UTC/angle
boundary script. That source change is not in the installed build71 and needs
new packaging and physical retest. Full P01–P08 and the wider function matrix
remain incomplete; no publication is authorized.

### 2026-09-27 23:21–23:31 BST: committed build72 import and planned sight Save

Commit `61b00666ae15609a16cab4cf35c49b0e2abd5256` includes the Android
ordinary-coordinate decimal display correction and boundary assertions. The
ARM64 archive `celestial_navigation_pi-2.8.13.0-android-arm64-16-android-arm64.tar.gz`
hashes to `3ab6eae5ac6c2cbaf3eedf166b55d5800c9ab5db2fa89afef6a75d0a6c99130a`.
Android and desktop builds, package creation and the Android UTC/angle boundary
script passed (`build72-android-final.log`, `build72-desktop.log`,
`package72.log` and boundary logs). Plugin Manager imported the exact archive;
the installed library SHA256 `d8e12dc343daf5d51bfa4a0130df4e23b7160bca4c8e110250f4bc97d67800ff`
matched its payload. PID16493 survived hot import. A selected Fiji DR latitude
now appears as `-20` rather than `-2e+01` in physical Plan context.

Selecting the pre-existing Sun2 observation transferred its exact DR
43.2366916666667,-77.533415 and 2025-07-20 17:16:33 UTC into Plan. Typing
33.987 seconds switched to Manual and gave 17:16:33.987 UTC in context and
Bodies. Selecting Venus and Create sight opened the editor with Venus and the
same fractional instant. I selected the centre limb, entered measurement
47.7667°, and saved. Independently read `Sights-after-planned72.xml` has five
observations, including Venus with `BodyLimb=1`, `Milliseconds=987`, the exact
date/time, measurement and DR coordinates; the original four attribute sets
were unchanged. The file SHA256 is
`e8fece5a0c5493397fc22154d8f2648fbd42b5ad7e822ac89beadde3706bb93e`.
A deliberate force-stop/cold launch created PID18454, and the Venus card with
the fractional timestamp and the other four records were visible. I selected
and deleted only the disposable Venus sight through the UI; this action showed
no separate confirmation. The resulting independently read file hashes exactly
to the pretest four-sight snapshot:
`030d4193395b4cb8bddce684d00fb1477c00e56bc0661a70f2af81cd9bcc1020`.
Screens `resume72-*.png`, both XML snapshots, import/build logs and post-run
crash/ANR checks are retained. Font scale1.15 and autorotation were restored.
This closes the planned-sight Save/cold-reopen example, not the remaining
Planner sorting, filtering, sky or export flows.

### 2026-09-27 23:33–23:40 BST: physical time-tagged moving observer

On the installed `61b0066` build, selected Sun2 supplied DR
43.2366916666667,-77.533415. Planner Now resolved to 2026-09-27
22:34:02.431 UTC. I enabled Time-tagged moving observer and entered true
COG90°, SOG10 kn; physical Context showed both values and Results ready.
Events showed nautical dusk at 00:21:39 UTC with observer
43°07.4875′N, 082°35.4195′W, and sunrise at 11:12:50 UTC with observer
43°12.4449′N, 080°07.2713′W. Independently calculated positions using
PROJ `geod +ellps=WGS84` from the selected DR, course and signed elapsed
times are 43.1247889984,-82.5903826194 and
43.2074139759,-80.1212016611. WGS84 inverse distances to the displayed
coordinates are 4.698 m and 1.089 m. Exact method/output is retained in
`planner-motion73-geod.txt`, with `resume73-{motion-final,events-on}.png`.

Planner Close/reopen preserved checked motion and 90/10 inputs while Now
refreshed its reference instant. Turning motion off restored each visible
event observer to fixed 43°14.2015′N, 077°32.0049′W, matching the DR; sunrise
became 11:02:29 UTC in that stationary run. The saved Sights.xml after all
these changes remained byte-identical to the four-sight pretest snapshot
(`030d4193395b4cb8bddce684d00fb1477c00e56bc0661a70f2af81cd9bcc1020`).
Screens `resume73-{reopen-lower,motion-off,events-off}.png` retain the mode
comparison. PID18454 was continuous.

A rapid two-tap page-selection attempt yielded a black Planner surface for
several seconds; Android Back returned to the workspace with PID18454
continuous. A later separately observed menu selection did not reproduce the
condition (`resume73-{context-return,context-wait,back-black,menu-repro,
context-repro}.png`). Its cause remains unconfirmed and warrants repetition
in cross-cutting acceptance. This focused P07 check does not cover alternate
context sources, reference times, invalid COG/SOG, or other Planner pages.

### 2026-09-27 23:40–23:42 BST: physical Almanac CSV Replace

The real Planner Almanac showed 175 rows from the selected context's Now
instant 2026-09-27 22:37:49.441 UTC. I retained the existing disposable
`celestial-reports/planner-worksheet65.csv` before mutation, SHA256
`fdf48d51f899d30a29187d0535cbeabf306f8890c5a3067473d5da9b66afc0d3`
(`planner-worksheet65-before-replace74.csv`). Through Export CSV, the custom
OpenCPN file picker, the existing filename selection, Save, and the explicit
Replace button, I wrote the current result to that same path. The app returned
to responsive Almanac with PID18454 continuous. Directly read device bytes
(`planner-worksheet65-after-replace74.csv`) hash to
`bad4a535a2ad738f664cd7e8ddcdd4b547aedf54eff6f0c32ca78b0914d785be`.
Independent CSV parsing found 175 data rows, seven expected body identities,
25 unique hourly instants, and nine columns. The first Sun Hc2.890668°
matches the on-screen 2°53.4401′ and its UTC retains .441; the final Polaris
record is at 2026-09-28 22:37:49.441 UTC. `Sights-after-export74.xml` remains
byte-identical to the four-sight pretest file. Screens
`resume74-{almanac,picker,reports,existing,confirm,replaced}.png` retain the
whole UI path. This verifies Replace for this disposable file and context;
other export destinations and failure cases are still open.

### 2026-09-27 23:43–23:55 BST: Horizon viewport failure and corrected retest

On runtime72 the fixed observation/calendar consumed most of the native
Horizon sheet, leaving approximately90px for its nested lower scroller.
The first repair, ea2bf9c, put the complete form into a wxScrolledWindow;
AndroidSurface already supplied another viewport, and physical build75
collapsed the inner form to roughly28px. This candidate was rejected despite
successful compilation and import. Its archive SHA256 is
1654631c5848e12d0ef5aefdc15f77f1401e4feaf4524b193c95381b78eea394,
library8541e1d9282ac14ac7e9719256b932556aee64682197bf8781df4bfc6ccda156;
resume75-fixed-horizon.png retains the failure. Full CTest passed54.10s on
that source, which did not establish tablet layout acceptance.

4145edc keeps Android controls directly in the complete content sizer so
AndroidSurface owns the single viewport; the desktop scroller is preserved.
Android and desktop compilation passed. The actual-display isolated
HorizonEventUi suite passed181ms (desktop76-horizon-display.log/xml).
The first sandbox display attempt failed wxEntryStart and is retained;
it was not counted as an executed GUI test.

Build76 archive SHA256
a26aae6e220b5ec43de2e0586630ea7e50a116fe953bcfbb0cffc21bfbf6ad0a
was imported through Plugin Manager from celnav76-4145edc.tar.gz. Independently
read installed library SHA256
b69a03bff16ab8034b72e6087a8c2959e44a11aaa5b663bf2d82c5fb6b0cc6b5
matches the payload; hot PID18454 was continuous. Real swipes now reach all
bearing inputs, weather, horizon quality and the entire final explanation in
portrait and landscape (resume76-{horizon-open,bearing,conditions,final,
landscape-real-final}.png). Font setting1.3 was exercised but no visibly larger
Qt text was established; do not count that as rendered-font scaling acceptance.
Back returned to workspace; Sights-after-horizon76.xml remained the exact
four-sight snapshot. Font1.15 and autorotation restored.

### 2026-09-27 23:56–2026-09-28 00:00 BST: Sunrise Save and cold persistence

On build76 I selected Sunrise and typed 11:02:29.987 UTC on27September2026.
Manual typing left System UTC capture selected, so I explicitly selected
Other manual entry before Create Event. Automatic provenance switching is
not verified and this build does not implement it. Default inputs were
time uncertainty2s, eye2m, temperature10C, pressure1013hPa, clear horizon,
refraction uncertainty10′ and no bearing. Actual Save produced five cards;
Sights-after-sunrise77.xml independently records Type3/HorizonEvent0/Sun,
upper limb, Milliseconds987, the explicit source and all these inputs.
The original four attribute sets were preserved. SHA256:
e6864235c036673e7e6b1f9a024dba6d850afca0249f11a130c4bf4fbfec82f1.
No DR location was entered; this is persistence evidence, not an independently
verified sunrise position calculation.

A deliberate cold restart at23:58BST created PID20871. The five cards and
fractional timestamp were present; Edit reopened the exact date/time/source.
Unchanged Save produced byte-identical Sights-after-sunrise77-reopen.xml.
After actual scrolling to the action buttons, Delete removed only the
disposable sunrise. Sights-after-cleanup77.xml exactly matches the original
four-sight SHA256030d4193395b4cb8bddce684d00fb1477c00e56bc0661a70f2af81cd9bcc1020.
Screens resume77-{cold-five,reopen,delete-controls,cleanup}.png retain the
workflow. PID20871 stayed continuous after the intentional restart. Full
crash77.txt, lastanr77.txt and meminfo77.txt retained. Bearing/branches,
alternate conditions, invalids and edited Cancel/Back remain open.

### 2026-09-28 00:07–00:12 BST: native Horizon input/provenance correction

48bcd97 adds Android-only native spin notifications, an owned coalescing
preview timer, and manual provenance when UTC fields/calendar change. Capture
current UTC suppresses these manual notifications while populating fields.
Android/desktop compilation and isolated actual-display HorizonEventUi passed
(184ms). Package78 archive SHA256
d30ea6b846ffca887405260990d27cfea69752eb9cc5445979a9098e75fdc7cc
was actually imported through Plugin Manager. Installed78.so independently
hashes to8cd5c9efcc0fb9b23cde306d18ad1a50f8580f10defaadcabbf6cf283b827b09,
matching its payload; PID20871 continuous. Toolbar ordering changed during
hot import; an unintended Dashboard toggle was immediately restored before
opening CelNav at its observed new location.

Typing hour11 automatically changed System UTC capture to Other manual entry;
Capture current UTC restored23:10:36.509 and System UTC capture. Selecting
calendar28September changed source back to Other manual entry. With magnetic
bearing90°, typed east variation+5° immediately produced95° true; typed west
deviation−2° produced93° true, matching independent90+5−2 arithmetic. Choosing
Hazy or indistinct horizon changed the default altitude uncertainty from10′
to20′ and added the haze warning. Actual final swipe showed both position
branches and the entire explanation/warning. Those geographic branches have
not yet been compared to an independent horizon reference.

Android Back discarded the unsaved new event directly, without a confirmation
sheet. This is not the desktop close-veto confirmation path; screenshot name
resume78-discard-prompt does not establish a prompt. Independently read
Sights-after-back78.xml retained the exact original four-sight SHA256
030d4193395b4cb8bddce684d00fb1477c00e56bc0661a70f2af81cd9bcc1020.
Screens resume78-{manual,capture,date-manual,variation,deviation,haze,
discard-prompt}.png retain actual outcomes. Numeric keyboard Back briefly
left the host action-bar region black while the complete plugin form remained
visible/responsive; the bar restored on the next edit. This rendering symptom
is retained, not claimed resolved. Full bearing Save/reopen/chart/independent
geometry and other Horizon variants remain open.

### 2026-09-28 00:13–00:21 BST: Sextant layout candidate rejected

On runtime78 the Sextant check page exposed clipped weather/IE labels and
numeric values, a one-line repeat table and multi-column profile inputs.
Actual Predict distance for default Sirius/Vega at boat53.1795133666667,
−2.85813505, 2026-09-27 23:13:07.829 UTC explicitly reported one or both
bodies below the usable horizon; no reading/profile was saved. A final swipe
showed the full baseline explanation (resume79-{sextant,prediction}.png).

ca992bf stacked Android groups and supplied model-backed selectable repeat
cards. Android/desktop compilation passed; actual-display isolated LunarUiSmoke
passed6397ms, with the existing missing testdata panel-icon warning retained.
Archive79 SHA2564e540c52add4938767a69a77894d0ebb0bd0020c204aa908167fb2a1e8a5bb22
was actually imported; installed79.so SHA256
285f89de4bbe5c4c2bd353054e16aca831d0f052717f5da12ed87fd79f8ca775 matches
the payload, PID20871 continuous. Physical first/body screenshots showed
that nested sizer groups retained narrow desktop width flags despite their
vertical orientation: coordinate text was severely clipped. This candidate
fails layout acceptance; cards/profile workflows were not claimed executed.
Screens resume79-stacked-{first,body}.png retain this failure.

2500641 expands nested labelled groups and removes vertical stretch weights;
its Android compilation failed due mixed pointer types in an initializer list
(build80-android.log), while desktop compiled. 5b8267c uses explicitly typed
sizer pointers. Build81 and physical retest are the next gate; none of the
Sextant layout, numerical or persistence acceptance is closed by these builds.

### 2026-09-28 00:24–00:35 BST: Sextant repeats/profile, uncertainty failure

Runtime81/5b8267c was imported through Plugin Manager. Independently read
installed81.so SHA2561dfbda34c03d89a86b1e25a533ebbb7ec686d6d9565f76cb6b0290be00052c05
matches its payload; archive SHA256
a39d7c365925c8c92ed065effd93c92a22cbdb95ec94e7ad7313f892e328ce1f.
PID20871 stayed continuous. The width fix exposes complete coordinate, body,
weather, observed and profile inputs (resume81-width-{first,bodies}.png).

At observer53.179512700000004,−2.85813665 and fixed UTC
2026-09-27 23:24:33.746, Deneb/Vega centre prediction was23°50.0312′
(23.833854159275464deg), with apparent altitudes60°29.9715′/38°11.5544′
and an explicit equal-altitude preference warning. This astronomical
prediction has not yet been independently referenced. Actual body catalogue
selection/scroll is retained in resume81-star-{menu,scroll}.png.

With raw observed angle equal to prediction and typed IE+1.50′, Add repeat
produced complete model cards: After IE23°48.5312′, residual to add+1.50′,
uncertainty±0.20′. Independent subtraction confirms these values. Two actual
repeats and saved disposable profile RESUME81-Disposable-Sextant/TEST81
are shown in resume81-{repeat1,repeat2,profile-name,profile-ready,profile-saved}.
Independent config-after-sextant81.conf records one point
23.80885416,1.5,0,2 with repeatability0. No original profiles existed in the
before snapshot; original sights remain byte-identical SHA256
030d4193395b4cb8bddce684d00fb1477c00e56bc0661a70f2af81cd9bcc1020.
Landscape final swipe shows the complete summary/explanation
(resume81-landscape-final.png); portrait was restored.

The saved point incorrectly reports uncertainty±0.00′ despite both entered
±0.20′ errors. This is a numerical failure, not accepted profile precision.
Android-only refinement was designed before coding: the independent formal
weighted-mean error bounds the existing scatter estimate. Reference examples
include equal0.2′ repeats→0.1414213562′, unequal0.2′/0.4′→0.1788854382′,
and ±1′ scatter dominating the measurement floor. Runtime1e0aae4 implements
this guarded correction; actual Android compilation and four host tests of
the Android engine branch pass. Tablet profile rebuilding remains pending.
Desktop numerical code remains unchanged.

Removing selected repeat2 leaves one complete card. Attempting Build/save
with one gives explicit at-least-two validation (resume82-before.png), Back
dismisses it with the saved profile retained (resume82-one-rejected.png).
Removing the final repeat shows No repeat readings added, preserving the
saved profile (resume82-no-repeats.png). Two Back presses return safely to
chart, PID20871 unchanged. Cold profile persistence remains pending.

### 2026-09-28 00:36–00:44 BST: corrected Sextant profile verified

Runtime1e0aae4 package82 SHA256
5837fde94eaeecce5c42bb9f9faeeb8d4dd141437c0dfe43f59c327965070288
was actually imported. Independently read installed82.so SHA256
08480fe32318ac41f6943b067b3c23bca9b2d1215d7bbacb25e65339ef0754ec
matches payload, PID20871 continuous. The retained observer/time/pair and
IE+1.50′ were re-entered. Prediction/raw and both repeat cards agree with
runtime81. Rebuilding the same named profile replaces it, leaving Count1;
config-after-sextant82.conf independently contains
23.80885416,1.5,0.1414213562,2. Display±0.14′ agrees with independent0.2/√2.
The old profile was not changed by loading/importing; only explicit rebuild
changed it. Screens resume82-{prediction,repeat1,repeat2,profile-corrected}.png.

Deliberate cold restart created PID24643. Actual reopened final fields show
the same name/serial, loaded point and ±0.14′ correction, with no unsaved
repeat cards (resume82-profile-cold-final.png). Saved config-cold-sextant82.conf
retains the point. Sights-after-sextant82.xml SHA matches original four.
Complete crash82.txt contains no fatal newer than historical27Sep17:19:57;
lastanr82.txt reports none since boot, meminfo82.txt retained. First cold
page swipes before layout settled did not move; later actual swipes reached
final controls. No claim that every rapid-open gesture succeeds.

Full eight CTest checks passed53.76s, including host execution of the Android
engine branch; isolated actual-display LunarUiSmoke passed6371ms with retained
missing testdata icon warning. These are not all-platform final CI acceptance.

Automatic approval review rejected transmitting the exact boat location/time
to USNO. No bypass or retry with those inputs occurred. A safer independent
reference was retrieved for public Greenwich51.4779,0 at2024-06-21 22:00 UT1.
USNO Deneb/Vega apparent-centre separation from published Hc, Zn and refraction
is23.8357759454deg; physical entry/comparison remains pending.

### 2026-09-28 00:46: Lunar layout build gate

Android candidate83 failed compilation because newly written card styling
called a nonexistent namespaced helper. Build83-android.log retains this
failure; no package/import occurred. Candidate84 calls the established
CN_StyleAndroidControls helper and Android compilation passes. Session
selection and residual cards plus pair-planner full-field cards are pending
physical validation, and remain unaccepted. Selection changes clear Android
candidates and disable saved-result actions so stale results cannot be saved.
Desktop layout/numerical branches retain the original behavior.

### 2026-09-28 00:54–01:09 BST: physical lunar cards and public references

Runtime84/source7b34fd4 was actually imported through Plugin Manager. Archive
SHA25652631432c084c8a3d6a36cc785df5ce7bf94ecc891deb7189d55b3dc0a8dbfcb;
independently read installed84.so SHA256
3b0fd4faea43a345912ba39c371f38a07563cd7c9d8cefb1247d837ca8715d93
matches payload. PID24643 remained continuous, xGRIB/xWeatherRouting enabled.
Android and desktop compilation passed; isolated actual-display LunarUiSmoke
passed6336ms, retaining the missing testdata icon warning.

Session observation cards expose the complete Fiji Moon–Saturn sight. Clear
changed the card to Select and reported no observations selected; Select
visible restored one selection. Actual final swipes expose all search, robust,
bias and motion controls, Solve, empty candidates and disabled Save. Solve
with one observation gives explicit at-least-two validation and safe Back.
Screens resume84-{sequence-first,clear,visible,one-invalid}.png. No session
worker started here; multi-observation/candidate/persistence acceptance is open.

Public Greenwich51.4779,0,2024-06-21 22:00:00 UTC was visibly entered in
resume84-greenwich-input.png. Actual Calculate pairs exposes all11 fields as
complete cards. Real swipes reached final Betelgeuse including Magnitude0.5
in portrait(resume84-pair-final.png) and landscape(resume84-landscape-final.png).
The screenshot resume84-deneb-reference.png contains Vega, not Deneb;
resume84-deneb-complete.png contains Deneb. Explicit transcription, independent
USNO22:00/22:05UT1 responses, hashes and comparison script are committed under
validation/android-usno-greenwich-20240621. Spica/Vega/Deneb distance errors
0.07947′/0.00929′/0.03590′ and geometric Hc errors below0.001′ satisfy0.1′.
Rate and0.1′ timing sensitivity agree within one-decimal display tolerance0.051.
Other fields/rows/providers are not independently accepted by this subset.

Latitude91 rejects and clears every old card; Back returns safely to the form
(resume84-pair-invalid{,-cleared}.png). Date2024-02-30 and hour25 reject with
explicit UTC-format validation(resume84-pair-invalid-{date,time}.png). Changing
system font scale1.15→1.3 while this dialog was open did not visibly resize it;
resume84-landscape-large.png is not proof of larger rendered text. Reopen/cold
font acceptance remains open. Long scrolling reached the inputs again.

Separate Sextant public reference: actually entered the same Greenwich/UTC
fixture, selected Deneb/Vega centre contact, pressure1013hPa/temp10°C/IE0.
Prediction23°50.1355′, exact populated observed23.835591672966757deg;
apparent altitudes44°58.0764′/60°15.6763′. Published USNO Hc minus refraction
and Zn yield23.8357759454019deg, error0.01105635′ within0.1′. Screens
resume84-sextant-{public-input,deneb,usno,usno-full}.png. The swipe starting
on a numeric control opened its keyboard; Back dismissed it safely. This
validates one apparent star centre pair, not Moon limbs or all contact modes.
No repeat/profile or sight was saved in these84 checks.

### 2026-09-28 01:12–01:17 BST: cold session failure reproduced

Both original backup hashes were reverified. Four public USNO-derived
Moon–Spica/Vega records at22:00/22:05UTC were appended to the exact original
Sights.xml while the host was stopped. First read raced command completion and
failed byte equality; no launch occurred. A subsequent complete read proved
exact original bytes plus four test records, eight total, SHA256
e12b4e45ff37ecd8a8bd6a90c9536aa675b0ebb9ef0e84b24584faf717693ab7.
The app then cold launched asPID28114. Public fixture records are independently
staged, not claimed entered/saved through the observation editor.

Font scale1.3 after cold startup visibly enlarges text (19pt versus prior18pt).
All five lunar cards, selection controls and full form remain touch reachable.
Select visible followed by Deselect original Fiji leaves four coherent public
readings, earliest selected DR51.4779,0. Actual known-position mode and search
±0.25h/robust on/bias off/motion off were selected. Solve returnedstd::exception,
no candidate, Save disabled; PID28114 survived. Screens resume84-session-
{fixture-first,public-selected,mode-menu,known-form,known-solve}.png.
This fails lunar session acceptance. Source diagnosis: deferred Android
Recompute leaves cold-loaded snapshots without an ephemeris callback.

### 2026-09-28 01:20–01:30 BST: session preparation repair and saved-report failure

Runtime85/source03495ff was imported through Plugin Manager in PID28114.
Archive SHA2567d097b30b0ed5b7b82ba53991246149e03063e7b51716ba2c57d10f54e57e123;
independently read installed85.so SHA256
944d475feb7dffcbee70ca8dee4556cd4a9505009c13fafda51649469eae10f4
matches its payload. Android/desktop builds and isolated actual-display
LunarUiSmoke passed (6444ms). Other plugins remain enabled.

The four cold-loaded public records now solve at known position51.4779,0,
search±0.25h, robust on, bias/motion off: additional correction−0.4s,
angular RMS0.23′, weighted RMS0.47 and time sigma1.8s. Full four residual
cards expose both epochs and shared Moon readings; largest absolute distance
residual0.49′. Screens resume85-session-{worker,known-final}.png.
The fixture is derived from independent published apparent altitudes/azimuths,
with finite refraction/contact convention differences; it is not an exact
plugin-generated synthetic observation. Cancel on the solution-name sheet
preserved the four original observations and original saved solution;
Sights-session85-cancel.xml retains the independent readback.

Saving RESUME85-USNO-Known added a disposable solution, but strict XML parsing
of Sights-session85-saved.xml failed: four labels contain forbidden &#x13;
references where the Unicode Moon–star dash should be. The pinned Android
wxString::ToStdString narrows that dash to U+0013. This is a persistence
failure, not accepted save/reload. Retain the bad file, remove only that test
solution while stopped, and retest explicit UTF8 labels on the next build.
Known-position display currently says infinite position uncertainty although
position was held fixed. Android presentation and input-change invalidation
are included in the next repair; physical acceptance remains pending.

Actual joint mode with the same four readings and search±0.25h returned
'No converged joint solution in the selected time interval', Save disabled,
PID28114 continuous (resume85-joint-final.png). This remains a failed joint
fixture, requiring diagnosis; do not report joint session acceptance.
The screenshot resume85-session-known-solve.png missed Solve; only subsequent
observed button activation and result screenshots establish the solve.

### 2026-09-28 01:35–01:54 BST: valid session persistence on runtime86

Stopped the host and removed only the named invalid RESUME85-USNO-Known
solution. Strict XML readback verified all eight observations, global clock
and all three original saved solutions unchanged (the original archive has
three solutions, not one). Cleanup readback SHA256
eef6ca9af46748ba7905a5671c9c9628c5ef2dca1499f878de44f7b200efb18c.

Runtime86/sourced523bb2 was actually imported; installed86.so SHA256
5d3f148cd8337828d73cefda86ee8ada08467d61592a2304e5ea2dd3b9e10182
matches payload. Archive SHA256
18ce78b1c4d90c27f151a30307f029bdafd2dcd24279f2c8483aeb6db85ed2cb.
Eight CTest checks passed53.49s and isolated LunarUiSmoke passed6387ms.
Cold PID29998 survived import/known solve/edit/save. Actual four public
readings, known-position51.4779,0, ±0.25h/robust on/bias off/motion off again
give−0.4s, RMS0.23′, time sigma1.8s. Unicode dashes display correctly and the
summary says Position held fixed (resume86-known-solve.png).
Editing native SOG0→1 and dismissing its keyboard clears the candidate,
removes residual cards and disables Save with explicit Inputs changed guidance
(resume86-stale-cleared.png). Restoring0 and solving again enables Save.

Explicitly named RESUME86-USNO-Known saves valid strict XML with four input
snapshots and four U+2013 labels. Exact correction−0.44659105661448395s is
within the independent fixture1s tolerance. All eight observation attributes,
clock, and original three solution attributes/text/input snapshots are
unchanged. First comparison of serialized original elements failed due only to
XML indentation tails after appending; verify-session86.log records the correct
semantic comparison. Sights-session86-saved.xml SHA256
c9c68c74c432e07ce111b1597ff5c25dd7843fb2986383856fc3c65894aedc36.

Deliberate cold restartPID30939 reloads the named saved solution, exact four
input trails and report. Actual final content swipe exposes all four residuals
and final shared-reading warning at font1.3 (resume86-cold-saved-{list,final}.png).
Copy report produces Android's clipboard toast; contents not independently
pasted/read back yet. Back returns to lunar tools with continuous PID, and
Sights-session86-cold-view.xml is byte-identical to the saved file. Crash buffer
still ends at historical27Sep17:19:57; lastanr86 reports none since boot.

The read-only viewer incorrectly captions its Close action Save, and its
historical known-position report still prints infinite position sigma. These
are presentation failures fixed in the next runtime, without rewriting old
trails. Source522bb0c's Android convergence repair passes all nine CTest checks
53.70s, including12 Android-compiled session cases and the independent USNO
production integration. It has been packaged/staged but NOT imported or
physically accepted. The next combined candidate retains that repair.

### 2026-09-28 01:55–01:59 BST: saved clock source watch guard

Runtime86/PID30939 Fix exposes all four saved solutions and existing manual
clock in the native choice popup. Actual selection of the public2024 session
against the original2025 Sun sights rejects with N/A, Watch mismatch, disabled
Show fix on chart, and complete explanation that visible sights exceed12h
from the lunar reference. This is the intended guard, not a numerical failure;
no correction was applied. Screens resume86-fix-{first,clock-menu,
stored-clock-result,mismatch-details}.png. A matching-watch positive fixture
and independent applied-fix geometry remain pending.


### 2026-09-28 02:03–02:13 BST: runtime88 joint session and active cancellation

Restored Fix to the existing manual correction after the deliberate watch
mismatch; the original Sphere result returned, including longitude
077°35.4026′W/error0.000554. App-owned Sights readback has the exact prior
c9c68c74c432e07ce111b1597ff5c25dd7843fb2986383856fc3c65894aedc36 hash.
A first ordinary adb cat lacked app access and captured Permission denied;
this was corrected with run-as, not treated as a data mutation.

Runtime88/sourceb416119 imported through Plugin Manager, PID30939 continuous.
Archive SHA256 c580b38197561f5f3a2f1d22c6d20ee477e68a45036d99445b229fef75c66ef1;
independently read installed88.so SHA256
34f2041c8f13b630d10fed7274156b138767cee0ebcec21f09307b26ee21d2c0
matches payload. xGRIB/xWeatherRouting remained enabled.

Four public USNO Moon–Spica/Vega readings, earliest DR51.4779,0, search±0.25h,
joint mode/robust on/bias off/motion off now succeed. Exact saved correction
−3.3278967674503392s, position51.478681216,0.012026328, about0.45NM from the
independent Greenwich position, RMS0.23′/weighted0.467005, time sigma36.8477s,
position sigma5.740NM. Clock within60s/position within3NM/RMS within0.5′ fixture
tolerances. All four complete residual cards and shared-reading annotations
reached by real portrait swipes at font1.3. This fixes the recorded runtime85
failure; the Android repair is not a desktop numerical redesign.

RESUME88-USNO-Joint saves strict XML with four intact UTF8 input trails, each
solverVersion2.8.13.0. All eight observation attributes/global clock and four
prior solutions compare semantically unchanged. Readback SHA256
53349e1c001d4f58776a4096d7de709cf38190b353a07c47972569e1ba3451ac.
Saved viewer has the correct single Close action. Landscape rotation at1.3
retains selected solution, wrapping, actual final residual warning after four
swipes, and Close returns safely. Earlier two-swipes image labelled final
was still mid-report; use resume88-report-landscape-final-verified.png.

Actual robust checkbox off clears candidate/disables Save with Inputs changed;
solving again gives the same displayed correction/position/RMS, expected for
these residuals below the Huber threshold. It does not test robust outliers.
The first attempted wide-span edit missed its field and Back safely closed
the tool; reopened controls explicitly show the default±12h span.

Two fresh wide-span calculations were captured immediately with active
progress 'Testing bounded solution0of9'; Cancel and Android Back were then
applied while the worker was active. Both return safely with empty candidates,
disabled Save and explicit Lunar session cancelled. Observations unchanged.
Saved XML remains byte-identical to the post-save file, PID30939 continuous.
Active-worker meminfo retained: PSS517729KiB/RSS583440KiB/swapPSS310KiB.
Screens resume88-{cancel,back}-{active,return}.png and cancel-summary.png.
Bias/moving fixture, clipboard readback and cold88 acceptance still pending.


### 2026-09-28 02:14–02:18 BST: DUT1 completion dispatch failure and repair

Runtime88 Advanced page shows the entire offline/bundled coverage and stacked
48dp actions at font1.3. No earth-rotation directory/update existed before
testing. Actual Check/download stayed responsive, disabled repeated download
and import, exposed Cancel, but timed out30s instead of installing.
Focused download88-system.log proves IERS HTTP200, 3768836bytes fully downloaded
in about3s, host getDownloadStatus state8/complete and busy-icon dismissal.
This is a plugin event-dispatch failure, not a network failure. Independent
curl of the same public endpoint succeeded and is retained as iers88-official.all
with response headers; it does not substitute for real plugin acceptance.

The pinned wx/Qt loop does not deliver the standalone handler's pending events.
Android repair overrides its virtual QueueEvent with an owned mutex-protected
event queue, drained by the panel's50ms Qt GUI timer. Stop cancels host routing,
stops both timers and discards only owned download events before next transfer;
Close destroys the queue. It pumps no unrelated wx application events. Desktop
path unchanged. Android and desktop compile; physical repaired build pending.
A second88 transfer and visible Cancel are retained; the screenshot says
Download cancelled, but the network had already completed with its events
stuck. This does not establish cancellation during active network work.


### 2026-09-28 02:19–02:25 BST: runtime89 real DUT1 installation, remaining UI defects

Runtime89/source210c8fa imported with continuousPID30939; installed89.so SHA256
9b000cab6238586572680827e01f1782da827a39b1dcc30469f7424e4cbe4675
matches reviewed payload. Archive SHA256
6a2c0fbc59eed5a4efda9c8e3a71ab205abd3df52922da214d5346f74d26157f.
Actual Check/download validates and installs3768836bytes, extends coverage to
2027-10-02, restores actions and cleans the temporary celestial-dut1 file.
App-owned readback iers89-installed.all SHA256
cc80680ec05c91b65e7d02c6068fe0d44dd0998dc880551975092d2d14aa8e18
is byte-identical to the independently retrieved official endpoint file.
Repeated download correctly reports already up to date. Runtime89's event
bridge therefore physically repairs88's false timeout.

Do not accept complete page UI yet: the two-line success/cancel message is
clipped even after another final swipe. Displayed Cancel taps/settled press in
several immediately captured active transfers did not invoke cancelDownload;
subsequent outcome was already up to date. Screens and download89-system.log
retain the failed attempts; filenames containing cancel do not establish a pass.
This page's labels/actions were reparented by AndroidSurface into a content
panel, while its dynamic Rewrap only laid out the original scroller. Next
Android repair measures native font heights, invalidates and lays out the
actual content owner before fitting the viewport. Desktop Wrap unchanged.
Actual repaired Cancel/Close/offline/cold/import-invalid tests remain pending.


### 2026-09-28 02:26–02:35 BST: runtime90 wrapping accepted; native host input blocker identified

Runtime90/bab7e7e installed library SHA256
3e2f5dfbe18f532601e4dfeacca1a826457c3490644a1952a47b7943db300130,
archive72beba0d92d455170b666c8aba774d3ca835a31f4ceef531f4fa7a25690283ab.
At font1.3 the complete offline/coverage text, action buttons and final
already-up-to-date status now fit and remain readable after an actual swipe.
A freshly captured active transfer still ignores the displayed Cancel tap.
Do not count resume90-cancel-* filenames as cancellation acceptance. PID30939
continuous. Retained logcat90-full.txt proves touch enters an extra native
Android Dialog window while the network transfer is active.

Host startAndroidFileDownload unconditionally calls androidShowBusyIcon,
including its background API. Java showBusyCircle creates a non-cancellable
ProgressDialog whose transparent window blocks the plugin controls. This is
the cause of the missed Cancel, rather than the repaired content geometry.
Android-only next repair dismisses this transfer's native spinner using the
existing activity hideBusyCircle method immediately after STARTED; panel owns
progress, timeout and Cancel. Normal host finish still clears its busy state.
A missing JNI method cancels safely with an explicit failure. Desktop path
unchanged; Android and desktop builds pass. Physical repaired91 pending.


### 2026-09-28 02:35–02:48 BST: runtime91 cancellable DUT1 and cold offline records

Committed689e2f6 archive SHA256
2a71e13ebbbd19e009d306b8e881f91818178712bcfea7fa6329d5120e230b9e;
actual imported installed91.so matches payload SHA256
404b00186b4fafafeaf70cb8a4349de06a93dd57059b2c918227bc8aa7e428f5.
PID30939 continuous through import and all warm tests. First navigation after
import used the collapsed toolbar coordinates while the toolbar was expanded;
resume91-advanced.png and first cancel-* images show only Chart, not a test.
Corrected explicit visible toolbar tap opens the real page.

Actual active-network Cancel passes: screenshot shows Downloading and active
Cancel, then readable Download cancelled/unchanged data and enabled actions.
Host log records HTTP200 then cancelDownload77 before DOWNLOAD_DONE. Separate
Close and Android Back also invoke cancelDownload77 during active work, return
to Tools and preserve PID. The Java service may still finish writing its open
unlinked descriptor; routing is detached and there is no surviving temporary
file or installation. Retained download91-close-back.log and screenshots.
Private files listing has no celestial-dut1 temporary files. Installed update
SHA remains cc80680ec05c91b65e7d02c6068fe0d44dd0998dc880551975092d2d14aa8e18;
sight/solution XML remains53349e1c001d4f58776a4096d7de709cf38190b353a07c47972569e1ba3451ac.

Native chooser imports a51-byte invalid file: explicit complete-file/size
rejection. A3755488-byte official table truncated at2027-09-11 is parsed but
refused as older coverage. A first numeric-corruption attempt changed an A
field overridden by the valid B field: it correctly reports already up to
date and does not establish malformed-data rejection. A fresh3768836-byte
fixture with inconsistent first-row year/MJD is actually rejected with IERS
date/MJD mismatch. All refusal explanations are completely readable at1.3
font in portrait and after genuine landscape final swipes. Native chooser
Back and nested file-dialog Back return safely.

Recorded enabled Wi-Fi, disabled it for offline download: clean failure,
actions restored, data preserved. Deliberate force-stop confirms empty PID;
offline cold start changes30939 to5081. Advanced reports installed coverage
through2027-10-02; saved RESUME88-USNO-Joint reloads -3.328s,36.848s uncertainty
and all four version2.8.13.0 trails. First quick portrait swipes failed to move
the report; reopen plus two slower1200ms content swipes reaches the actual
last residual/shared-reading warning (resume91-report-scroll-retry.png).
Copy report followed by Ctrl+V into a disposable unsaved filename field shows
the expected final 'independent repeated measurements' text. KEYCODE_PASTE
and Ctrl+Home did not perform the expected action; no full bytewise clipboard
readback is claimed. Back discarded the draft.

Cold file readbacks match the unchanged update/XML hashes above. Crash history,
meminfo and host log retained as crash91-cold.log/meminfo91-cold.txt/host91-cold.log.
Wi-Fi restored enabled. Android and desktop compile; UTF8 CTest passes0.13s.
Further provider/lunar variants and the remaining function map are pending.


## 28 September 2026, runtime91 voyage output and runtime92 repair

Installed runtime91 source `689e2f6`, library SHA
`404b00186b4fafafeaf70cb8a4349de06a93dd57059b2c918227bc8aa7e428f5`,
PID5081, portrait/font1.3. Public Greenwich51.4779,0, both dates21June2024,
custom title RESUME91-Greenwich-Voyage, calculator safety with all default
content plus increments, Ageton, coverage-direct and altitude tables. One
of each of the five forms; A4/compact/monochrome/duplex. Actual title/date,
coordinate keyboards and final coverage/content/forms controls inspected.
Earlier stale-coordinate/Enter input sequences cancelled an unsaved form;
those are not acceptance evidence. Settled-state screenshots verify the final
request. Full route, band/global, safety and content variants remain pending.

Actual PDF read back: `resume91-greenwich-tables.pdf`,14300193bytes,446A4
pages, SHA505ae0b111840b8a52be8a7990a157ff704a743b4b7677101c1828d3c0d343b0.
Manifest includes60increment pages,46Ageton,307direct,7altitude and5forms.
Refreshed estimate446matches actual (initial452 preceded reducing six form
copies). Nine representative pages rendered; cover, ephemeris, visual aids,
increments, Ageton, dip, direct instructions, sight and watch forms inspected.
Retained USNO22:00fixture gives printed Sun GHA/Dec errors0.02078/0.03420′,
Moon0.05364/0.03262′; all within0.1′. `voyage91-usno-check.txt` retained.
Initial comparison script incorrectly matched the Moon row as Sun and failed;
corrected column parsing checked both rows, rather than ignoring the failure.

Tablet viewer actual typed446/keyboardBack/Go renders final watch form with
Next disabled. ViewerBack returns request. ExplicitNo then confirmationBack
leave SHA byte-identical. First-pages preview has complete text through final
Moon23h row and explicit four-page limit after five actual swipes. PreviewBack
returns safely. PID5081 continuous throughout. Existing voyage-almanac.pdf
untouched: only a newly named disposable file was written.

ExplicitYes replaced the backed-up disposable with A5/booklet/signature8:
224physical pages,595.28x419.53pt,7161028bytes,
SHA1569ba17dd12da5362dcd9669cf1ed6594e05a3cb187f2892c32f58873f02d17.
Imposition pairs8/1 first, but **FAIL**: long page8heading crosses centre
fold into cover; independent full text also shows Moon23h row345°43.1′
missing although present in A4. Therefore booklet/small-paper are not accepted.
Runtime92 Android-only repair wraps bounded ASCII headings and uniformly fits
the complete A4 logical layout to shorter/narrower leaves, retaining all dense
rows rather than reflowing and truncating. Smaller leaves imply smaller print;
zoom/full-size A4 remains preferable for dense reference tables. Desktop writer
unchanged. A separately compiled Android layout regression retains final
REF-059 across A4/Letter/A5, normal/booklet. 18focused almanac checks pass
4126ms (`almanac92-focused.log`), Android/desktop build pass. First Android
compile failed on old-wx FromUTF8(std::string); fixed explicit c_str and retry
passes. Synthetic A5booklet render inspected: bounded heading and all60rows.
**Actual corrected tablet output/rotation and other variants still pending.**
