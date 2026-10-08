# 2.9 Almanac accuracy audit — USNO comparison

Audit date: 22 September 2026. Software: Celestial Navigation 2.9.0.0 development branch. Document revision: 1 (independent of software version).
Calculation source commit: `562871c`; local DE440s SHA-256: `c1c7feeab882263fc493a9d5a5b2ddd71b54826cdf65d8d17a76126b260a49f2`.

## Findings

Across this sample, Sun, Venus, the other navigational planets, Aries and selected stars closely track the official USNO values. DE440s materially reduces the worst sampled lunar GHA difference from 0.194′ to 0.121′ at engine precision. The rounded lunar table still differs from USNO by as much as 0.154′; therefore this audit does **not** establish exact 0.1′ Nautical Almanac concordance for every Moon row. Four high-difference lunar epochs agree with independent JPL Horizons to about 0.001′ in GHA, supporting the DE440s result at those epochs. No plugin calculation was changed as part of this audit.

## Method and coverage

- 108 scheduled USNO Celestial Navigation API epochs (two whole-second source requests each); 754 selected body/reference cases, each evaluated by both engine modes. Dates span 2015, 2024, 2025, 2026 and 2027, with dates on either side of the 2015 leap second (not the 23:59:60 instant), seasons and dated IERS prediction coverage. Three locations were used per UTC epoch.
- Reference time is UT1 = plugin UTC + its date-specific DUT1. In a direct check, USNO's API reported fractional-second text but computed at the whole second. Each reference value here interpolates independent calls at the adjacent whole seconds; GHA and azimuth use circular interpolation. The original uncorrected run is excluded from the results.
- Source `DE440s` means the runtime actually selected the short kernel. Mars, Jupiter, Saturn, Aries and stars use the analytical path in both modes. The table includes the plugin's **actual rounded hourly Almanac cells** for the Sun, Moon, planets and Aries, as well as raw engine values. Each row retains its source URLs in the detailed TSV.
- Errors below are absolute angular differences in arcminutes. GHA and azimuth differences wrap at 360°. A 0.1′ printed table has up to 0.05′ rounding error even with an identical underlying value.

## GHA compared with USNO

| Body | Cases | With kernel | Analytical raw median / max (′) | Kernel-mode raw median / max (′) | Analytical printed max (′) | Kernel-mode printed max (′) |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| Sun | 70 | DE440s 70 | 0.007 / 0.008 | 0.006 / 0.008 | 0.051 | 0.051 |
| Moon | 58 | DE440s 58 | 0.087 / 0.194 | 0.083 / 0.121 | 0.227 | 0.154 |
| Venus | 55 | DE440s 55 | 0.008 / 0.010 | 0.007 / 0.009 | 0.058 | 0.058 |
| Mars | 52 | Analytical 52 | 0.005 / 0.009 | 0.005 / 0.009 | 0.050 | 0.050 |
| Jupiter | 57 | Analytical 57 | 0.001 / 0.004 | 0.001 / 0.004 | 0.049 | 0.049 |
| Saturn | 55 | Analytical 55 | 0.001 / 0.006 | 0.001 / 0.006 | 0.049 | 0.049 |
| Aries | 108 | Earth rotation 108 | 0.001 / 0.001 | 0.000 / 0.000 | 0.049 | 0.049 |
| Sirius | 55 | Analytical 55 | 0.001 / 0.001 | 0.001 / 0.001 | — | — |
| Vega | 56 | Analytical 56 | 0.001 / 0.001 | 0.001 / 0.001 | — | — |
| Arcturus | 51 | Analytical 51 | 0.001 / 0.001 | 0.001 / 0.001 | — | — |
| Spica | 51 | Analytical 51 | 0.001 / 0.001 | 0.001 / 0.001 | — | — |
| Antares | 50 | Analytical 50 | 0.002 / 0.002 | 0.002 / 0.002 | — | — |
| Polaris | 36 | Analytical 36 | 0.015 / 0.023 | 0.015 / 0.023 | — | — |

## Declination compared with USNO

