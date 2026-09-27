# Android 5.14 tester installation work

Requested by Paul on 27 September 2026, after the upstream host fix PR
https://github.com/OpenCPN/OpenCPN/pull/5457 was submitted.

- [ ] Provide a simple tablet-side installer/patcher for testing while the
  official OpenCPN Android fix is awaiting review/release.
- [ ] Detect package, version, ABI and signing certificate; support only
  explicitly verified host variants and explain incompatibilities.
- [ ] Verify patch payload/source provenance and hashes before installation.
- [ ] Preserve profiles, charts, user records and other plugins; verify backup
  and recovery before recommending a migration.
- [ ] Physically test the entire tester flow and tarball import on the tablet.
- [ ] Retain precise limitations and a short guide. Publication requires the
  existing explicit approval, separately from creating/testing artifacts.

## Feasibility constraints

A folder-selection patch cannot ordinarily modify another installed app's
private files on an unrooted Android tablet. Android's application sandbox
protects those files: https://source.android.com/docs/security/app-sandbox
An in-place APK update also requires a compatible signing identity:
https://developer.android.com/studio/publish/app-signing

The retained org.opencpn.opencpn.dev 5.14 host is a development APK. Its
original and patched certificates match locally, allowing a verified in-place
update without uninstalling or clearing data. That does not establish that
we possess a production OpenCPN signing key or that stock Play-store APKs
can be updated in place. No root requirement, signing bypass, uninstall or
automatic data loss is an acceptable default tester flow.

Assess a separately installed, clearly identified patched test host with
explicit profile/chart migration if a production-key update is unavailable.
Do not claim this candidate route is implemented or tested yet.

## Agreed delivery on 27 September 2026

Paul agreed to a patched Android OpenCPN APK on pob220 GitHub for testers,
with an unofficial 5.14.1-style version label and only the narrowly scoped
host patch as a functional change. Provisional display version:
5.14.1-pob220-import-fix (check existing versions before final naming).

This replaces the proposed folder-patching interaction. Produce and verify
the APK, explain compatible ABI/package/certificate, test actual installation
and Plugin Manager import, and retain original APK plus profile/chart backup
and restore evidence. Returning to stock is possible but different signing
identities can require an uninstall; do not promise automatic data retention.
Keep official OpenCPN releases and this test build clearly distinguishable.
The new request does not approve publication of the unfinished Celestial
Navigation release or approval of its CircleCI publication gate.

## Candidate built and locally installed

Final fresh-dependency host source
`a4b40b4b4f7701bf2686484bded8b76955b8f6df` is available on branch
android/5.14.1-pob220-import-fix in pob220/OpenCPN-weather-routing.
The master PR includes the narrow runtime fixes; tester branding and build
packaging are on the separate 5.14 backport branch.

APK: OpenCPN-5.14.1-pob220-import-fix-arm64.apk, SHA256
`05abd439cd6ae65eb24044b10458f71341f8df7d32bb35bd16401b6f18ff14e2`;
libgorp SHA256
`78e3aacc1cb74523c8f25a3576199eefc80eeb6e10ff23e5510d3ebbf1f24714`.
Retained under /tmp/celnav-android-20260927/host129-fresh/, alongside
unstripped binary and provenance JSON. This is a local candidate, not a live
download URL. The APK was installed in place without uninstall or data clear.
Android package versionName and native upgrade notice both display
5.14.1-pob220-import-fix; version code 129, org.opencpn.opencpn.dev, ARM64.

The fresh core build used a newly extracted checksum-verified v1.2 support
archive. No shared repaired Qt cache was used. Lunasvg is pinned to the actual
verified source commit and was explicitly built before gorp; baseline core
linking otherwise references its archive without a target dependency. A stale
desktop FindZLIB result was replaced with the NDK ARM64 sysroot library. Full
configure/compiler logs are retained.

All 10 backed-up private plugin/user files matched after the upgrade; only
the opaque 24-byte profileInstalled marker was rewritten. Sights.xml retained
the observation and three solutions identically. The host version upgrade
also reset ShowActiveRouteHighway and ToolbarX; restoration of their prior values
was attempted individually without replacing the rest of the
profile. ToolbarX=4 persisted; ShowActiveRouteHighway returned to 1 at the next
host save and remains under investigation. Do not count that setting as restored. Version and
regenerated font-cache keys are expected startup changes, recorded separately
from navigator data. A same-version final APK update cold-started without a
second version notice. Native tarball import and success acknowledgement
reopened the workspace with PID 20434 continuous.

Private backup paths: private-before-host129.tar, profile-before-host129.conf
and Sights-before-host129.xml in the audit directory. Tar SHA256
`6951afa9f145b0579a6614a970cff9091de415e9b5b646dbdf5c96f613353181`.
Meaningful packaging refusal checks rejected a wrong source SHA, wrong base
APK checksum and manifest changes outside the two version fields before
creating an APK.

Still required before tester handoff: browser download interaction,
original-host return/restore guide and
final audit. Production stock signing compatibility and stock-to-development
profile migration have not been tested; the safe default is to retain stock
and use a separate test package, with explicit chart/profile setup.

The final fresh-host/CI-plugin touch disable and re-enable also passed with
PID 20434 continuous, persisted bEnabled values and actual toolbar/workspace
visibility. The existing My Files REQUEST_INSTALL_PACKAGES permission is
already allowed; no broader install permission was enabled for this test.

## Normal tablet installer test, 18:15–18:18 BST

Opened the final APK from Samsung My Files > Downloads, selected Package
installer > Just once, and accepted the visible Update prompt. Play Protect
reported that it had not seen this developer before. More details exposed
the per-file Install anyway option, which completed the update. Play Protect
was left enabled; no global security setting was changed. The installer
reported App installed and Open launched the host. The package remains
5.14.1-pob220-import-fix / 129 and Sights.xml is byte-identical to the backup.
The host reached the chart and Celestial workspace; PID 23025 reflects the
expected process replacement during APK installation. Retained xGRIB and
xWeatherRouting toolbar icons are present. This verifies a file-based system
installer flow, not a browser download or stock Play-store migration.
Screenshots: tester-package-prompt.png, tester-package-protect-details.png,
tester-package-success.png, manual129-ready.png, manual129-workspace.png
in the private audit directory. Testers may see an unfamiliar-developer
notice with this development-signed APK; do not advise disabling Play Protect.
