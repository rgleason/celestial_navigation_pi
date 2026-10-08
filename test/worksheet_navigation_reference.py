#!/usr/bin/env python3
"""Independent reference for the tablet's three-Sun worksheet workflows.

Inputs are INTERCEPT_SIGHTS rows 0, 2 and 6 in altitude_tests.cpp, from the
existing Intercept Calculation worksheet. No plugin calculation is imported.
Altitude uses the spherical astronomical triangle; observer travel uses
GeographicLib WGS84, and angular least squares uses SciPy. These deliberately
stationary sights give large residuals when an artificial 5 kn eastward track
is imposed. This is a regression case, not a recommended navigation fix.

Run with numpy, scipy and geographiclib installed in an isolated environment.
The physical audit used NumPy 2.5.2, SciPy 1.18.1, GeographicLib 2.1.
Compare navigation results at 0.1 NM/0.1 arcminute, stationary legacy solvers
at 0.3 NM (different objectives). This is not a DE440 ephemeris accuracy test.
"""
import json
from datetime import datetime, timezone

import geographiclib
import numpy as np
import scipy
from geographiclib.geodesic import Geodesic
from scipy.optimize import least_squares

ROWS = np.array([
    [10.1467683333333, 20.548655, 30.1571],
    [77.5317933333333, 20.51284, 67.1934],
    [137.975475, 20.4805416666667, 35.2452],
])  # GHA degrees west, declination north, worksheet Ho degrees
TIMES = [datetime(2025, 7, 20, *clock, tzinfo=timezone.utc)
         for clock in [(12, 47, 0), (17, 16, 33), (21, 18, 20)]]
HOURS = np.array([(time - TIMES[0]).total_seconds() / 3600 for time in TIMES])
DR = (43.2366916666667, -77.533415)  # north/east-positive degrees


def hc(latitude, longitude, row):
    gha, declination, _ = row
    lat, dec, lha = np.radians([latitude, declination, gha + longitude])
    return float(np.degrees(np.arcsin(np.clip(
        np.sin(lat) * np.sin(dec) + np.cos(lat) * np.cos(dec) * np.cos(lha),
        -1, 1))))


def travel(position, hours, course=90, speed=5):
    point = Geodesic.WGS84.Direct(*position, course, hours * speed * 1852)
    return point['lat2'], point['lon2']


def fit(speed, epoch_extra_seconds=0):
    elapsed = HOURS[-1] - HOURS + epoch_extra_seconds / 3600

    def residual(position):
        # Propagate back from the final fix epoch to each observation.
        return np.array([(hc(*travel(position, -hours, speed=speed), row)
                          - row[2]) * 60
                         for row, hours in zip(ROWS, elapsed)])

    solution = least_squares(residual, DR, xtol=1e-13, ftol=1e-13, gtol=1e-13)
    assert solution.success
    return dict(latitude=float(solution.x[0]), longitude=float(solution.x[1]),
                hc_minus_ho_arcmin=residual(solution.x).tolist(),
                rms_arcmin=float(np.sqrt(np.mean(residual(solution.x)**2))))


def sequence(moving):
    residual = np.array([(row[2] - hc(
        *(travel(DR, hour) if moving else DR), row)) * 60
        for row, hour in zip(ROWS, HOURS)])
    median = np.median(residual)
    mad = np.median(np.abs(residual - median))
    outlier = np.abs(residual - median) > max(2, 3 * 1.4826 * mad)
    keep = ~outlier
    return dict(ho_minus_hc_arcmin=residual.tolist(),
                mean_arcmin=float(np.mean(residual)),
                sd_sample_arcmin=float(np.std(residual, ddof=1)),
                median_arcmin=float(median), mad_arcmin=float(mad),
                possible_outliers=outlier.tolist(),
                robust_trend_arcmin_per_hour=float(np.polyfit(
                    HOURS[keep], residual[keep], 1)[0]))


if __name__ == '__main__':
    print(json.dumps(dict(
        versions=dict(numpy=np.__version__, scipy=scipy.__version__,
                      geographiclib=geographiclib.__version__),
        input_rows=ROWS.tolist(), utc=[time.isoformat() for time in TIMES],
        dr=DR, stationary_fix=fit(0), running_fix=fit(5),
        fractional_epoch_running_fix=fit(5, .987),
        stationary_sequence=sequence(False), moving_sequence=sequence(True)),
        indent=2))
