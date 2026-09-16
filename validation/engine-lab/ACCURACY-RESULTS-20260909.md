# Expanded accuracy checkpoint — 9 September 2026

No numerical changes have been applied. Both isolated engines remain identical
to the pinned v2.8.5.1 snapshot and its frozen headless adapter. Production source
and the installed OpenCPN plugin have not been changed.

## Result

The expanded offline suite completed **123 scenarios / 758 checks**, with
**30 failed accuracy checks (15 per engine), zero execution errors and zero
A/B differences**. The nonzero exit status is intentional evidence of failed
accuracy gates, not a harness crash. Limits were fixed before evaluating results.

The table counts checks for **one engine**; both produced the same results.

| Check | Count | Largest absolute error | Limit | Failures |
| --- | ---: | ---: | ---: | ---: |
| Geocentric Sun/Moon RA/Dec | 100 | 0.05263″ | 0.1″ | 0 |
| Airless topocentric altitude | 50 | 7.19767″ | 1″ | 9 |
| Airless topocentric separation | 25 | 0.28617″ | 1″ | 0 |
| Airless limb/contact increments | 96 | 0.00193″ | 0.01″ | 0 |
| Standard-atmosphere refraction increments | 24 | 8.74939″ | 10″ | 0 |
| Published ray-traced refraction table | 15 | 1.93749″ | 2″ | 0 |
| Independent inverse clock recovery | 34 | 0.53516 s | 1 s | 0 |
| Independent inverse position recovery | 34 | 0.14136 NM | 0.1 NM | 6 |
| Positive-pressure vacuum-limit bound | 1 | 0.000000653″ | 0.000001″ | 0 |

Position errors are angular separations converted using 60 NM per degree; the
largest is approximately 262 metres. These are theoretical reference tests,
not a claim that sextant observations attain these errors in practice.

The original 18-scenario / 86-check suite still passes. Harness unit tests pass
(10 original plus 11 expanded). Existing CTest targets `celestial_tests` and
`ui_utf8_literals` also pass; this is not a new interactive GUI test.

## What the discrepancies indicate

The large altitude discrepancies strongly implicate the adapter's existing
UT1=UTC assumption, not the use of an ellipsoid. Earth orientation depends on
UT1, whereas the sight timestamps here are UTC. JPL supplies dated UT1−UTC as
quantity 49. See the [Horizons manual](https://ssd.jpl.nasa.gov/horizons/manual.html).

Separately acquired JPL DUT1 values predict the missing first-order altitude
term, `−15.041067 × DUT1 × cos(latitude) × sin(azimuth)`, in arcseconds:

| Moon case | DUT1 (s) | Actual altitude error | Predicted omitted-DUT1 term |
| --- | ---: | ---: | ---: |
| 1972-07-01 12:00 UTC | +0.35851 | +5.24954″ | +5.34775″ |
| 2016-12-31 23:59:59 UTC | −0.40878 | +5.32090″ | +5.04828″ |
| 2017-01-01 00:00:00 UTC | +0.59122 | −7.19767″ | −7.30102″ |

Across the complete grid, subtracting this explanatory projection leaves at
most 0.45670″. **This is not a corrected engine run**: all original altitude
failures remain red. Remaining effects, including diurnal aberration, polar
motion and frame conventions, need separate controlled investigation.

The six inverse position failures occur in the three limb variants of each of
two geometries: 2020 leap day (about 0.10278 NM) and the June equator case
(about 0.14136 NM). DUT1 alone must not be assumed to explain those failures;
the latter has only −0.01720 s DUT1. Clock/position coupling and the remaining
topocentric model differences should be measured after the first correction.

## Scope and provenance

- 25 site configurations, 1972–2025, including northern/southern/equatorial
  locations, elevation, low/near-zenith angles and conjunction/opposition.
- 120 raw Horizons responses plus 17 separately acquired DUT1 responses are
  archived with request parameters, acquisition dates and SHA256 checksums.
- Thirty-four externally generated inverse cases use fixed JPL angles and a
  deliberately shifted watch label. They do not use baseline-generated sights.
  Known reference position selects the branch; ambiguity selection is not tested.
- Geometry exclusions are recorded. Leap-boundary forward tests are retained;
  inverse scans crossing a leap second are excluded, not treated as successes.
- Inverse vacuum fixtures use 0.000001 hPa solely because the unchanged public
  solver rejects zero pressure. Its tiny refraction effect is separately bounded.
- Refraction table values come from the documented ray-tracing column in
  [ERFA refco.c](https://github.com/liberfa/erfa/blob/master/src/refco.c), not an
  evaluation of the same bundled ERFA function as the engine. Horizons'
  approximate atmosphere is a separate comparison with a separate tolerance.
- DE441/Horizons and DE440s share related underlying astronomy. The external
  pipeline is independent; it is not an infallible or wholly independent model.
- No new GNSS-verified real observations, physical sea-horizon dip benchmarks,
  refracted contact benchmarks, moving-observer truth or star/planet lunars
  have been added. Existing raw observations remain untouched.

## Recommended next experiment

Add a dated, explicit DUT1 input **only to the candidate adapter**, rerun these
same fixtures and retain the frozen baseline and tolerances. Then isolate the
remaining topocentric effects and inverse clock/position residuals one at a
time. These findings do not justify a production version bump or installation
yet.

Reproduction commands and fixture details are in [README.md](README.md).
Detailed outputs, source/binary hashes, failures, exclusions and DUT1 projections
are generated in `.work/accuracy-report.json` by `accuracy.py`.
