#!/usr/bin/env python3
"""Explicit acquisition of a new expanded reference set; never overwrites one."""
import argparse
import concurrent.futures
import datetime as dt
import hashlib
import json
from pathlib import Path
import urllib.parse
import urllib.request
from accuracy_reference import decode, utc_shift

LAB = Path(__file__).resolve().parent


def parameters(site, target, frame):
    utc = site["utc"].replace("T", " ").rstrip("Z")
    stop = utc_shift(site["utc"], 60).replace("T", " ").rstrip("Z")
    p = {"format":"json", "COMMAND":f"'{target}'", "EPHEM_TYPE":"'OBSERVER'",
         "CENTER":"'500@399'", "START_TIME":f"'{utc}'", "STOP_TIME":f"'{stop}'",
         "STEP_SIZE":"'1m'", "QUANTITIES":"'2,13'", "APPARENT":"'AIRLESS'",
         "ANG_FORMAT":"'DEG'", "EXTRA_PREC":"'YES'", "CSV_FORMAT":"'YES'",
         "TIME_DIGITS":"'SECONDS'"}
    if frame != "geocentric":
        lat, lon = site["position"]
        p.update(CENTER="'coord@399'", COORD_TYPE="'GEODETIC'", QUANTITIES="'2,4,13'",
                 SITE_COORD=f"'{lon},{lat},{site['height_m']/1000}'")
    if frame == "refracted":
        p["APPARENT"] = "'REFRACTED'"
    return p


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    spec_bytes = (LAB / "accuracy-grid.json").read_bytes()
    spec = json.loads(spec_bytes)
    requests, cases = {}, []
    for site in spec["sites"]:
        links = {}
        for body, target in (("moon",301),("sun",10)):
            for frame in ("geocentric", "topocentric", "refracted"):
                # The Horizons atmosphere is sea-level only. Elevated sites
                # remain airless benchmarks; do not assume local pressure.
                if frame == "refracted" and site["height_m"] != 0:
                    continue
                p = parameters(site, target, frame)
                query = urllib.parse.urlencode(p)
                key = hashlib.sha256(query.encode()).hexdigest()[:24]
                requests[key] = p
                links[body + "_" + frame] = key
        cases.append({**site, "responses":links})

    def fetch(item):
        key, params = item
        url = "https://ssd.jpl.nasa.gov/api/horizons.api?" + urllib.parse.urlencode(params)
        request = urllib.request.Request(url, headers={"User-Agent":"CelestialNavigation-EngineLab/2"})
        with urllib.request.urlopen(request, timeout=45) as response:
            raw = response.read()
        payload = json.loads(raw)
        if "error" in payload:
            raise RuntimeError(payload["error"])
        values = decode(payload, params["APPARENT"] == "'REFRACTED'")
        filename = key + ".json"
        (args.output / filename).write_bytes(raw)
        print(filename, flush=True)
        return key, {"file":filename, "sha256":hashlib.sha256(raw).hexdigest(),
                     "parameters":params, "url":url, "decoded":values,
                     "retrieved_utc":dt.datetime.now(dt.timezone.utc).isoformat(),
                     "api_signature":payload.get("signature")}

    # At most two outstanding read-only requests, to avoid hammering the service.
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        responses = dict(pool.map(fetch, requests.items()))
    manifest = {"schema_version":1, "kind":"independent_calculated_accuracy_reference",
                "spec_sha256":hashlib.sha256(spec_bytes).hexdigest(),
                "cases":cases, "responses":responses}
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2)+"\n")
    print(f"Saved {len(responses)} responses for {len(cases)} sites")


if __name__ == "__main__":
    main()
