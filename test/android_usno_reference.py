#!/usr/bin/env python3
"""Compare an actual tablet CSV with retained independent USNO references."""
import argparse
import csv
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('csv', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1] / 'validation/android-usno-20260927'
def reference(suffix):
    data = json.loads((root / f'usno-worksheet-{suffix}.json').read_text())
    return {r['object'].lower(): r['almanac_data'] for r in data['properties']['data']}
a, b, fractional = reference('33'), reference('34'), reference('fraction70')
# This retained API version echoes .987 but calculates the preceding second.
assert a == fractional
rows = list(csv.DictReader(args.csv.open()))
assert len(rows) == 175
assert rows[0]['UTC'] == '2025-07-20T17:16:33.987Z'
assert all(r['UTC'].endswith('.987Z') for r in rows)
for row in rows[:7]:
    body = row['Body'].lower()
    if body not in a:
        print(f'{body}: not returned by USNO below its altitude threshold')
        continue
    for key, column in [('hc', 'Hc_deg'), ('dec', 'Declination_deg')]:
        expected = a[body][key] + .987 * (b[body][key] - a[body][key])
        error = (float(row[column]) - expected) * 60
        print(f'{body} {key}: {error:+.6f} arcmin')
        assert abs(error) <= .1, f'{body} {key} outside 0.1 arcminute'
    # Report rather than conflate frames: DE440 overwrites Zn with airless
    # topocentric azimuth; USNO supplies geocentric Zn and UT1 rather than UTC.
    for key, column in [('gha', 'GHA_deg'), ('zn', 'Zn_true_deg')]:
        expected = a[body][key] + .987 * (b[body][key] - a[body][key])
        print(f'{body} {key} frame/time comparison: {(float(row[column])-expected)*60:+.6f} arcmin')
print('Six visible bodies: independent Hc/declination tolerance PASS')
