#!/usr/bin/env python3
"""Compare transcribed physical cards with independent public USNO almanac."""
import json
import math
from pathlib import Path

root = Path(__file__).resolve().parents[1] / "validation/android-usno-greenwich-20240621"
references = []
for name in ("usno-220000.json", "usno-220500.json"):
    data = json.loads((root / name).read_text())["properties"]["data"]
    references.append({row["object"].lower(): row["almanac_data"] for row in data})


def distance(first, second):
    dec1, dec2 = map(math.radians, (first["dec"], second["dec"]))
    gha = math.radians(first["gha"] - second["gha"])
    return math.degrees(math.acos(math.sin(dec1) * math.sin(dec2)
                                 + math.cos(dec1) * math.cos(dec2) * math.cos(gha)))


observed = json.loads((root / "tablet-observed.json").read_text())
for body, actual in observed["rows"].items():
    angles = [distance(ref["moon"], ref[body]) for ref in references]
    rate = (angles[1] - angles[0]) * 60 * 12
    expected = {"distance_deg": angles[0], "rate_arcmin_hour": rate,
                "sensitivity_seconds": 360 / abs(rate),
                "hc_deg": references[0][body]["hc"]}
    tolerances = {"distance_deg": 0.1 / 60, "rate_arcmin_hour": 0.051,
                  "sensitivity_seconds": 0.051, "hc_deg": 0.1 / 60}
    for field, value in expected.items():
        error = abs(actual[field] - value)
        assert error <= tolerances[field], (body, field, error)
        print(f"{body} {field}: actual={actual[field]:.9f}, reference={value:.9f}, error={error:.9f}")

# USNO altitude corrections subtract refraction from an apparent observation.
# Reconstruct apparent star altitudes and use their published true azimuths.
# This is a separate frame from the geocentric planning distances above.
data = json.loads((root / "usno-220000.json").read_text())["properties"]["data"]
stars = {row["object"].lower(): row for row in data}
sextant = observed["sextant"]
directions = []
for body in sextant["bodies"]:
    row = stars[body]
    altitude = row["almanac_data"]["hc"] - row["altitude_corrections"]["refr"]
    directions.append((math.radians(altitude),
                       math.radians(row["almanac_data"]["zn"])))
h1, z1 = directions[0]
h2, z2 = directions[1]
expected = math.degrees(math.acos(math.sin(h1) * math.sin(h2)
                                 + math.cos(h1) * math.cos(h2) * math.cos(z1-z2)))
error = abs(sextant["apparent_distance_deg"] - expected) * 60
assert error <= sextant["tolerance_arcmin"], error
print(f"Deneb/Vega apparent: reference={expected:.9f}, error={error:.9f} arcmin")
