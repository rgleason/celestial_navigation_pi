# Moon altitude oblateness review for 2.9.0

Issue #306 correctly identifies a small effect absent from the ordinary Moon
altitude reduction: spherical parallax does not model an oblate observer.
This does not establish that a standalone latitude correction should simply
be added to the existing observed altitude.

`python3 validation/moon-oblateness-audit.py` compares independent WGS84 vector
geometry with the equatorial-radius spherical reduction. Across 540 cases
(latitudes -75 to +75 degrees, altitude 5 to 75 degrees, four azimuths and
horizontal parallax 54, 57 and 61 arcminutes), the difference ranges from
-0.230180 to +0.171100 arcminutes. The effect depends on azimuth as well as
latitude and altitude; it can change sign. A latitude-only approximation can
therefore be misleading. The audit verifies equatorial equality and the
north/south reflection symmetry. It is a geometric audit, not a new comparison
against printed Nautical Almanac page 280; that page's transcription has not
been independently verified here.

## Decision for this pass

Do not apply the proposed additive correction in isolation. The ordinary
altitude workflow forms a spherical COP from one corrected Ho. An ellipsoidal
correction evaluated only at the saved DR would make that displayed circle and
the position solver inconsistent away from the DR. A complete change must
evaluate the observer at each trial position and generate the corresponding
non-spherical locus, with tests shared by reduction, fix and rendering.
This is deferred from this alpha pass; ordinary Moon altitude still uses
spherical parallax and should not be described as ellipsoid-corrected.

The DE440 lunar-distance solver already uses WGS84 observer geometry. No
second correction is applied there. Its independent Horizons airless-altitude
reference test and synthetic limb/contact/moving-observer tests remain in the
regression suite.
