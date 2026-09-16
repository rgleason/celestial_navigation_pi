# Eclipse-engine validation

The validation suite is offline and deterministic once the three manifest-
pinned data files are present. It does not use the plugin's pre-existing
low-precision nautical-almanac routines.

## Full catalog replay

`tests/nasa_catalog_1850_2100.csv` is a compact fixture derived from the three
official NASA/GSFC century catalogs. With DE440s, the engine reproduces all
571 events from 1850 through 2100 with exactly the same dates and classes:

| Class | NASA | Engine |
| --- | ---: | ---: |
| Partial | 195 | 195 |
| Annular | 186 | 186 |
| Total | 172 | 172 |
| Hybrid | 18 | 18 |

Across all 571 rows, the maximum difference from NASA's published greatest-
eclipse Dynamical Time is 0.614 second and the maximum magnitude difference
is 0.000392. This includes grazing non-central T-/T+/A-/A+ eclipses. The test
is run automatically whenever `ECLIPSE_DE440_PATH` is supplied and has a
one-second / 0.0005 acceptance threshold.

Reference catalogs:

* <https://eclipse.gsfc.nasa.gov/SEcat5/SE1801-1900.html>
* <https://eclipse.gsfc.nasa.gov/SEcat5/SE1901-2000.html>
* <https://eclipse.gsfc.nasa.gov/SEcat5/SE2001-2100.html>

## 2 August 2027 detailed replay

The independent DE440 result at NASA's published greatest-eclipse epoch and
ΔT is 25.503566° N, 33.180432° E. NASA's rounded detailed-table value is
25°30.3′ N, 33°11.0′ E, about 0.33 km away.

At 10:00 UT1 the engine obtains:

| Quantity | Engine |
| --- | ---: |
| Northern limit | 27.854493° N, 31.730604° E |
| Central line | 26.887752° N, 31.011477° E |
| Southern limit | 25.921031° N, 30.301562° E |
| Path width | 257.495 km |

The corresponding NASA rounded positions differ by about 0.36 km, 0.20 km
and 0.19 km respectively. At NASA's greatest-eclipse location, the spherical-
limb C2–C3 duration is 382.415 seconds versus NASA's 382.6 seconds.

Reference:
<https://eclipse.gsfc.nasa.gov/SEbeselm/SEbeselm2001/SE2027Aug02Tbeselm.html>

## Independent readers and optional lunar limb

At the 2027 epoch, all six native DE440 Earth-centred Sun/Moon vector
components agree with independently captured JPL Horizons values to 0.01 km.
The native binary-PCK reader's full 3 × 3 J2000-to-MOON_PA_DE440 rotation
matrix agrees with official CSPICE N0067 output to 2 × 10⁻¹² per element.

The 64-ppd LOLA converter was exercised against NASA's official
`LDEM64_PA_pixel_202405.grd`. The resulting 530,841,624-byte runtime pack is
verified byte-for-byte before use. At the 2027 reference position its terrain
profile shifts C1, C2, C3 and C4 by +2.199, −1.072, +1.741 and −0.130 seconds
respectively. These are terrain refinements, not claimed as a replacement for
a full atmospheric/observational contact model.

## Performance and isolation

On the development laptop, a complete 1850–2100 discovery/classification run
takes about 14.4 seconds; 2027 path plus five magnitude contours exports as
valid GeoJSON in about 0.26 second. The eclipse engine/CLI has no CSPICE,
netCDF or network-library dependency, and the plugin gains no CSPICE or netCDF
dependency from the module. netCDF is linked only into the optional
developer-side converter; the plugin template's unrelated existing libraries
are outside the eclipse engine.

## OpenCPN integration replay

The plugin has an opt-in `CELESTIAL_ECLIPSE_INTEGRATION_TEST` build mode used
only for automated integration runs. In the isolated Test-OpenCPN 5.14
profile it opens the planner, discovers the 2027 eclipses, selects 2 August,
computes 101 centre/limit samples and five magnitude contours, evaluates local
circumstances with the LOLA pack, and plots the result on the chart. The same
scenario has been visually and log-verified through both OpenCPN rendering
paths:

* software `RenderOverlay` (`wxDC`); and
* hardware-accelerated `RenderGLOverlay` (`piDC` on OpenGL 4.6/Mesa).

The shipping build has this test mode disabled. No integration run or install
used the machine's working OpenCPN profile.
