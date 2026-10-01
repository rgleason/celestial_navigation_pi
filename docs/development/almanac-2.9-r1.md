# Astronavigation tables — 2.9 development

Document revision: 1, 22 September 2026. Software target: Celestial Navigation
2.9.0.0 development branch. This note is versioned independently of the plugin.
The released 2.8.9 manuals and Bob's training documents are unchanged.

## Purpose

The new **Compact astronavigation tables** preset creates offline hourly
ephemerides and paper sight-reduction references, without the optional planning
pages or large coverage-specific direct Hc/Zn tables. It is not a reproduction
of Reeds, nor an implementation of its compressed monthly or ABC/versine tables.
Keeping hourly samples avoids introducing a new, unvalidated interpolation method.

## Use

1. Open **Generate Voyage Almanac** and select **Compact astronavigation tables**.
2. Choose the UTC date range. The preset uses global ephemerides; it does not
   require a saved route. The existing one-year range limit remains.
3. Normally leave **Use one DUT1 value for the whole document** unchecked.
   This uses the bundled offline IERS table on covered dates, with reported
   UT1=UTC fallback outside coverage. A checked value overrides every date.
4. On **Content**, retain **Paper only: Ageton** for printed increments,
   reduction and altitude-correction tables. Choose **Scientific calculator**
   for a shorter document without those optional paper tables. Changing these
   options makes the preset Custom; required dependencies are still enforced.
5. **Monthly star data (otherwise voyage midpoint)** controls star-table epochs.
   The compact preset uses monthly tables.
6. On **Forms & output**, choose the paper size and output file, then preview
   or generate the PDF. Review **Sources and conventions** before using it.

## Ephemeris and time sources

The existing 2.9 automatic provider is used, without a separate Almanac engine:

| Quantity | Source |
| --- | --- |
| Sun, Moon, Venus | DE440s body centres when installed and supported for the requested instant; analytical fallback otherwise |
| Mars, Jupiter, Saturn | Analytical in this implementation; the short kernel's planetary-system barycentres are not silently substituted for body centres |
| Navigational stars and Polaris | Stellar catalogue and analytical apparent-place model |
| Aries | Earth rotation and equinox model, not a DE440s object |
| DUT1 | Dated offline IERS values, or explicitly reported zero fallback; optional fixed user override |

Mercury is supported by the shared provider but is not one of the standard
four planets tabulated by this preset. Installing DE440s is optional; generation
does not require an Internet connection. Actual sources are recorded for each
tabulated body, including hourly interpolation endpoints, so mixed fallback
within a document is visible. Source counts do not audit optional event/planning
pages. Fractional seconds remain supported internally; hourly pages themselves
are sampled on the hour.

GHA and declination are geocentric apparent quantities. The lunar table now
explicitly uses **geocentric semidiameter**, rather than the observer-dependent
semidiameter returned for local planning by the shared engine. Local planning
retains its existing observer-specific value. Venus retains the provider's
almanac phase correction. Printed angles are rounded to 0.1 arcminute;
rounding agreement is not a claim of exact physical accuracy.

## Verification

Local results on 22 September 2026: the plugin builds; 209 headless tests pass,
with seven opt-in GUI tests skipped. The Almanac GUI test passes separately at
both sizes, including top and bottom scrolling positions. Rendered A4 source,
hourly, star, increment, Ageton and altitude-correction pages were inspected.
This is Linux verification, not Windows/macOS certification.

- Build the plugin and `celestial_tests` targets.
- Run the complete headless test suite with `CELNAV_TEST_DE440_ENABLE=1`.
- Focused tests cover preset dependencies, exact page estimates, source reporting
  with and without the kernel, out-of-coverage fallback, fixed DUT1 override,
  geocentric lunar semidiameter, and PDF generation/booklet layout.
- A printed-cell Sun reduction uses the archived USNO Point Judith coordinates,
  hourly ephemeris, printed increments and Ageton reduction. GHA/declination
  tolerances are 0.2/0.1 arcminute; Hc tolerance is 0.5 arcminute. Its synthetic
  sextant observation also checks dip/refraction and the resulting intercept.
  This is a regression example, not a claim that all physical observations
  achieve those tolerances.
- Run the opt-in `AlmanacUi.*` test separately with
  `CELESTIAL_RUN_UI_TESTS=1` and a working display. It checks preset/method
  transitions and control bounds at 1080 × 760 and 760 × 560.
- A two-day A4 paper-only example produces 135 pages, about 4.6 MB, because the
  universal paper references dominate short voyages. Scientific-calculator mode
  is substantially smaller. This is not an 80-page annual Reeds equivalent.

## Development boundaries

This work remains on pob220's development branch. No upstream PR, catalogue
publication, working-plugin installation or modification of the 2.8.9 release
is part of this change. The commit skips publishing CI. Windows and macOS GUI
checks remain necessary before a release; Linux GUI checks cannot certify them.

Compressed monthly ephemerides, a new ABC/versine method, and broader body-centre
kernel support remain separate proposals needing their own reference testing.
Do not label this development output as a certified replacement for an official
Nautical Almanac.

## Reference and implementation pointers

- [USNO celestial-navigation service](https://aa.usno.navy.mil/data/celnav)
- [JPL planetary ephemerides](https://ssd.jpl.nasa.gov/planets/eph_export.html)
- [IERS Earth orientation data](https://www.iers.org/IERS/EN/DataProducts/EarthOrientationData/eop.html)
- `test/navigation_de440_tests.cpp`: archived USNO and Horizons comparisons.
- `test/almanac_generator_tests.cpp`: printed-table and fallback regressions.
- `test/almanac_ui_tests.cpp`: opt-in display checks.
