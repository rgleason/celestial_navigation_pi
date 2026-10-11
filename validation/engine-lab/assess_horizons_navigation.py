#!/usr/bin/env python3
"""Compare the offline DE440s planet-centre engine with archived JPL Horizons.

Horizons supplies geocentric apparent RA/Dec and WGS84 airless topocentric
altitude. The comparison uses the *geometric centre* of Venus; the additional
navigational centre-of-light phase correction is assessed against USNO.
"""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


TARGETS = {"mercury": 199, "venus": 299}


def circular_difference(a, b):
    return (a - b + 180.0) % 360.0 - 180.0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--references", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--kernel", type=Path, required=True)
    args = parser.parse_args()
    manifest = json.loads((args.references / "manifest.json").read_text())
    for case in manifest["cases"]:
        for item in case["provenance"]:
            raw = (args.references / item["file"]).read_bytes()
            if hashlib.sha256(raw).hexdigest() != item["sha256"]:
                raise RuntimeError(f"JPL response changed: {item['file']}")
    errors = {name: {"ra": [], "dec": [], "alt": []} for name in TARGETS}
    print("site\tbody\tRA difference (arcsec)\tDec difference (arcsec)\tairless altitude difference (arcsec)")
    for case in manifest["cases"]:
        for name, target in TARGETS.items():
            command = [str(args.binary), str(args.kernel), str(target),
                       case["utc"].replace("Z", ""),
                       str(case["position"][0]), str(case["position"][1]),
                       str(case["height_m"]), "auto", "auto"]
            state = json.loads(subprocess.check_output(command, text=True))
            ra = (state["gha_aries_deg"] - state["centre_gha_deg"]) % 360.0
            observed = case["expected"]
            dra = circular_difference(ra, observed[f"{name}_ra_deg"]) * 3600
            ddec = (state["centre_declination_deg"] -
                    observed[f"{name}_dec_deg"]) * 3600
            dalt = (state["airless_topocentric_altitude_deg"] -
                    observed[f"{name}_alt_deg"]) * 3600
            for key, value in (("ra", dra), ("dec", ddec), ("alt", dalt)):
                errors[name][key].append(abs(value))
            print(f"{case['id']}\t{name}\t{dra:+.4f}\t{ddec:+.4f}\t{dalt:+.4f}")
    for name, values in errors.items():
        print(f"{name}: max |RA|={max(values['ra']):.4f}\", "
              f"max |Dec|={max(values['dec']):.4f}\", "
              f"max |alt|={max(values['alt']):.4f}\"")


if __name__ == "__main__":
    main()
