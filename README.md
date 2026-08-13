Celestial Navigation Plugin for OpenCPN
=======================================

Perform sight reductions and plot positions from celestial observations.

This maintained fork adds:

* a time-integrity panel showing local, UTC, GNSS/NMEA and chrony status;
* an optional persistent show/hide control for the time panel; and
* observed sunrise/sunset horizon events, with optional magnetic or true
  bearing, magnetic variation, compass deviation and uncertainty estimates;
  and
* a completely offline solar-eclipse planner powered by JPL DE440, with event
  discovery through 2100, local C1–C4 circumstances, central-path limits,
  partial-eclipse magnitude contours and optional NASA LOLA lunar-limb contact
  refinement.

The eclipse planner is opened with **Eclipses…** on the plugin's main window.
Its astronomical data is deliberately separate from the plugin binary: use
the three **Import** buttons once to copy the checksum-verified DE440s base
kernel and, optionally, the lunar-orientation and LOLA packs into OpenCPN's
private plugin-data directory. No eclipse calculation or UI action performs
network access. See [eclipse/DATA.md](eclipse/DATA.md) for exact files,
provenance, checksums and storage sizes.

Maintained fork: https://github.com/pob220/celestial_navigation_pi

Upstream maintenance repository:
https://github.com/rgleason/celestial_navigation_pi

Original source repository:
https://github.com/seandepagnier/celestial_navigation_pi

Compiling
=========

* `git clone --recurse-submodules https://github.com/pob220/celestial_navigation_pi.git`

Under windows, you must find the file "opencpn.lib" (Visual Studio) or "libopencpn.dll.a" (mingw) which is built in the build directory after compiling opencpn.  This file must be copied to the plugin directory.

Build as normally:

* `mkdir build && cd build`
* `cmake ..`
* `cmake --build .`
* `cmake --install .`

License
=======
The plugin code is licensed under the terms of the GPL v3 or, at your will, later.
