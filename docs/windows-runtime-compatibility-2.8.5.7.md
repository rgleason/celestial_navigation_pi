# Windows runtime compatibility — 2.8.5.7

Version 2.8.5.7 changes only the locking implementation used to publish DUT1
updates and serialize access to the retained DE440 kernel.

Stock Windows installations of OpenCPN 5.12 and 5.14 can load an older
`msvcp140.dll` from the application directory. Microsoft changed
`std::mutex` construction in Visual Studio 2022 17.10, and code built with
current headers can fail on its first lock when hosted by that older runtime.
The DUT1 update is loaded during plugin initialization, so the incompatibility
could terminate OpenCPN immediately after installing or enabling the plugin.

On Windows, these two locks now use the operating system's exclusive SRW lock.
Other platforms retain `std::mutex`. Both implementations have the same
non-recursive mutual-exclusion semantics, and the guarded data and operations
are unchanged.

The Windows package build inspects the completed plugin DLL and fails if it
imports the MSVC `_Mtx_lock`, `_Mtx_unlock`, `_Mtx_init*` or `_Mtx_destroy*`
entry points. Regression tests exercise concurrent DUT1 publication and the
existing DUT1 and DE440 calculation paths.

OpenCPN should still update its bundled Microsoft runtime because other modern
plugins can encounter the same host-wide incompatibility. This plugin-side
change allows Celestial Navigation to remain compatible with already installed
OpenCPN releases without modifying their files.

## Extension to the 2.9.3 Compact and lunar paths

The first full 2.9.3 CI run (Rick's pipeline 550, commit `16f029f`) built
all 17 non-Windows jobs successfully. Each of the three Windows jobs compiled
and linked the plugin, then failed the existing DLL import check on `_Mtx_lock`.
New ordinary `std::mutex` locks had bypassed the earlier compatibility fix.

The Compact epoch cache, retained Compact engine loader, session-owned DE440
stream and retained Classic lunar callback now reuse `eclipse::Mutex` and
`eclipse::MutexGuard`. The existing wrapper uses native SRW locks only on
Windows; Linux, macOS and Android still use `std::mutex`. The numerical
calculations, coefficient/data files and provider selection are unchanged.
The DLL import check remains enabled for all three Windows build variants.

The vendored Compact provenance records this integration patch and the original
upstream engine hash separately. Concurrent regression cases compare a shared,
evicting Compact cache against uncached calculations and compare parallel
retained Compact, DE440 and Classic lunar callbacks against serial samples,
including retained observer-direction callbacks where available.

Local validation of this extension: the Release plugin and test executable
built successfully; all 55 focused Compact, lunar DE440/boundary, navigation,
DUT1 and lunar-worker tests passed, including the two new concurrency cases.
The Compact package integrity check passed with the integration patch recorded.
Windows DLL imports are verified by the three MSVC jobs in the PR's fresh CI run.

The next run (pipeline 551, `1365a0f`) confirmed that the Windows x64 DLL
has no MSVC mutex imports and generated its archive. It exposed a second,
unrelated validation defect: the x64 script expected version `2.9.2.0` and
emitted a matching stale release URL. The script now derives the expected
plugin/API versions from the checked-out `CMakeLists.txt` and uses that plugin
version in the development Alpha URL. Architecture and API checks remain
strict. Three metadata regression tests cover version bumps, wrong platform/API
values, and missing or duplicate source version settings.