| Body | Cases | Analytical raw median / max (′) | Kernel-mode raw median / max (′) | Analytical printed max (′) | Kernel-mode printed max (′) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Sun | 70 | 0.001 / 0.002 | 0.001 / 0.003 | 0.049 | 0.049 |
| Moon | 58 | 0.022 / 0.079 | 0.029 / 0.046 | 0.111 | 0.077 |
| Venus | 55 | 0.002 / 0.003 | 0.002 / 0.003 | 0.047 | 0.047 |
| Mars | 52 | 0.001 / 0.001 | 0.001 / 0.001 | 0.049 | 0.049 |
| Jupiter | 57 | 0.001 / 0.002 | 0.001 / 0.002 | 0.051 | 0.051 |
| Saturn | 55 | 0.001 / 0.001 | 0.001 / 0.001 | 0.049 | 0.049 |
| Sirius | 55 | 0.000 / 0.000 | 0.000 / 0.000 | — | — |
| Vega | 56 | 0.001 / 0.001 | 0.001 / 0.001 | — | — |
| Arcturus | 51 | 0.000 / 0.001 | 0.000 / 0.001 | — | — |
| Spica | 51 | 0.001 / 0.001 | 0.001 / 0.001 | — | — |
| Antares | 50 | 0.000 / 0.000 | 0.000 / 0.000 | — | — |
| Polaris | 36 | 0.000 / 0.000 | 0.000 / 0.000 | — | — |

## Agreement at the printed 0.1′ rounding step

A cell counts as a match when its difference from the USNO unrounded reference is at most 0.05′ (one half-step). These percentages describe agreement with **USNO's model**, not a pass/fail judgement about JPL's Moon position.

| Body | GHA analytical / kernel mode | Declination analytical / kernel mode |
| --- | ---: | ---: |
| Sun | 97% / 97% | 100% / 100% |
| Moon | 22% / 33% | 72% / 69% |
| Venus | 96% / 96% | 100% / 100% |
| Mars | 100% / 100% | 100% / 100% |
| Jupiter | 100% / 100% | 96% / 96% |
| Saturn | 100% / 100% | 100% / 100% |
| Aries | 100% / 100% | — / — |

## Printed star SHA at its 00:00 UTC table epoch

Reference SHA is USNO star GHA minus USNO Aries GHA at the same UT1. Only visible stars returned by USNO at the table epoch qualify.

| Star | Compared cells | Maximum printed SHA difference (′) |
| --- | ---: | ---: |
| Sirius | 17 | 0.048 |
| Vega | 22 | 0.048 |
| Arcturus | 18 | 0.048 |
| Spica | 18 | 0.049 |
| Antares | 21 | 0.041 |

## Computed altitude and azimuth compared with USNO

| Body | Hc max analytical / kernel mode (′) | Zn max analytical / kernel mode (°) |
| --- | ---: | ---: |
| Sun | 0.007 / 0.007 | 0.000 / 0.000 |
| Moon | 0.186 / 0.113 | 0.009 / 0.008 |
| Venus | 0.009 / 0.009 | 0.000 / 0.000 |
| Mars | 0.008 / 0.008 | 0.001 / 0.001 |
| Jupiter | 0.005 / 0.005 | 0.000 / 0.000 |
| Saturn | 0.006 / 0.006 | 0.000 / 0.000 |
| Sirius | 0.002 / 0.002 | 0.000 / 0.000 |
| Vega | 0.003 / 0.003 | 0.000 / 0.000 |
| Arcturus | 0.002 / 0.002 | 0.000 / 0.000 |
| Spica | 0.003 / 0.003 | 0.000 / 0.000 |
| Antares | 0.002 / 0.002 | 0.000 / 0.000 |
| Polaris | 0.002 / 0.002 | 0.000 / 0.000 |

## Representative printed GHA entries

| UTC | Body | USNO (°) | Analytical printed (°) | DE440s-mode printed (°) | Δ analytical (′) | Δ kernel mode (′) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 2024-06-13T08:00:00 | Sun | 299.959291 | 299.960000 | 299.960000 | 0.043 | 0.043 |
| 2027-08-01T08:00:00 | Moon | 314.129107 | 314.130000 | 314.131667 | 0.054 | 0.154 |
| 2024-06-13T08:00:00 | Venus | 297.398838 | 297.398333 | 297.398333 | -0.030 | -0.030 |
| 2024-03-20T16:00:00 | Mars | 87.899222 | 87.900000 | 87.900000 | 0.047 | 0.047 |
| 2024-06-13T08:00:00 | Jupiter | 319.695356 | 319.695000 | 319.695000 | -0.021 | -0.021 |
| 2024-03-20T16:00:00 | Saturn | 74.370062 | 74.370000 | 74.370000 | -0.004 | -0.004 |
| 2024-06-13T00:00:00 | Aries | 261.797788 | 261.798333 | 261.798333 | 0.033 | 0.033 |

