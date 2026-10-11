# DE440s versus the USNO celestial-navigation API: Moon time argument

This is an offline-engine diagnostic, **not** a reason to adjust a navigator's
recorded UTC or to change the production clock model. The USNO web service
labels its input UT1; the plugin uses UTC, converts to TT with TAI−UTC, and
rotates Earth with dated DUT1. Those are separate time scales. The comparison
uses complete USNO API responses in `references/usno-20260918/`, including
request URLs, API version and SHA-256 hashes. The reference acquisition script
is opt-in; normal tests remain offline.

Point Judith is one of Bob's reported-observation scenarios, **not** an
authoritative fix or measurement. The independent references below are the
astronomical positions requested from JPL Horizons and USNO for its epoch;
other epochs/sites in the grid test whether the effect generalizes.

At 2024-06-13 19:26 UT1 (Point Judith), our DE440s geocentric Moon centre is
about 3.9 arcseconds away in GHA and 2.0 arcseconds in declination from the
USNO API after matching UT1. Holding UT1 fixed and advancing **only the
dynamical time argument** by an inferred 8.829 seconds makes both coordinates
agree to about 0.003 arcseconds, the scale of the API's decimal rounding.
The Sun at that same shifted ephemeris epoch also agrees with the API to its
reported precision. The offset is not constant over years:

| Requested UT1 epoch | Extra TT seconds inferred from Moon | Residual GHA after fit | Residual Dec after fit |
| --- | ---: | ---: | ---: |
| 2010-04-20 | +2.349 | <0.001″ | 0.002″ |
| 2020-02-29 | +5.964 | <0.001″ | 0.001″ |
| 2024-06-13 | +8.829 | 0.002″ | 0.003″ |
| 2025-03 (test epoch) | +9.311 | <0.001″ | <0.001″ |

The full diagnostic grid is reproducible with `assess_usno.py`. This fit
strongly indicates different UT1→TT (ΔT) policies. It does **not** establish
which specific algorithm the USNO online service uses; its public API
documentation does not specify that algorithm. For true UTC observations,
the IERS DUT1 and TAI−UTC relationship remains the appropriate time basis;
fitting an offset to an online table would degrade physical correctness.

Other causes checked:

- DE405, DE430 and DE440s JPL kernels put the geometric Moon centre within
  0.007″ of one another in this 2024 case. Kernel generation is not the source
  of the several-arcsecond difference.
- The DE440s apparent Moon coordinates match archived JPL Horizons (DE441)
  at the same UTC within the test's 0.1″ tolerance.
- Independent Horizons Mercury/Venus tests span Point Judith, equatorial,
  southern, elevated northern and March 2025 cases. For their geometric
  centres, maximum differences are 0.122″ RA, 0.043″ Dec and 0.218″ airless
  topocentric altitude. See `references/horizons-planets-20260918/` and
  `assess_horizons_navigation.py`. Venus's centre-of-light phase correction
  is intentionally excluded from that centre-to-centre comparison.
- A lunar limb/profile correction cannot alter a tabulated **centre** GHA or
  declination. DE440s gives the Moon's centre of mass, not its terrain. The
  observer-specific mean semidiameter remains a separate correction, with the
  chosen upper/lower limb applied once. The optional eclipse topography pack
  is not required for navigation.
- DUT1 shifts terrestrial rotation, not the lunar orbit. A dedicated test
  varies DUT1 by one second and sees about 15″ of GHA rotation but no
  material change in lunar declination or distance.

The online USNO comparison remains useful at *navigational published*
precision, but is not an independent sub-arcsecond acceptance criterion
unless its time-scale convention is known. JPL Horizons and independently
specified IERS/TAI values are the more suitable high-precision checks.

References:

- [USNO celestial-navigation service](https://aa.usno.navy.mil/data/celnav)
- [USNO API documentation (UT1 time label and output definitions)](https://aa.usno.navy.mil/data/api)
- [USNO glossary (ΔT = TT−UT1)](https://aa.usno.navy.mil/faq/asa_glossary)
- [USNO Earth-orientation information (ΔT from TAI−UTC and UT1−UTC)](https://maia.usno.navy.mil/information/eo-values)
- [JPL Horizons documentation](https://ssd.jpl.nasa.gov/horizons/manual.html)
- [USNO Nautical Almanac history (DE430 adoption)](https://aa.usno.navy.mil/publications/na_history)
- [NASA lunar limb-profile explanation](https://eclipse.gsfc.nasa.gov/SEmono/reference/profile.html)
