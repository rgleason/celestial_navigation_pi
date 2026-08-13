# Offline Eclipse Engine

This directory contains the standalone numerical engine used by the Celestial
Navigation plugin's eclipse planner. It deliberately has no wxWidgets or
OpenCPN dependency and performs no network access.

Initial development uses the published Besselian elements for the total solar
eclipse of 2 August 2027 as a terrestrial-geometry reference. Production
events are derived independently from the installed, checksum-verified DE440
offline data pack.

Capabilities
------------

The reusable C++11 library and its small CLI provide:

* solar-eclipse discovery and partial/annular/total/hybrid classification;
* TT/TDB ephemeris evaluation, a modelled TT−UT1 (ΔT), ERFA Earth rotation,
  WGS 84 observer geometry, apparent Sun altitude and azimuth;
* central line and northern/southern totality or annularity limits;
* local C1, C2, maximum, C3 and C4 times, magnitude, obscuration and duration;
* global maximum-magnitude contours generated with marching squares;
* GeoJSON export for independent inspection; and
* optional contact refinement using the DE440 Moon principal-axes binary PCK
  and a compact 64-pixels-per-degree NASA LOLA terrain pack.

All runtime readers are native code in this directory. CSPICE and netCDF are
not plugin dependencies: CSPICE was used only as an independent PCK-reader
validation reference, while netCDF is needed only by the developer-side
`lola-pack` converter.

See [DATA.md](DATA.md) for the exact offline pack, integrity checks and storage
budget. The standalone tool can verify a staged kernel with:

```sh
eclipse-cli verify-data /path/to/de440s.bsp
eclipse-cli verify-pck /path/to/moon_pa_de440_200625.bpc
eclipse-cli verify-lola /path/to/lola64-pa.bin
```

Build and test:

```sh
cmake -S eclipse -B build-eclipse -G Ninja
cmake --build build-eclipse
ctest --test-dir build-eclipse --output-on-failure
```

To exercise the official optional data in the test binary:

```sh
ECLIPSE_DE440_PATH=/path/to/de440s.bsp \
ECLIPSE_LUNAR_PCK_PATH=/path/to/moon_pa_de440_200625.bpc \
ECLIPSE_LOLA_PATH=/path/to/lola64-pa.bin \
  ./build-eclipse/eclipse_core_tests
```

Useful standalone checks include `find`, `path-2027`, `local-2027`,
`local-2027-lola` and `geojson-2027`; run the CLI without arguments for exact
syntax. Detailed validation evidence is recorded in
[VALIDATION.md](VALIDATION.md).

Time interpretation
-------------------

DE440 is evaluated on TT/TDB. Chart and local-result times are displayed as
UT1 using the event's ΔT value. For future dates, the corresponding civil UTC
cannot be known to the second because future leap seconds and Earth rotation
are not predictable. This is stated in the UI; the engine does not disguise a
modelled future UT1 value as exact UTC.

Reference source:

- NASA/GSFC, Fred Espenak, *Besselian Elements for the Total Solar Eclipse of
  2027 Aug 02*.
  <https://eclipse.gsfc.nasa.gov/SEbeselm/SEbeselm2001/SE2027Aug02Tbeselm.html>
