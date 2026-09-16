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
