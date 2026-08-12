# Offline Eclipse Engine

This directory contains the standalone numerical engine used by the Celestial
Navigation plugin's eclipse planner. It deliberately has no wxWidgets or
OpenCPN dependency and performs no network access.

Initial development uses the published Besselian elements for the total solar
eclipse of 2 August 2027 as a terrestrial-geometry reference. Production
events are derived independently from the installed, checksum-verified DE440
offline data pack.

See [DATA.md](DATA.md) for the exact offline pack, integrity checks and storage
budget. The standalone tool can verify a staged kernel with:

```sh
eclipse-cli verify-data /path/to/de440s.bsp
```

Build and test:

```sh
cmake -S eclipse -B build-eclipse -G Ninja
cmake --build build-eclipse
ctest --test-dir build-eclipse --output-on-failure
```

Reference source:

- NASA/GSFC, Fred Espenak, *Besselian Elements for the Total Solar Eclipse of
  2027 Aug 02*.
  <https://eclipse.gsfc.nasa.gov/SEbeselm/SEbeselm2001/SE2027Aug02Tbeselm.html>
