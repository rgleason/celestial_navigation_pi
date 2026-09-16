# Lunar correctness and usability pass — 2.8.5.1

## What changed

Recorded distances, altitudes and reading times remain observations, not solver
outputs. “Save lunar solution” appends a named, versioned result with immutable
input snapshots, the existing and additional clock corrections, uncertainty and
the selected candidate's calculation report. It does not change the global
clock error. Saved results can be reviewed/copied from Lunar Tools and explicitly
selected for a fix using working copies of visible sights from the same watch.
The guard rejects sights more than 12 hours from the result's recorded reference
epoch; the operator must still identify the same clock/watch. The correction is
applied once, retaining fractional seconds. Opening an existing sight no longer
replaces its saved DR with the boat's present position.

The XML sight schema stays readable by older plugins: optional `LunarSolution`
children sit inside the existing `ClockError` node. New readers also accept old
files without these children. Older programs will discard the extra solution
records if they rewrite the file, so keep a backup before downgrading. Numeric
attributes now retain round-trip precision and use a locale-independent decimal
point. The optional recorded-time-basis attribute defaults to nominal UTC for
old sights. The new per-altitude time controls are explicitly 24-hour HH:MM:SS.

## Numerical model and reference audit

The former Sun–Moon calculation mixed a DE440 astrometric separation with
analytical geographic positions. DE440 light-time-corrected directions now also
include annual aberration, and the separation and geographic positions come
from the same apparent vectors. Independent [JPL Horizons](https://ssd.jpl.nasa.gov/horizons/manual.html)
fixtures test both apparent geocentric directions and topocentric lunar altitude.
Analytical fallback remains available when DE440 is missing or outside its
coverage, and is explicitly labelled reduced accuracy.

Lunar calculations now use the [WGS84 reference ellipsoid](https://earth-info.nga.mil/?action=wgs84&dir=wgs84):
a = 6378137 m, 1/f = 298.257223563. Observer latitude is geodetic; celestial GPs
remain geocentric directions. Exact vector subtraction supplies parallax and
topocentric semidiameters. Refraction is applied to observed altitude limbs and
to the apparent discs for distance contacts. Dip affects horizon altitudes, never
the inter-body angle. Both simultaneous and separately timed observations use
the same forward model. Spherical Direct Triangle arithmetic remains in the
report as a comparison, not as the final WGS84 answer. Motion is advanced on
WGS84 with a stateless direct geodesic, also checked against GeographicLib.

Bob's [Point Judith PDF](https://github.com/user-attachments/files/31986919/Lunars.Testing.Sept.8.2026.PDF.pdf)
uses 2024-06-13 19:26:00, DR 41°22′N 71°29′W, LD 85°40.3′ near/near,
Moon Hs 34°34′ upper, Sun Hs 51°58′ lower, eye 6.1 m, 23.9 C, 1019.3 hPa, IE 0.
With those inputs the near-DR WGS84 branch recovers approximately −8.45 seconds,
41.3674°N 71.4465°W, with formal time sigma about 26.3 seconds for 0.2′ angle
uncertainties. The alternate branch is near 2.58°N, not another nearby fix.
The former output was about −12.1 seconds; correcting apparent directions alone
in the spherical model gives about −11.2 seconds.

This does **not** establish exact agreement with every published answer.
The [Reed calculator](https://clockwk.com/apps/lunxx-may23.aspx) reports rounded
values and uses different ephemeris/reduction details (for example, displayed
Moon HP 54.33′ versus DE440 54.3093′). Its oblateness setting also changes the
answer. No empirical offset was introduced to force agreement. A position at
entered UTC and a position at recovered UTC are different comparisons: −12.1 s
alone shifts longitude about 3.025′ through Earth rotation. The new **Check at
entered UTC** mode keeps UTC fixed and reports a model-minus-observed residual;
it does not save a recovered clock correction.

At entered UTC the near-DR position is approximately 41.36593°N, 71.48114°W
(71°28.868′W), which is the appropriate comparison with the reference's
71°28.8′W position. The time matcher also now stops on an exact zero residual:
previously, an exact scan/midpoint hit could be refined away from the solution.

The PDF's separately timed screenshot must not be treated as a valid independent
test while retaining the averaged altitudes: use the actual angles belonging to
each timestamp (the original Sun pair is at 19:22 and 19:30, not 19:31).

## Multiple readings and uncertainty

A coherent watch can improve precision through independent readings. Shared
altitudes identified by body and recorded epoch count once even if several
lunar triples refer to them. Conflicting copies are rejected. The fit retains
residuals and outlier warnings without changing observations. Formal covariance
does not shrink below the supplied measurement-error floor merely because a
small sample fits perfectly. Candidate reports/positions follow the selected UTC.

WGS84 is a reference ellipsoid, not a full model of the physical geoid/local
vertical. Eye height approximates ellipsoidal height. UT1 is approximated by UTC;
measured DUT1, polar motion, diurnal aberration, local vertical deflection and
full light deflection are not applied. Refraction and horizon/instrument biases
limit real accuracy. Formal position sigma is horizontal RMS, not a 68% circular
confidence radius. More correlated or biased measurements do not guarantee a
better answer. Existing non-lunar altitude/fix algorithms are not converted to
an ellipsoidal model by this pass.

## Verification

Run `cmake --build build` and `ctest --test-dir build --output-on-failure` with
`OCPN_BUILD_TEST=ON` and a local DE440s kernel at `eclipse/data/de440s.bsp`.
Tests cover independent Horizons directions/altitude, WGS84 geodesic motion,
synthetic time/position recovery for limbs/contacts/moving observers, coincident
time-mode agreement, old/new solution XML, fractional fix correction without
raw-data mutation, and shared-reading covariance. GUI smoke testing is opt-in:
`CELESTIAL_RUN_UI_TESTS=1 build/test/celestial_tests --gtest_filter=LunarUiSmoke.*`
with a working display. Linux tests do not replace Windows/macOS acceptance
testing or on-water verification.

Verified on Linux, 2026-09-09: release plugin build; 148 automated tests and
the UTF-8 source check; the separately enabled GUI smoke test (one additional
test). No Windows/macOS runtime or on-water testing is claimed.
