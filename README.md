Celestial Navigation Plugin for OpenCPN
=======================================

Perform sight reductions and plot positions from celestial observations.

This maintained fork adds:

* a time-integrity panel showing local, UTC, GNSS/NMEA and chrony status;
* an optional persistent show/hide control for the time panel; and
* observed sunrise/sunset horizon events, with optional magnetic or true
  bearing, magnetic variation, compass deviation and uncertainty estimates;
* a fully offline Sun, Moon and sight planner with twilight/rise/set/transit
  tables, Moon phase information, navigational-body sky plot, best pair/triad
  ranking and CSV almanac export;
* a time-tagged numerical running fix which advances each observation through
  a COG/SOG motion model to a common epoch;
* a sight-sequence analyzer for residuals, scatter, robust outliers, trend and
  personal bias, plus dedicated noon and Polaris helpers;
* a rebuilt lunar-distance workflow which clears the observed distance,
  searches for every matching UTC in a selectable interval, quantifies the
  time uncertainty, recovers a constant watch offset, and intersects the two
  accompanying altitude circles to obtain latitude/longitude candidates;
* coastal sextant tools for vertical-angle ranges, optional bearing/range
  positions, one-angle chart loci and numerical three-object/two-angle fixes;
* a working celestial-body azimuth sight, explicitly distinguished from a
  terrestrial horizontal sextant angle; and
* a completely offline solar-eclipse planner powered by JPL DE440, with event
  discovery through 2100, local C1–C4 circumstances, central-path limits,
  partial-eclipse magnitude contours and optional NASA LOLA lunar-limb contact
  refinement.

The eclipse planner is opened with **Eclipses…** on the plugin's main window.
Its astronomical data is deliberately separate from the plugin binary. The
complete runtime data set is kept in `eclipse/data` using Git LFS: the DE440s
base kernel and the optional lunar-orientation and converted LOLA packs. Use
the three **Import** buttons once to copy these checksum-verified files from
the checkout into OpenCPN's private plugin-data directory. No eclipse
calculation or UI action performs network access. See
[eclipse/DATA.md](eclipse/DATA.md) for exact files, provenance, checksums and
storage sizes.

The ordinary navigation planner is independent of the eclipse data packs. It
uses the plugin's bundled VSOP87D, ELP2000 and navigational-star data, works
without DE440 or LOLA, and never requires a network connection. Lunar
distances use the locally installed DE440s kernel when it is present and fall
back to the bundled analytical catalogue otherwise; LOLA is never required.
See the
[offline planning and running-fix guide](manual/modules/ROOT/pages/offline-planning.adoc).
The separate
[lunar-distance and coastal-sextant guide](manual/modules/ROOT/pages/lunar-coastal.adoc)
explains the observation model, unknown-watch-offset workflow, genuine
two-position ambiguity, controls and limitations.

Maintained fork: https://github.com/pob220/celestial_navigation_pi

Upstream maintenance repository:
https://github.com/rgleason/celestial_navigation_pi

Original source repository:
https://github.com/seandepagnier/celestial_navigation_pi

Compiling
=========

Install Git LFS before cloning so the offline eclipse data is checked out as
real binary files rather than small LFS pointer files.

* `git lfs install`
* `git clone --recurse-submodules https://github.com/pob220/celestial_navigation_pi.git`
* `cd celestial_navigation_pi`
* `git lfs pull`

The three eclipse files should then be present at:

* `eclipse/data/de440s.bsp`
* `eclipse/data/moon_pa_de440_200625.bpc`
* `eclipse/data/lola64-pa.bin`

Their sizes and SHA-256 digests are pinned in the adjacent manifests. The
normal celestial-navigation, planning and almanac features do not require
these files. DE440s is required only by the eclipse planner; the orientation
and LOLA files add optional lunar-limb contact refinement.

Under windows, you must find the file "opencpn.lib" (Visual Studio) or "libopencpn.dll.a" (mingw) which is built in the build directory after compiling opencpn.  This file must be copied to the plugin directory.

Build as normally:

* `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
* `cmake --build build`
* `cmake --install build`

After installing or loading the plugin, open **Eclipses…** and import the
three files from `eclipse/data`. This is a local copy operation and requires no
network access.

Tests
=====

* `cmake -S . -B build -DOCPN_BUILD_TEST=ON`
* `cmake --build build`
* `ctest --test-dir build --output-on-failure`

The standalone eclipse engine retains its own independent regression suite in
`eclipse/tests`.

License
=======
The plugin code is licensed under the terms of the GPL v3 or, at your will, later.
