#!/usr/bin/env python3
"""Independent geometry audit for #306; no production correction is applied.

Compare geocentric altitude relative to the observer's geodetic horizontal
for WGS84 and an equatorial-radius sphere, with identical airless apparent
altitude, azimuth, and horizontal parallax. Standard-library-only.
"""
from itertools import product
from math import asin, cos, degrees, radians, sin, sqrt


def correction(latitude, altitude, azimuth, hp_arcmin):
    a = 6378137.0
    f = 1 / 298.257223563
    e2 = f * (2 - f)
    phi, h, z, hp = map(radians, (latitude, altitude, azimuth, hp_arcmin / 60))
    distance = a / sin(hp)
    n = a / sqrt(1 - e2 * sin(phi) ** 2)
    observer_north = -n * e2 * sin(phi) * cos(phi)
    observer_up = n * (1 - e2 * sin(phi) ** 2)
    projection = observer_north * cos(h) * cos(z) + observer_up * sin(h)
    line_distance = -projection + sqrt(
        projection ** 2 + distance ** 2 - observer_north ** 2 - observer_up ** 2)
    ellipsoid = asin((line_distance * sin(h) + observer_up) / distance)
    spherical = h + asin(sin(hp) * cos(h))
    return degrees(ellipsoid - spherical) * 60


if __name__ == "__main__":
    rows = [(lat, h, z, hp, correction(lat, h, z, hp))
            for lat, h, z, hp in product(
                (-75, -60, -45, -30, 0, 30, 45, 60, 75),
                (5, 10, 30, 60, 75), (0, 90, 180, 270), (54, 57, 61))]
    print(f"{len(rows)} cases; correction range "
          f"{min(r[4] for r in rows):+.6f} to {max(r[4] for r in rows):+.6f} arcmin")
    for lat in (-60, -30, 0, 30, 60):
        print(f"latitude={lat:+3d} H=30 Z=0 HP=57: {correction(lat, 30, 0, 57):+.6f} arcmin")
    assert abs(correction(0, 30, 0, 57)) < 1e-10
    for lat, h, z, hp, value in rows:
        assert abs(value - correction(-lat, h, 180-z, hp)) < 1e-10
