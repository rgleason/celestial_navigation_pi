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
