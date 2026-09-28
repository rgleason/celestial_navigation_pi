# Independent horizon bearing fixture

Use the retained public USNO Sun response for 2024-06-14 17:00:00 UT1.
Its declination/GHA and solar semidiameter supply independent astronomical
inputs. The [USNO horizon definition](https://aa.usno.navy.mil/faq/RST_defs)
uses a nominal 34 arcminute horizon refraction. Set tablet pressure1010hPa,
temperature10°C, eye height0 and an unobstructed sea horizon. Estimate horizontal
parallax from USNO's altitude parallax divided by cos(Hc); this small-angle
approximation and UTC/UT1 differences are covered by the declared tolerance.

`make_reference.py` solves the forward east/north/up celestial direction with
SciPy least squares from multiple independent starting positions. It does not
use the plugin, its horizon inversion, or a plugin-generated ephemeris. Both
latitude branches must be retained. Limits fixed before physical execution:
each displayed branch within1NM of its independent reference; true60° and
magnetic57°+east5°−west2° must give the same effective bearing/branches.
This is an approximate sea-horizon model check, not a refraction/terrain
accuracy claim or an assertion of a unique fix.

Stage one excluded public observation while protecting every existing record.
Staging is not Save acceptance. Use actual editor inputs, Save, independent XML
readback, reopen/cold reload, portrait/landscape final result swipes and chart
display. Exercise mismatched event/bearing refusal and Cancel/Back without
changing saved data. Remove only the owned record and restore only the
task-changed horizon defaults after strict current-state checks.
