#!/usr/bin/env python3
"""Explicit, opt-in reference acquisition; never used by offline test runs.

Keep the exact queries and complete responses, rather than copying unexplained
numbers. Refuse to overwrite an existing reference set. Uses only stdlib.
"""
import argparse
import csv
import datetime as dt
import hashlib
import json
from pathlib import Path
import urllib.parse
import urllib.request

LAB = Path(__file__).resolve().parent


def first_row(result):
    before, after = result.split("$$SOE", 1)
    table, _ = after.split("$$EOE", 1)
    headers = next(line for line in reversed(before.splitlines()) if "R.A.__(a-app)" in line)
    values = next(csv.reader([table.strip().splitlines()[0]], skipinitialspace=True))
    names = next(csv.reader([headers], skipinitialspace=True))
    return dict(zip((name.strip() for name in names), (value.strip() for value in values)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    # mkdir with exist_ok=False prevents accidental reference replacement.
    args.output.mkdir(parents=True, exist_ok=False)
    corpus_bytes = (LAB / "corpus.json").read_bytes()
    corpus = json.loads(corpus_bytes)
    cache = {}
    cases = []
    for site in corpus["reference_grid"]:
        expected = {}
        provenance = []
        instant = dt.datetime.fromisoformat(site["utc"].replace("Z", "+00:00"))
        for body, target in (("moon", "301"), ("sun", "10")):
            for center in ("geocentric", "topocentric"):
                params = {
                    "format": "json", "COMMAND": f"'{target}'",
                    "EPHEM_TYPE": "'OBSERVER'", "CENTER": "'500@399'",
                    "START_TIME": "'" + instant.strftime("%Y-%m-%d %H:%M:%S") + "'",
                    "STOP_TIME": "'" + (instant + dt.timedelta(minutes=1)).strftime("%Y-%m-%d %H:%M:%S") + "'",
                    "STEP_SIZE": "'1m'", "QUANTITIES": "'2,13'",
                    "APPARENT": "'AIRLESS'", "ANG_FORMAT": "'DEG'",
                    "EXTRA_PREC": "'YES'", "CSV_FORMAT": "'YES'",
                }
                if center == "topocentric":
                    params.update(CENTER="'coord@399'", COORD_TYPE="'GEODETIC'",
                        SITE_COORD=f"'{site['position'][1]},{site['position'][0]},{site['height_m']/1000}'",
                        QUANTITIES="'2,4,13'")
                query = urllib.parse.urlencode(params)
                if query not in cache:
                    url = "https://ssd.jpl.nasa.gov/api/horizons.api?" + query
                    request = urllib.request.Request(url, headers={"User-Agent": "CelestialNavigation-EngineLab/1"})
                    with urllib.request.urlopen(request, timeout=40) as response:
                        raw = response.read()
                    payload = json.loads(raw)
                    if "error" in payload:
                        raise RuntimeError(payload["error"])
                    result = payload["result"]
                    row = first_row(result)
                    filename = f"{site['id']}-{body}-{center}.json"
                    (args.output / filename).write_bytes(raw)
                    meta = {"file": filename, "sha256": hashlib.sha256(raw).hexdigest(),
                            "url": url, "parameters": params,
                            "retrieved_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
                            "api_signature": payload.get("signature"),
                            "model_header": [line.strip() for line in result.splitlines()
                                if line.startswith(("Target body name:", "EOP file", "Center geodetic", "Atmos refraction"))]}
                    cache[query] = row, meta
                    print(filename, flush=True)
                row, meta = cache[query]
                provenance.append(meta)
                if center == "geocentric":
                    expected[f"{body}_ra_deg"] = float(row["R.A.__(a-app)"])
                    expected[f"{body}_dec_deg"] = float(row["DEC___(a-app)"])
                else:
                    expected[f"{body}_alt_deg"] = float(row["Elevation_(a-app)"])
        cases.append({**site, "expected": expected, "provenance": provenance})
    manifest = {"schema_version": 1, "kind": "independent_calculated_reference",
                "corpus_sha256": hashlib.sha256(corpus_bytes).hexdigest(),
                "policy": corpus["reference_policy"], "cases": cases}
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
