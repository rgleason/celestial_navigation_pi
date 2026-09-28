# Physical lunar-pair reference

Public Royal Observatory Greenwich fixture: 51.4779° N, 0° E,
2024-06-21 22:00:00 UTC on tablet, 22:00:00/22:05:00 UT1 from USNO.
Responses retrieved 28 September 2026 through the
[USNO API](https://aa.usno.navy.mil/data/api), with SHA256 in the manifest.
These public historical inputs contain no tablet boat position.

`tablet-observed.json` is an explicit transcription of real displayed cards
on committed Android runtime7b34fd4, with screenshot names retained in private
evidence. It is not a plugin file export or generated numerical fixture.
`test/android_lunar_pair_usno_reference.py` compares geocentric Moon–Spica,
Moon–Vega and Moon–Deneb distance, five-minute distance rate, 0.1′ timing
sensitivity and star geometric Hc with these independent USNO outputs.
Distance/Hc tolerance is0.1arcminute; rate/sensitivity tolerance includes
one-decimal display rounding (0.051arcminute/hour or second).

The Moon parallax and refraction corrections in the service are not applied
to this geocentric planning distance. Sextant apparent contact predictions
use a different frame. The separate physical Deneb/Vega centre check at
1013 hPa and 10°C produced23.835591672966757°, versus23.8357759454019°
from USNO geometric Hc minus its refraction correction and published Zn.
Its0.01106′ discrepancy is within0.1′. Refraction models, UTC/UT1 and
topocentric azimuth conventions can differ; this is a tolerance check,
not an assertion that both implementations use identical frames.
This validates three physical rows at one instant, not all rows/epochs/data
providers, session solver results or optional terrain accuracy.

## Independent session input

`make_session.py` reproducibly creates `session-sights.xml`, four public
Moon–Spica/Vega observations at22:00/22:05 from the retained USNO responses.
It applies USNO parallax/refraction to Hc, computes apparent centre separation
from those altitudes and published Zn, subtracts Moon semidiameter for near
contact, and uses the lower Moon limb for altitude. Eye height/IE are zero,
pressure1013hPa, temperature10°C, uncertainties0.5′, motion off. The same
Moon-altitude reading is shared between both stars at each epoch.
The model under test is never used to generate these fixture readings.

These are approximate observations across different apparent-coordinate,
refraction, UTC/UT1 and disc projection conventions. The low Moon altitude
(approximately5°) accentuates those differences. Predetermined acceptance
limits: known-position clock within1s; joint clock within60s and reference
position within3NM; angular RMS below0.5′. The joint limits cover fixture
model differences and are not a claim of general navigation accuracy.
`LunarUsnoSession.PublicGreenwichClockAndJointReference` uses real production
ephemerides, exercises the Android-compiled session engine, and checks reversed
observation order. Physical entry/staging, solve, save and reopen are tracked
separately in the tablet audit; this test alone cannot accept the Android UI.
