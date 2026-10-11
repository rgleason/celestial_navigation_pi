# Celestial Navigation 2.9.0: optional DE440s navigation ephemerides

The plugin remains entirely offline. A locally installed, checksum-verified
`de440s.bsp` is used for the Sun, Moon, Mercury and Venus centres when the
sight is on or after 1972-01-01 and within the compact kernel's 1849–2150
coverage. Earlier historical dates use the analytical fallback because an
automatic modern UTC-to-TT conversion is not justified there. Other planets
and navigational stars continue to use the bundled analytical catalogue.
Without the file, or beyond
its coverage, the existing analytical calculation remains available. Neither
the lunar-orientation kernel nor LOLA terrain is needed for navigation.

The supported DE440s calculations share an explicit UTC/TT/TDB/UT1 time model,
retain fractional seconds, and use the sight date's offline DUT1 value when
available. Outside the Earth-rotation table the engine reports its UT1=UTC
fallback; an explicitly supplied almanac DUT1 takes precedence. WGS84
observer-specific light time and parallax are used for the displayed airless
topocentric altitude/azimuth and for lunar position fitting. Geocentric Hc is
kept as a separate quantity. The Moon's selected upper/centre/lower limb and
mean angular radius are handled in the observation correction, not hidden in
the SPK. Venus retains its navigational centre-of-light convention.

This is an ephemeris improvement, not a claim of perfect sight accuracy.
Atmospheric refraction, horizon/dip, clock quality, index error, lunar figure
and terrain, and the conditioning of a position fix still matter. The larger
optional lunar-orientation and LOLA files remain eclipse-only refinements.

The headless test suite checks the supported target IDs, fractional seconds,
DUT1 separation from dynamical time, Moon limb range, WGS84 observer geometry,
the analytical and out-of-coverage fallbacks, and Moon–Sun/Mercury/Venus lunar
pairs. Archived USNO and JPL Horizons responses are under
`validation/engine-lab/references/`. The independent comparisons and an
investigation of a small Moon discrepancy with the USNO online API are in
`validation/engine-lab/DE440S-TIME-DIAGNOSTIC-20260918.md`. The API's
undocumented time conversion was **not** used to tune the production engine.

For a full local regression run:

```sh
cmake -S . -B build -DOCPN_BUILD_TEST=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The 31.2 MiB DE440s file is an optional separately installed data pack and
must not be bundled into the plugin binary or source commit.
