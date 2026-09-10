#!/usr/bin/env python3
"""Offline baseline/candidate/reference comparison; no installed-plugin writes."""
import argparse
import datetime as dt
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
from fetch_horizons import first_row

LAB = Path(__file__).resolve().parent


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def dm(text):
    degrees, minutes = map(float, text.split(":"))
    if not (math.isfinite(degrees) and math.isfinite(minutes) and 0 <= minutes < 60):
        raise ValueError("Invalid degree/minute reading")
    return degrees + minutes / 60


def seconds(a, b):
    return (dt.datetime.fromisoformat(a.replace("Z", "+00:00")) -
            dt.datetime.fromisoformat(b.replace("Z", "+00:00"))).total_seconds()


def differences(a, b):
    result = {}
    for key in sorted(a.keys() | b.keys()):
        if key not in a or key not in b:
            result[key] = {"baseline": a.get(key), "candidate": b.get(key)}
        elif a[key] != b[key]:
            result[key] = {"baseline": a[key], "candidate": b[key]}
            if isinstance(a[key], (float, int)) and isinstance(b[key], (float, int)):
                result[key]["delta"] = b[key] - a[key]
    return result


def invoke(executable, kernel, case, mode, params=None, rows="", timezone="UTC"):
    command = [str(executable), str(kernel), case["utc"], mode,
               *(str(x) for x in case["position"])]
    command.extend(f"{key}={value}" for key, value in sorted((params or {}).items()))
    env = dict(os.environ, TZ=timezone)
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    process = subprocess.run(command, input=rows, text=True, capture_output=True,
                             env=env, timeout=120)
    if process.returncode:
        raise RuntimeError(f"{case['id']}/{mode}: {process.stderr.strip()}")
    values = {}
    for line in process.stdout.splitlines():
        key, value = line.split("\t")
        number = float(value)
        if key in values:
            raise ValueError(f"Duplicate output: {key}")
        if not math.isfinite(number):
            # Shared observations have intentionally missing duplicate residuals;
            # unconstrained uncertainties may also be infinite. Preserve status.
            if "residual_" not in key and not key.endswith("_sigma"):
                raise ValueError(f"Unexpected nonfinite output: {key}={value}")
            values[key] = value
        else:
            values[key] = number
    if not values:
        raise ValueError("Empty executable output")
    return values


def session_rows(case):
    readings = {r["id"]: r for r in case["readings"]}
    rows = []
    for distance in (r for r in case["readings"] if r["kind"] == "distance"):
        for moon_id, sun_id in (("m1", "s1"), ("m2", "s2")):
            moon, sun = readings[moon_id], readings[sun_id]
            rows.append(" ".join(map(str, [seconds(distance["utc"], case["utc"]),
                dm(distance["angle_dm"]), dm(moon["angle_dm"]), dm(sun["angle_dm"]),
                seconds(moon["utc"], distance["utc"]), seconds(sun["utc"], distance["utc"]),
                distance["id"], moon_id, sun_id])))
    return "\n".join(rows) + "\n"


def verify_manifest(root, manifest):
    expected = set()
    for line in manifest.read_text().splitlines():
        sha, relative = line.split("  ", 1)
        file = root / relative
        if not file.resolve().is_relative_to(root.resolve()):
            raise ValueError("Manifest path escapes snapshot")
        expected.add(relative)
        if not file.is_file() or digest(file) != sha:
            raise ValueError(f"Frozen baseline modified/missing: {relative}")
    actual = {str(p.relative_to(root)) for name in ("engine", "driver")
              for p in (root / name).rglob("*") if p.is_file()}
    if expected != actual:
        raise ValueError("Frozen baseline contains unrecorded files")


