# Physical Planner reference, 27 September 2026

Retained primary US Naval Observatory API responses for the existing worksheet
position, 2025-07-20 17:16:33/34 UT1 and the fractional query 17:16:33.987.
See [API documentation](https://aa.usno.navy.mil/data/api) and
[the navigation data service](https://aa.usno.navy.mil/data/celnav).
The manifest records input times and response SHA256.

In these responses, the fractional request echoes its input time but all
almanac values are numerically identical to the whole-second 33 response.
Interpolate between independently computed seconds33/34 for the .987 test;
over this one-second interval curvature is negligible at0.1arcminute. This
is a reference-service observation, not a reason to truncate plugin inputs.
USNO uses UT1; the plugin's input is UTC with its existing DUT1 provider.

`test/android_usno_reference.py <actual-tablet-csv>` checks the saved first-hour
Hc and declination for Sun, Moon, Venus, Mars, Jupiter and Polaris against those
responses, at0.1arcminute tolerance. This is a physical output comparison,
not a calculation by the plugin tested against itself. Saturn is not returned
below the service's altitude threshold in this case and has no pass here.

GHA and Zn differences are reported separately. DE440's retained baseline
provides geocentric Hc but airless topocentric Zn, while this USNO response uses
geocentric Zn; UTC/UT1 must also be distinguished. Do not describe this as an
exact same-frame azimuth verification, or use it to claim all other epochs,
altitudes, optional-data configurations or platform builds were tested.
