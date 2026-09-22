#!/usr/bin/env python3
"""Archive selected independent JPL Horizons lunar apparent-place checks."""
import argparse
import csv
import json
from datetime import datetime, timedelta
from pathlib import Path
from urllib.parse import urlencode
from urllib.request import urlopen

TIMES = ["2024-09-22T16:00:00", "2026-01-03T00:00:00",
         "2027-08-01T00:00:00", "2027-08-01T08:00:00"]
BASE = "https://ssd.jpl.nasa.gov/api/horizons.api"


def fetch(text):
    instant = datetime.fromisoformat(text)
    end = instant + timedelta(minutes=1)
    parameters = {
        "format": "json", "COMMAND": "'301'", "CENTER": "'500@399'",
        "EPHEM_TYPE": "'OBSERVER'", "START_TIME": f"'{instant:%Y-%m-%d %H:%M}'",
        "STOP_TIME": f"'{end:%Y-%m-%d %H:%M}'", "STEP_SIZE": "'1 m'",
        "QUANTITIES": "'2,13'", "ANG_FORMAT": "'DEG'",
        "CSV_FORMAT": "'YES'", "OBJ_DATA": "'NO'",
    }
    url = BASE + "?" + urlencode(parameters)
    with urlopen(url, timeout=30) as response:
        payload = json.load(response)
    content = payload["result"]
    first = next(line for line in content.splitlines()
                 if line.startswith(" ") and instant.strftime("%Y-%b-%d %H:%M") in line)
    cells = next(csv.reader([first]))
    return [text, float(cells[3]), float(cells[4]), float(cells[5]), url]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    with args.output.open("w", newline="") as target:
        writer = csv.writer(target, delimiter="\t")
        writer.writerow(["utc", "apparent_ra_deg", "apparent_dec_deg",
                         "angular_diameter_arcsec", "url"])
        for instant in TIMES:
            writer.writerow(fetch(instant))
    print(f"Fetched {len(TIMES)} JPL Horizons lunar rows")


if __name__ == "__main__":
    main()
