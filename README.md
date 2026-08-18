Celestial Navigation Plugin for OpenCPN
=======================================

Perform sight reductions and plot positions from celestial observations.

This contribution adds:

* a time-integrity panel showing local, UTC, GNSS/NMEA and chrony status;
* an optional persistent show/hide control for the time panel; and
* observed sunrise/sunset horizon events, with optional magnetic or true
  bearing, magnetic variation, compass deviation and uncertainty estimates;
* a fully offline Sun, Moon and sight planner with twilight/rise/set/transit
  tables, Moon phase information, navigational-body sky plot, best pair/triad
  ranking and CSV almanac export;
* a **Generate Almanac…** workflow which creates a route- or position-aware,
  completely offline voyage PDF with hourly Sun/Moon/Aries and navigational-
  planet ephemerides, all 57 navigational stars, sight recommendations,
  lunar opportunities, correction/reduction references and working forms. A
  saved route can be selected in the generator, or the same workflow can be
  opened pre-populated using **Generate fallback almanac…** on an OpenCPN
  route's context menu;
  The generator ranges from a compact Passage Brief to a full global annual
  edition. Its calculator-free safety level enforces hourly ephemerides,
  minute/second and v/d interpolation tables, altitude corrections,
  voyage-specific direct Hc/Zn lookup tables, universal Ageton reduction
  tables, instructions and forms. Optional planning graphs and booklet
  signature imposition are included;
* a time-tagged numerical running fix which advances each observation through
  a COG/SOG motion model to a common epoch;
* a sight-sequence analyzer for residuals, scatter, robust outliers, trend and
  personal bias, plus dedicated noon and Polaris helpers;
* a rebuilt lunar-distance workflow which supports simultaneous or separately
  time-tagged lunar distance/Moon altitude/body altitude readings, searches
  for every matching UTC in a selectable interval, jointly recovers a
  constant watch offset and reference-epoch position, optionally advances a
  moving vessel by COG/SOG, and reports numerical time/position uncertainty;
* coastal sextant tools for vertical-angle ranges, optional bearing/range
  positions, one-angle chart loci and numerical three-object/two-angle fixes;
* a working celestial-body azimuth sight, explicitly distinguished from a
  terrestrial horizontal sextant angle; and
* a completely offline solar-eclipse planner powered by JPL DE440, with event
  discovery through 2100, local C1–C4 circumstances, central-path limits,
  partial-eclipse magnitude contours and optional NASA LOLA lunar-limb contact
  refinement.

The eclipse planner is opened with **Eclipses…** on the plugin's main window.
Its astronomical data is deliberately separate from the plugin binary and
source contribution: the DE440s base kernel and the optional lunar-orientation
and converted LOLA packs are distributed separately. Use the three **Import**
buttons once to copy the checksum-verified files into OpenCPN's private
plugin-data directory. No eclipse calculation or UI action performs network
access. See
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
explains the simultaneous and sequential observation models,
unknown-watch-offset workflow, genuine position ambiguity, controls and
limitations.

The almanac can now be **calculator-free**: after printing, a navigator can
reduce and plot supported sights using the document, sextant, accurate watch,
pencil and plotting tools, without a computer, internet connection,
scientific calculator or external almanac. The cover carries a machine-checked
dependency manifest. See [the implementation and performance note](docs/ALMANAC_GATE6.md).

Maintenance repository:
https://github.com/rgleason/celestial_navigation_pi

Full development repository:
https://github.com/pob220/celestial_navigation_pi

Public eclipse-data downloads:
https://github.com/pob220/celestial_navigation_pi/releases/tag/eclipse-data-2026.1

Original source repository:
https://github.com/seandepagnier/celestial_navigation_pi

Compiling
=========

* `git clone --recurse-submodules https://github.com/rgleason/celestial_navigation_pi.git`
* `cd celestial_navigation_pi`

The three separately distributed eclipse files are:

* [`de440s.bsp`](https://github.com/pob220/celestial_navigation_pi/releases/download/eclipse-data-2026.1/de440s.bsp)
  (required for the eclipse planner);
* [`moon_pa_de440_200625.bpc`](https://github.com/pob220/celestial_navigation_pi/releases/download/eclipse-data-2026.1/moon_pa_de440_200625.bpc)
  (optional lunar-orientation refinement); and
* [`lola64-pa.bin`](https://github.com/pob220/celestial_navigation_pi/releases/download/eclipse-data-2026.1/lola64-pa.bin)
  (optional lunar-limb terrain refinement).

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
separately obtained files. This is a local copy operation and requires no
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
