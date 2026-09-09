"""Independent reference decoding/geometry, with no imports from either engine."""
import csv
import datetime as dt
import math


def row(result):
    before, after = result.split("$$SOE", 1)
    table, _ = after.split("$$EOE", 1)
    headers = next(line for line in reversed(before.splitlines())
                   if "Date__" in line and "R.A." in line)
    names = [x.strip() for x in next(csv.reader([headers]))]
    values = [x.strip() for x in next(csv.reader([table.strip().splitlines()[0]]))]
    if len(names) != len(values):
        raise ValueError("Horizons column count changed")
    return dict(zip(names, values))


def decode(payload, refracted=False):
    r = row(payload["result"])
    kind = "r" if refracted else "a"
    result = {}
    for output, header in (("ra", "R.A._(rfct-app)" if refracted else "R.A.__(a-app)"),
                            ("dec", "DEC_(rfct-app)" if refracted else "DEC___(a-app)"),
                            ("az", f"Azimuth_({kind}-app)"),
                            ("alt", f"Elevation_({kind}-app)"), ("diameter", "Ang-diam")):
        if header in r:
            value = float(r[header])
            if not math.isfinite(value):
                raise ValueError("Nonfinite Horizons value")
            result[output] = value
    if not {"ra", "dec", "diameter"} <= result.keys():
        raise ValueError("Required Horizons columns missing")
    result["utc_label"] = next(value for key, value in r.items() if key.startswith("Date__"))
    return result


def separation(ra1, dec1, ra2, dec2):
    # atan2(|u x v|, u.v), stable near conjunction and opposition. This does
    # not use the engine's acos implementation or its ephemeris vectors.
    def vector(ra, dec):
        a, d = map(math.radians, (ra, dec))
        return math.cos(d)*math.cos(a), math.cos(d)*math.sin(a), math.sin(d)
    a, b = vector(ra1, dec1), vector(ra2, dec2)
    cross = (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
    return math.degrees(math.atan2(math.sqrt(sum(x*x for x in cross)), sum(x*y for x,y in zip(a,b))))


def utc_shift(timestamp, seconds):
    instant = dt.datetime.fromisoformat(timestamp.replace("Z", "+00:00"))
    return (instant + dt.timedelta(seconds=seconds)).isoformat(timespec="seconds").replace("+00:00", "Z")


def expected_sight(moon, sun, limb, moon_contact, sun_contact):
    """Vacuum spherical-disc geometry from external topocentric directions.

    Angular diameters come from Horizons; an artificial horizon doubles the
    altitude angle, but never doubles the measured inter-body separation.
    """
    centre = separation(moon["ra"], moon["dec"], sun["ra"], sun["dec"])
    mrad, srad = moon["diameter"]/7200, sun["diameter"]/7200
    sign = (-1, 0, 1)[limb]
    return {"distance": centre + (-1, 0, 1)[moon_contact]*mrad + (-1, 0, 1)[sun_contact]*srad,
            "moon_alt": 2*(moon["alt"] + sign*mrad),
            "sun_alt": 2*(sun["alt"] - sign*srad)}