def verify_references(directory, references, corpus):
    grid = corpus["reference_grid"]
    cases = references["cases"]
    if len(cases) != len(grid) or not cases:
        raise ValueError("Incomplete reference grid")
    if references["policy"] != corpus["reference_policy"]:
        raise ValueError("Reference policy differs from corpus")
    for case, site in zip(cases, grid):
        if any(case.get(key) != value for key, value in site.items()):
            raise ValueError("Reference site differs from corpus")
        decoded = {}
        for provenance in case["provenance"]:
            file = directory / provenance["file"]
            if not file.resolve().is_relative_to(directory.resolve()):
                raise ValueError("Reference path escapes directory")
            if digest(file) != provenance["sha256"]:
                raise ValueError("Reference response integrity failure")
            row = first_row(json.loads(file.read_text())["result"])
            params = provenance["parameters"]
            body = {"'301'": "moon", "'10'": "sun"}[params["COMMAND"]]
            if params["CENTER"] == "'500@399'":
                decoded[f"{body}_ra_deg"] = float(row["R.A.__(a-app)"])
                decoded[f"{body}_dec_deg"] = float(row["DEC___(a-app)"])
            else:
                decoded[f"{body}_alt_deg"] = float(row["Elevation_(a-app)"])
        if len(decoded) != 6 or decoded != case["expected"]:
            raise ValueError("Expected values do not match archived Horizons rows")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kernel", type=Path, required=True)
    parser.add_argument("--references", type=Path,
                        default=LAB / "references/horizons-20260909")
    parser.add_argument("--output", type=Path, default=LAB / ".work/report.json")
    parser.add_argument("--allow-differences", action="store_true",
                        help="Research mode: report differences without requiring equivalence")
    args = parser.parse_args()
    corpus_bytes = (LAB / "corpus.json").read_bytes()
    corpus = json.loads(corpus_bytes)
    original_corpus_hash = hashlib.sha256(corpus_bytes).hexdigest()
    work = LAB / ".work"
    verify_manifest(work / "baseline", work / "baseline.sha256")
    if (work / "baseline-revision.txt").read_text().strip() != corpus["baseline_revision"]:
        raise ValueError("Unexpected baseline revision")
    if digest(args.kernel) != corpus["kernel"]["sha256"]:
        raise ValueError("Kernel does not match the pinned DE440s checksum")
    references = json.loads((args.references / "manifest.json").read_text())
    if references["corpus_sha256"] != original_corpus_hash:
        raise ValueError("Reference acquisition used a different corpus; review before updating")
    verify_references(args.references, references, corpus)
    binaries = {name: work / f"build-{name}/lunar-lab" for name in ("baseline", "candidate")}
    # Always rebuild after source edits; do not compare an accidentally stale binary.
    for name in binaries:
        subprocess.run(["cmake", "--build", str(work / f"build-{name}"), "-j", "2"], check=True)
    runs, failures = [], []

    def pair(label, case, mode, params=None, rows="", kind="regression"):
        outputs = {name: invoke(binary, args.kernel, case, mode, params, rows)
                   for name, binary in binaries.items()}
        delta = differences(outputs["baseline"], outputs["candidate"])
        entry = {"id": label, "kind": kind, "mode": mode,
                 "outputs": outputs, "differences": delta, "checks": []}
        if delta and not args.allow_differences:
            failures.append(label + ": baseline/candidate mismatch")
        runs.append(entry)
        return entry

    def check(entry, label, passed, details=None):
        entry["checks"].append({"label": label, "passed": bool(passed), "details": details})
        if not passed:
            failures.append(entry["id"] + ": " + label)

    for case in references["cases"]:
        entry = pair(case["id"], case, "reference", {"eye_height": case["height_m"]},
                     kind="independent_calculated_reference")
        for name, output in entry["outputs"].items():
            for key, expected in case["expected"].items():
                delta = output[key] - expected
                if key.endswith("_ra_deg"):
                    delta = (delta + 180) % 360 - 180
                error = delta * 3600
                policy_key = ("topocentric_tolerance_arcsec" if key.endswith("_alt_deg")
                              else "geocentric_direction_tolerance_arcsec")
                limit = corpus["reference_policy"][policy_key]
                check(entry, f"{name}/{key} versus Horizons", abs(error) <= limit,
                      {"signed_error_arcsec": error, "tolerance_arcsec": limit})

    point = corpus["observations"][0]
    params = dict(point["settings"])
    reduced = point["published_reduced_example"]
    params.update(distance=dm(reduced["distance_dm"]), moon_alt=dm(reduced["moon_alt_dm"]),
                  sun_alt=dm(reduced["sun_alt_dm"]))
    pair("point-judith-published-average", point, "solve", params,
         kind="derived_published_observation_regression")
    pair("point-judith-entered-utc", point, "position", params,
         kind="derived_published_observation_regression")
    raw = pair("point-judith-raw-watch", point, "session", point["settings"], session_rows(point),
               kind="reported_real_observations_no_absolute_truth")
    for name, output in raw["outputs"].items():
        count = sum(isinstance(value, (int, float)) for key, value in output.items()
                    if key.startswith("candidate_0_residual_"))
        check(raw, name + "/count each of eight raw readings once", count == 8)
    rows = session_rows(point)
    repeated = pair("shared-reading-duplicate-control", point, "session", point["settings"],
                    rows + rows.splitlines()[0] + "\n", kind="correlation_regression")
    for name, output in repeated["outputs"].items():
        for key in ("candidate_0_seconds", "candidate_0_time_sigma", "candidate_0_position_sigma"):
            check(repeated, name + "/duplicate must not change " + key,
                  output[key] == raw["outputs"][name][key])
    pair("point-judith-raw-watch-fixed-DR", point, "session",
         {**point["settings"], "solve_position": 0}, session_rows(point),
         kind="reported_real_observations_conditioned_on_DR_not_GNSS")

    # Freeze generated readings from the baseline before invoking either solver.
    # Self-roundtrips are internal consistency tests, never independent evidence.
    for moving in (0, 1):
        for limb in range(3):
            options = {**point["settings"], "moon_limb": limb, "sun_limb": 2-limb,
                       "moon_contact": limb, "sun_contact": 2-limb,
                       "separate_times": moving, "moon_dt": -90 * moving,
                       "sun_dt": 120 * moving, "moving": moving, "speed": 8,
                       "course": 125}
            generated = invoke(binaries["baseline"], args.kernel, point, "predict",
                               {**options, "offset": 23.75})
            observed = {**options, "distance": generated["distance_deg"],
                        "moon_alt": generated["moon_alt_deg"], "sun_alt": generated["sun_alt_deg"]}
            entry = pair(f"synthetic-limb-{limb}-moving-{moving}", point, "solve", observed,
                         kind="baseline_generated_internal_consistency")
            entry["generated_inputs"] = observed
            for name, output in entry["outputs"].items():
                matches = []
                for i in range(int(output["candidate_count"])):
                    prefix = f"candidate_{i}_"
                    for j in range(int(output[prefix + "position_count"])):
                        separation = math.hypot(output[prefix + f"{j}_lat"] - point["position"][0],
                            (output[prefix + f"{j}_lon"] - point["position"][1]) *
                            math.cos(math.radians(point["position"][0]))) * 60
                        matches.append(separation < 0.02 and abs(output[prefix + "seconds"] - 23.75) < 0.15)
                check(entry, name + "/recover injected position/time", any(matches))

    entry = pair("timezone-invariance", point, "ephemeris", {"offset": 0.123})
    for name, output in entry["outputs"].items():
        for timezone in ("Europe/London", "Pacific/Honolulu"):
            other = invoke(binaries[name], args.kernel, point, "ephemeris", {"offset": 0.123},
                           timezone=timezone)
            check(entry, name + "/" + timezone, not differences(output, other))

    # Negative control: the external altitude fixture must distinguish the
    # deliberately selected historical spherical model from WGS84.
    case = references["cases"][0]
    entry = pair("spherical-model-negative-control", case, "reference",
                 {"ellipsoid": 0, "eye_height": case["height_m"]}, kind="negative_control")
    for name, output in entry["outputs"].items():
        error = abs(output["moon_alt_deg"] - case["expected"]["moon_alt_deg"]) * 3600
        check(entry, name + "/reference detects spherical approximation", error > 2,
              {"absolute_error_arcsec": error})

    if digest(LAB / "corpus.json") != original_corpus_hash:
        raise ValueError("Raw corpus was modified during testing")
    candidate_hashes = {str(p.relative_to(work / "candidate")): digest(p)
                        for p in sorted((work / "candidate").rglob("*")) if p.is_file()}
    report = {"baseline_revision": corpus["baseline_revision"], "kernel_sha256": digest(args.kernel),
              "corpus_sha256": original_corpus_hash, "candidate_source_sha256": candidate_hashes,
              "binary_sha256": {name: digest(path) for name, path in binaries.items()},
              "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
              "equivalence_required": not args.allow_differences,
              "runs": runs, "failures": failures,
              "scope": "Sun-Moon DE440 lunar distance/session engines only; no ordinary altitude/fix, analytical fallback, planet/star lunars, GUI or on-water certification"}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
    print(f"{len(runs)} comparison scenarios; {sum(len(r['checks']) for r in runs)} checks; "
          f"{len(failures)} failures. Report: {args.output}")
    for failure in failures:
        print("FAIL:", failure)
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
