#!/usr/bin/env python3
"""Acquire USNO celestial-navigation reference responses explicitly.

The offline tests never call the network. Keep the complete raw JSON, request
URL, response digest and API version so discrepancies remain auditable.
"""

import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
import urllib.parse
import urllib.request


LAB = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    grid_bytes = (LAB / "accuracy-grid.json").read_bytes()
    grid = json.loads(grid_bytes)
    cases = []
    for site in grid["sites"]:
        instant = dt.datetime.fromisoformat(site["utc"].replace("Z", "+00:00"))
        # USNO labels the time UT1. We pass integer UTC seconds because the
        # service appears to truncate fractional seconds in its calculation.
        # The DUT1 difference is retained as a comparison uncertainty, not
        # quietly absorbed into an ephemeris tolerance.
        params = {
            "date": instant.strftime("%Y-%m-%d"),
            "time": instant.strftime("%H:%M:%S"),
            "coords": f"{site['position'][0]:.10f},{site['position'][1]:.10f}",
        }
        url = "https://aa.usno.navy.mil/api/celnav?" + urllib.parse.urlencode(params)
        request = urllib.request.Request(
            url, headers={"User-Agent": "CelestialNavigation-EngineLab/1"})
        with urllib.request.urlopen(request, timeout=40) as response:
            raw = response.read()
        payload = json.loads(raw)
        if "error" in payload or "data" not in payload.get("properties", {}):
            raise RuntimeError(f"USNO response failed for {site['id']}: {payload}")
        properties = payload["properties"]
        if (properties.get("year"), properties.get("month"), properties.get("day")) != (
                instant.year, instant.month, instant.day):
            raise RuntimeError(f"USNO returned a different date for {site['id']}")
        values = {}
        for entry in properties["data"]:
            if entry["object"] in ("Sun", "Moon", "Mercury", "Venus"):
                values[entry["object"]] = entry["almanac_data"]
            if entry["object"] == "ARIES":
                values["ARIES"] = entry["almanac_data"]
        filename = f"{site['id']}.json"
        (args.output / filename).write_bytes(raw)
        cases.append({
            "id": site["id"], "utc": site["utc"],
            "position": site["position"],
            "file": filename, "sha256": hashlib.sha256(raw).hexdigest(),
            "url": url, "api_version": payload.get("apiversion"),
            "returned_time": properties.get("time"), "values": values,
        })
        print(filename, flush=True)
    manifest = {
        "schema_version": 1, "kind": "USNO_celnav_reference",
        "grid_sha256": hashlib.sha256(grid_bytes).hexdigest(),
        "retrieved_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "time_note": "USNO input is UT1; integer UTC seconds were supplied. Account for DUT1 when assessing sub-arcsecond residuals.",
        "cases": cases,
    }
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
