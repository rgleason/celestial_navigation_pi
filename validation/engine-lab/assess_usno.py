#!/usr/bin/env python3
"""Compare the standalone DE440s engine with archived USNO navigation data.

This is a diagnostic report, not an acceptance gate: the two products may use
different dynamical ephemerides or apparent-place conventions. Never tune a
numerical model to force agreement with a rounded printed value.
"""

import argparse
import datetime as dt
import hashlib
import json
import math
from pathlib import Path
import subprocess


LAB = Path(__file__).resolve().parent
TARGETS = {"Sun": 10, "Moon": 301, "Mercury": 199, "Venus": 299}


def circular_difference(a, b):
    return (a - b + 180.0) % 360.0 - 180.0


def run_engine(binary, kernel, target, instant, site, dut1="auto", tai="auto"):
    command = [str(binary), str(kernel), str(target),
               instant.strftime("%Y-%m-%dT%H:%M:%S.%f"),
               str(site[0]), str(site[1]), "0", str(dut1), str(tai)]
    return json.loads(subprocess.check_output(command, text=True))


def inferred_moon_tt_offset(binary, kernel, instant, site, reference):
    """Fit only the ephemeris-time argument while holding UT1 fixed.

    This diagnoses a time-scale difference; it is not a suggested production
    correction or an independent lunar reference.  Both GHA and declination
    must agree after the *same* offset to support that interpretation.
    """
    base = run_engine(binary, kernel, 301, instant, site, dut1=0)
    step = run_engine(binary, kernel, 301, instant, site, dut1=0,
                      tai=base["tai_minus_utc_seconds"] + 1)
    gha_rate = circular_difference(step["gha_deg"], base["gha_deg"]) * 3600
    dec_rate = (step["declination_deg"] - base["declination_deg"]) * 3600
    gha_error = circular_difference(reference["gha"], base["gha_deg"]) * 3600
    dec_error = (reference["dec"] - base["declination_deg"]) * 3600
    gha_weight = math.cos(math.radians(reference["dec"])) ** 2
    offset = ((gha_weight * gha_error * gha_rate + dec_error * dec_rate) /
              (gha_weight * gha_rate ** 2 + dec_rate ** 2))
    fitted = run_engine(binary, kernel, 301, instant, site, dut1=0,
                        tai=base["tai_minus_utc_seconds"] + offset)
    residual_gha = circular_difference(fitted["gha_deg"], reference["gha"]) * 3600
    residual_dec = (fitted["declination_deg"] - reference["dec"]) * 3600
    return offset, residual_gha, residual_dec


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--references", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--kernel", type=Path, required=True)
    args = parser.parse_args()
    manifest = json.loads((args.references / "manifest.json").read_text())
    rows = []
    for case in manifest["cases"]:
        raw = (args.references / case["file"]).read_bytes()
        if hashlib.sha256(raw).hexdigest() != case["sha256"]:
            raise RuntimeError(f"Archived USNO response changed: {case['id']}")
        utc = dt.datetime.fromisoformat(case["utc"].replace("Z", "+00:00"))
        if case["id"] in ("before-2016-leap", "after-2016-leap"):
            continue  # UTC leap second needs explicit non-POSIX handling.
        for name, data in case["values"].items():
            if name not in TARGETS:
                continue
            first = run_engine(args.binary, args.kernel, TARGETS[name],
                               utc, case["position"])
            # The USNO API argument is UT1; solve UTC = input UT1 - DUT1.
            matched_utc = utc - dt.timedelta(seconds=first["dut1_seconds"])
            calculated = run_engine(args.binary, args.kernel, TARGETS[name],
                                    matched_utc, case["position"])
            gha_error = circular_difference(calculated["gha_deg"], data["gha"]) * 3600
            dec_error = (calculated["declination_deg"] - data["dec"]) * 3600
            rows.append((case["id"], name, gha_error, dec_error,
                         round(calculated["gha_deg"] * 60, 1) == round(data["gha"] * 60, 1),
                         round(calculated["declination_deg"] * 60, 1) == round(data["dec"] * 60, 1)))
    print("case\tbody\tGHA difference (arcsec)\tDec difference (arcsec)\tprinted GHA same\tprinted Dec same")
    for row in rows:
        print(f"{row[0]}\t{row[1]}\t{row[2]:+.3f}\t{row[3]:+.3f}\t{row[4]}\t{row[5]}")
    for name in TARGETS:
        selected = [row for row in rows if row[1] == name]
        if selected:
            max_gha = max(abs(row[2]) for row in selected)
            max_dec = max(abs(row[3]) for row in selected)
            same_gha = sum(row[4] for row in selected)
            same_dec = sum(row[5] for row in selected)
            print(f"{name}: n={len(selected)}, max |GHA|={max_gha:.3f}\", "
                  f"max |Dec|={max_dec:.3f}\", rounded GHA {same_gha}/{len(selected)}, "
                  f"rounded Dec {same_dec}/{len(selected)}")
    print("\nMoon time-scale diagnostic (unique requested UT1 epochs):")
    print("case\tinferred additional TT seconds\tGHA residual after fit (arcsec)\tDec residual after fit (arcsec)")
    seen = set()
    for case in manifest["cases"]:
        if "Moon" not in case["values"] or case["utc"] in seen:
            continue
        seen.add(case["utc"])
        instant = dt.datetime.fromisoformat(case["utc"].replace("Z", "+00:00"))
        offset, gha, dec = inferred_moon_tt_offset(
            args.binary, args.kernel, instant, case["position"],
            case["values"]["Moon"])
        print(f"{case['id']}\t{offset:+.3f}\t{gha:+.3f}\t{dec:+.3f}")


if __name__ == "__main__":
    main()
