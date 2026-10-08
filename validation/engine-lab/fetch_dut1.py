#!/usr/bin/env python3
"""Read-only acquisition of dated DUT1 diagnostics, separate from accuracy data."""
import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
import urllib.parse
import urllib.request
from accuracy_reference import row
from fetch_accuracy import LAB, parameters


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output",type=Path,required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=False)
    spec = json.loads((LAB/"accuracy-grid.json").read_text())
    results = {}
    for site in spec["sites"]:
        if site["utc"] in results: continue
        p = parameters(site,301,"geocentric")
        p["QUANTITIES"] = "'2,49'"
        url = "https://ssd.jpl.nasa.gov/api/horizons.api?"+urllib.parse.urlencode(p)
        with urllib.request.urlopen(url,timeout=40) as response:
            raw = response.read()
        payload = json.loads(raw)
        if "error" in payload: raise RuntimeError(payload["error"])
        values = row(payload["result"])
        dut1 = float(values["UT1-UTC"])
        filename = site["id"]+".json"
        (args.output/filename).write_bytes(raw)
        results[site["utc"]] = {"file":filename,"sha256":hashlib.sha256(raw).hexdigest(),
            "url":url,"parameters":p,"dut1_seconds":dut1,
            "retrieved_utc":dt.datetime.now(dt.timezone.utc).isoformat()}
        print(site["utc"],dut1,flush=True)
    (args.output/"manifest.json").write_text(json.dumps({"kind":"independent_dut1_diagnostic",
                                                        "epochs":results},indent=2)+"\n")


if __name__ == "__main__":
    main()
