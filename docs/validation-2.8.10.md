# 2.8.10 validation - 23 September 2026

Based on rgleason/celestial_navigation_pi master
`c3900cc037accec67addaa9ac9c26d5ac70035f5` (2.8.9.0). This is a focused
backport, not a merge of the private 2.9 branch. Release version: 2.8.10.0.

## Local results

| Check | Result |
| --- | --- |
| Release plugin and test executable, Arch Linux x86-64 / wxWidgets 3.2 | Built successfully |
| Main headless suite | 207 passed; 6 opt-in GUI tests skipped in this run |
| Numerical audit in UTC, London, New York and Kolkata | 10 tests passed in each separate timezone process |
| CI APT configuration and UTF-8 UI literal checks | Passed |
| Standalone eclipse core | Passed, including a separate run with the verified DE440s kernel supplied |
| All 6 opt-in GUI tests, run in separate display processes | Passed: Almanac, Lunar Tools, Find, Fix, Coastal, Horizon Event |
| Almanac layout at 1080 x 760 and 760 x 560 | All three tabs checked at top and bottom scroll positions; horizontal bounds pass |
| Generated DE440s Almanac | PDF writer test passed; sources page and Sun/Moon daily table rendered and visually checked |
| Local CPack archive | Generated successfully; about 4.14 MiB compressed, with no optional kernel or lunar terrain pack |
| Windows, macOS, Android and other Linux distributions | Require the release branch's CI builds and platform-user testing; not claimed tested locally |

The smallest-dialog check found a pre-existing long Safety level choice
clipping the Content tab. Its choices now say “Self-contained: calculator”
and “Self-contained: paper only”; selection semantics are unchanged.

## Accuracy and fallback coverage

The new navigation tests exercise Sun/Moon/Mercury/Venus selection, independent
archived JPL Horizons planet-centre references, observer geometry, fractional
seconds, and separate dynamical and Earth-rotation times. Changing DUT1 rotates
GHA without changing the dynamical Moon range/declination. Lunar tests retain
the Sun/Moon path and extend supported lunar pairs to Mercury/Venus.

The Almanac tests check geocentric lunar semidiameter, rounded printed Sun
reduction and correction tables, actual source reporting, manual DUT1, and
out-of-date coverage. Missing and invalid kernel replacements explicitly
exercise analytical fallback and recovery when the verified kernel becomes
available again. Dates before modern UTC handling (1972), dates outside the
kernel, stars and unsupported body centres fall back without substituting
planetary-system barycentres for planet centres.

Three 2026 Air Almanac lunar GHA rounding-boundary rows are regression-tested
against the publication's stated UT1/Delta-T convention. Archived USNO online
Moon differences are tested separately as a time-argument diagnostic; no
fitted time offset is applied in production. These checks do not imply every
digit in every printed almanac will agree.

Source tests: [navigation](../test/navigation_de440_tests.cpp),
[Almanac](../test/almanac_de440_tests.cpp),
[GUI](../test/almanac_ui_tests.cpp),
[lunar](../test/lunar_de440_tests.cpp).

## Reproduce

Place the optional official `de440s.bsp` in `eclipse/data/` for the integration
tests only. It is not committed or bundled. Verified SHA-256:
`c1c7feeab882263fc493a9d5a5b2ddd71b54826cdf65d8d17a76126b260a49f2`.

```sh
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release -DOCPN_BUILD_TEST=ON
cmake --build build-linux --parallel 4
ctest --test-dir build-linux --output-on-failure
cmake -S eclipse -B build-eclipse -DECLIPSE_BUILD_LOLA_TOOL=OFF
cmake --build build-eclipse --parallel 4
ECLIPSE_DE440_PATH="$PWD/eclipse/data/de440s.bsp" build-eclipse/eclipse_core_tests
CELESTIAL_RUN_UI_TESTS=1 build-linux/test/celestial_tests --gtest_filter=AlmanacUi.*
```

GUI tests need a working desktop display and must each run in a separate
process. They use test/mocked application state, not the user's working
OpenCPN profile. Normal offline calculations never download data.

## References

- [JPL DE440s kernel](https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de440s.bsp)
- [JPL Horizons documentation](https://ssd.jpl.nasa.gov/horizons/manual.html)
- [USNO 2026 Air Almanac](https://aa.usno.navy.mil/downloads/publications/aira26_all.pdf)
