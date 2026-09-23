# Celestial Navigation 2.8.10 - optional DE440s navigation and Almanac

This is a focused backport of the tested DE440s navigation path from the
private 2.9 branch, based on Rick's 2.8.9 master. It does not include the
2.9 planner modes, ecliptic/Moon-path displays, sight-management additions,
Lab application or compact-Almanac preset. Existing workflows remain.

## Use in the field

- Existing verified `de440s.bsp` installations are reused automatically.
  Otherwise choose **Eclipses... > Download and install DE440s...**, or
  **Import DE440s...** for a local copy. No download occurs during calculation.
- Supported body centres are Sun, Moon, Mercury and Venus. The ordinary
  navigational Almanac tables include Sun, Moon and Venus; Mercury is not
  added as a new printed navigational planet.
- Automatic modern UTC handling starts at 1972-01-01 and is limited by the
  compact kernel's coverage (ending in 2150). Earlier dates, unsupported
  targets, missing/invalid packs and out-of-coverage dates use the analytical
  fallback. Mars, Jupiter, Saturn, and stars including Polaris remain
  analytical. Aries uses Earth rotation/equinox calculations.
- The 31.2 MiB optional kernel is not bundled with the plugin. Lunar
  orientation and LOLA terrain remain optional eclipse refinements; they
  are not needed for centre GHA/declination or ordinary lunar work.
- The Almanac defaults to dated offline IERS DUT1. Its checkbox instead
  applies one user-specified DUT1 throughout the document. Outside dated
  coverage, UT1=UTC fallback is reported. Updating Earth-rotation data remains
  available under **Lunar Tools > Advanced**.
- A **Sources and conventions** page records actual ephemeris choices and
  DUT1 coverage, including interpolation endpoints. Sight calculations also
  identify DE440s versus analytical use.

## Numerical boundaries

UTC, TT/TDB and UT1 remain distinct and fractional seconds are retained.
Lunar parallax and limb corrections use the same ephemeris distance as the
Moon position. Almanac semidiameter remains geocentric; observer-specific
augmentation is not silently printed as a universal Almanac value. Venus
retains the navigational centre-of-light correction.

DE440s is a higher-precision ephemeris foundation, not a promise that every
printed Almanac digit will match. Reference time assumptions, apparent-place
conventions, radius definitions and rounding must also match. Sextant,
atmosphere, horizon and clock errors still limit real observations.
No empirical offset has been added to imitate the USNO online Moon time
argument discussed in the reference investigation.

The existing analytical method and Windows-compatible mutex implementation
are retained. No chart, observation or user-configuration schema changes
are required. The bundled full manuals remain revisioned separately; this
release note and updated HTML explain the new ephemeris behaviour.

## Validation

See the [2.8.10 validation record](validation-2.8.10.md) for test results,
coverage and remaining platform limitations.