## Independent JPL Horizons checks of the lunar offset

Horizons supplies geocentric apparent right ascension and declination. For these checks, GHA is USNO's well-matched GHA of Aries minus Horizons apparent RA. Horizons RA is printed to 0.00001°, limiting this comparison to about 0.001′.

| UTC | USNO Moon GHA (°) | Horizons-derived GHA (°) | Plugin DE440s GHA (°) | Plugin–Horizons (′) | Plugin–USNO (′) | Dec plugin–Horizons (′) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 2024-09-22T16:00:00 | 181.672511 | 181.674108 | 181.674092 | -0.001 | 0.095 | 0.000 |
| 2026-01-03T00:00:00 | 4.982918 | 4.984861 | 4.984853 | -0.000 | 0.116 | -0.000 |
| 2027-08-01T00:00:00 | 199.152080 | 199.154110 | 199.154094 | -0.001 | 0.121 | -0.000 |
| 2027-08-01T08:00:00 | 314.129107 | 314.131102 | 314.131094 | -0.000 | 0.119 | 0.000 |

At these four epochs, the lunar DE440s path follows Horizons much more closely than the USNO service. This supports the lunar ephemeris calculation; it does not establish why the services differ or guarantee identical Nautical Almanac printed digits.

## Interpretation and limits

The reference is the USNO API output, not an assertion of an absolute 'true' angle. USNO cautions that the numerical precision returned by its API may exceed the accuracy of its displayed values. The short DE440s kernel is also not an independent reference for its own results; JPL Horizons provides the separate selected-case planet-centre check in `test/navigation_de440_tests.cpp`.

The USNO Moon SD altitude correction is observer dependent and includes augmentation; the Almanac prints geocentric SD. Those columns must not be subtracted directly. USNO applies a centre-of-light phase convention for Venus. Printed star SHA is tabulated at the day's reference epoch, so the report compares star GHA/declination from the engine at the observation time and printed SHA at the matching midnight epoch separately.

The Sun, navigational planets, Aries and sampled stars agree closely with USNO. The Moon is the exception: kernel-mode raw GHA reaches 0.121′ and the rounded table reaches 0.154′ from USNO. The analytical maximum is larger. At 2027-08-01 08:00 UTC, the printed DE440s GHA differs from USNO by 0.154′ versus 0.054′ for the analytical path, while the raw DE440s value matches Horizons within 0.001′. The four independent Horizons checks above favour the plugin's DE440s result at those epochs. A result outside 0.1′ can reflect reference-model differences or a defect and should be investigated by body, epoch and quantity; it is not silently accepted. This suite samples reference-visible bodies rather than every hourly row of a year. Air Almanac PDFs provide a separate printed cross-check, but their 1′ resolution cannot certify this plugin's 0.1′ tables. This audit did not systematically compare every printed HP/SD, v/d or altitude-correction table entry with an external source; the existing focused tests cover selected reductions and the geocentric lunar SD definition.

## Data and official references

- [USNO API documentation](https://aa.usno.navy.mil/data/api)
- [USNO celestial-navigation data conventions](https://aa.usno.navy.mil/data/celnav)
- [JPL Horizons documentation](https://ssd.jpl.nasa.gov/horizons/manual.html)
- [USNO Air Almanac PDFs](https://aa.usno.navy.mil/publications/aira)
- `schedule.tsv`: UTC, UT1, DUT1, location and forecast quality.
- `reference.tsv`: USNO interpolated reference values and URLs.
- `comparison.tsv`: every body in both modes, raw and printed values.
- `manifest.json`: exact source requests and USNO API version.
- `horizons-moon.tsv`: four JPL rows and exact query URLs.
