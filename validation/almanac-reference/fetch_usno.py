#!/usr/bin/env python3
"""Fetch a reproducible, rate-limited USNO celestial-navigation comparison set.

Run only when refreshing references. The test runner consumes the saved TSV
offline. USNO interprets the request clock as UT1, so use the schedule generated
by AlmanacReferenceAudit.ExportUtcAndDut1Schedule rather than raw UTC strings.
"""
import argparse
import concurrent.futures
import csv
import json
import time
from datetime import datetime, timedelta
from pathlib import Path
from urllib.parse import urlencode
from urllib.request import Request, urlopen

BASE = "https://aa.usno.navy.mil/api/celnav"
BODIES = {
    "SUN": "Sun", "MOON": "Moon", "VENUS": "Venus", "MARS": "Mars",
    "JUPITER": "Jupiter", "SATURN": "Saturn", "ARIES": "Aries",
    "POLARIS": "Polaris", "SIRIUS": "Sirius", "VEGA": "Vega",
    "ARCTURUS": "Arcturus", "SPICA": "Spica", "ANTARES": "Antares",
}
HEADER = ["utc", "ut1", "dut1_seconds", "latitude", "longitude", "body",
          "ref_gha", "ref_dec", "ref_hc", "ref_zn", "ref_sd",
          "api_version", "url"]


def fetch(index, sample):
    ut1 = sample["ut1_date"] + "T" + sample["ut1_time"]
    instant = datetime.fromisoformat(ut1)
    lower = instant.replace(microsecond=0)
    fraction = instant.microsecond / 1_000_000

    def request(second):
        url = BASE + "?" + urlencode({
            "date": second.strftime("%Y-%m-%d"),
            "time": second.strftime("%H:%M:%S"),
            "coords": sample["latitude"] + "," + sample["longitude"],
            "ID": "pob220",
        })
        for attempt in range(4):
            try:
                with urlopen(Request(url, headers={"User-Agent": "CelNav-reference-audit/1.0"}),
                             timeout=30) as response:
                    payload = json.load(response)
                if "error" in payload:
                    raise ValueError(f"USNO error: {payload['error']}")
                return url, payload
            except Exception:
                if attempt == 3:
                    raise
                time.sleep(1.5 * (attempt + 1))

    floor_url, floor_payload = request(lower)
    if fraction:
        ceil_url, ceil_payload = request(lower + timedelta(seconds=1))
    else:
        ceil_url, ceil_payload = floor_url, floor_payload
    first_items = {x["object"]: x for x in floor_payload["properties"]["data"]}
    last_items = {x["object"]: x for x in ceil_payload["properties"]["data"]}

    def interpolate(a, b, circular=False):
        if a is None or b is None:
            return None
        try:
            a, b = float(a), float(b)
        except (TypeError, ValueError):
            return None
        delta = (b - a + 180) % 360 - 180 if circular else b - a
        result = a + fraction * delta
        return result % 360 if circular else result

    rows = []
    for name in first_items.keys() & last_items.keys():
        body = BODIES.get(name.upper())
        if body is None:
            continue
        first = first_items[name]["almanac_data"]
        last = last_items[name]["almanac_data"]
        first_correction = first_items[name].get("altitude_corrections") or {}
        last_correction = last_items[name].get("altitude_corrections") or {}
        rows.append({
            "utc": sample["utc"], "ut1": ut1,
            "dut1_seconds": sample["dut1_seconds"],
            "latitude": sample["latitude"],
            "longitude": sample["longitude"], "body": body,
            "ref_gha": interpolate(first.get("gha"), last.get("gha"), True),
            "ref_dec": interpolate(first.get("dec"), last.get("dec")),
            "ref_hc": interpolate(first.get("hc"), last.get("hc")),
            "ref_zn": interpolate(first.get("zn"), last.get("zn"), True),
            "ref_sd": interpolate(first_correction.get("sd"),
                                   last_correction.get("sd")),
            "api_version": floor_payload["apiversion"],
            "url": floor_url + " | " + ceil_url,
        })
    return index, rows, {
        "floor_url": floor_url, "ceil_url": ceil_url,
        "ut1_fractional_second": fraction,
        "api_version": floor_payload["apiversion"],
        "returned_bodies_floor": sorted(first_items),
        "returned_bodies_ceil": sorted(last_items),
        "ut1": ut1,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("schedule", type=Path)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("manifest", type=Path)
    args = parser.parse_args()
    with args.schedule.open(newline="") as source:
        schedule = list(csv.DictReader(source, delimiter="\t"))
    assert len(schedule) == 108, len(schedule)
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        results = list(pool.map(lambda pair: fetch(*pair), enumerate(schedule)))
    results.sort(key=lambda item: item[0])
    with args.fixture.open("w", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=HEADER, delimiter="\t")
        writer.writeheader()
        for _, rows, _ in results:
            writer.writerows(rows)
    args.manifest.write_text(json.dumps({
        "reference": "USNO Celestial Navigation API",
        "endpoint": BASE,
        "queries": [manifest for _, _, manifest in results],
        "selected_rows": sum(len(rows) for _, rows, _ in results),
        "selection": sorted(BODIES.values()),
    }, indent=2) + "\n")
    print(f"Fetched {len(results)} USNO queries, "
          f"{sum(len(rows) for _, rows, _ in results)} selected reference rows")


if __name__ == "__main__":
    main()
