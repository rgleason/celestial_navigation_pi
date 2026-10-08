#!/usr/bin/env python3
"""Expanded offline independent accuracy suite. A red result is a finding,
not permission to retune the tolerance or edit the frozen engine.
"""
import argparse
import datetime as dt
import json
import math
import re
from pathlib import Path
import subprocess
from accuracy_reference import decode, expected_sight, separation, utc_shift, row
from fetch_accuracy import parameters
from run import LAB, digest, differences, invoke, seconds, verify_manifest


def verify_epoch(actual, utc):
    instant = dt.datetime.fromisoformat(utc.replace("Z", "+00:00"))
    months = ("Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec")
    expected = f"{instant.year:04}-{months[instant.month-1]}-{instant.day:02} {instant.hour:02}:{instant.minute:02}:{instant.second:02}"
    if re.fullmatch(r"\d{4}-[A-Za-z]{3}-\d{2} \d{2}:\d{2}", actual):
        actual += ":00"
    if actual != expected:
        raise ValueError(f"Reference row epoch mismatch: {actual} != {expected}")


def verify_accuracy_references(directory, spec):
    manifest = json.loads((directory / "manifest.json").read_text())
    if manifest["spec_sha256"] != digest(LAB / "accuracy-grid.json"):
        raise ValueError("Reference specification checksum mismatch")
    if not manifest["cases"] or len(manifest["cases"]) != len(spec["sites"]):
        raise ValueError("Incomplete accuracy grid")
    result = {}
    seen = set()
    for case, site in zip(manifest["cases"], spec["sites"]):
        if any(case.get(k) != v for k,v in site.items()) or site["id"] in seen:
            raise ValueError("Unexpected or duplicate reference site")
        seen.add(site["id"])
        decoded = {}
        required = {b+"_"+f for b in ("moon","sun") for f in ("geocentric","topocentric")}
        if site["height_m"] == 0:
            required.update({"moon_refracted", "sun_refracted"})
        if set(case["responses"]) != required:
            raise ValueError("Reference frames missing or unexpected")
        for label, key in case["responses"].items():
            metadata = manifest["responses"][key]
            file = directory / metadata["file"]
            if not file.resolve().is_relative_to(directory.resolve()) or digest(file) != metadata["sha256"]:
                raise ValueError("Reference integrity failure")
            body, frame = label.split("_")
            if metadata["parameters"] != parameters(site, 301 if body == "moon" else 10, frame):
                raise ValueError("Reference query mismatch")
            values = decode(json.loads(file.read_text()), frame == "refracted")
            if values != metadata["decoded"]:
                raise ValueError("Expected values changed relative to raw response")
            verify_epoch(values["utc_label"], site["utc"])
            decoded[label] = values
        result[case["id"]] = decoded
    return result


def inverse_eligibility(moon, sun, policy):
    altitudes = [moon["alt"],sun["alt"]]
    distance = separation(moon["ra"],moon["dec"],sun["ra"],sun["dec"])
    crossing = abs(math.sin(math.radians(moon["az"]-sun["az"])))
    reasons = []
    if min(altitudes) < policy["inverse_min_altitude_deg"] or max(altitudes) > policy["inverse_max_altitude_deg"]:
        reasons.append("altitudes outside predeclared inverse-test range")
    if not policy["inverse_min_separation_deg"] <= distance <= policy["inverse_max_separation_deg"]:
        reasons.append("separation outside predeclared inverse-test range")
    if crossing < policy["inverse_min_abs_sin_azimuth_crossing"]:
        reasons.append("weak altitude-circle crossing geometry")
    return reasons


def position_error_nm(position, truth):
    # Angular error converted to conventional 60 NM/degree; adequate at this
    # suite's 0.1 NM gate, and independent of either engine's distance routine.
    return separation(position[1],position[0],truth[1],truth[0])*60


def verify_dut1_references(directory, spec):
    epochs = json.loads((directory/"manifest.json").read_text())["epochs"]
    values = {}
    for site in spec["sites"]:
        meta = epochs[site["utc"]]
        file = directory/meta["file"]
        if not file.resolve().is_relative_to(directory.resolve()) or digest(file) != meta["sha256"]:
            raise ValueError("DUT1 reference integrity failure")
        requested = parameters(site,301,"geocentric")
        requested["QUANTITIES"] = "'2,49'"
        if meta["parameters"] != requested:
            raise ValueError("DUT1 request does not match site epoch")
        raw_row = row(json.loads(file.read_text())["result"])
        verify_epoch(next(v for k,v in raw_row.items() if k.startswith("Date__")), site["utc"])
        value = float(raw_row["UT1-UTC"])
        if not math.isfinite(value) or value != meta["dut1_seconds"]:
            raise ValueError("DUT1 value does not match raw response")
        values[site["id"]] = value
    return values


def dut1_diagnostics(directory, spec, external, runs, production=False):
    values = verify_dut1_references(directory, spec)
    result = []
    by_id = {r["id"]:r for r in runs}
    for site in spec["sites"]:
        value = values[site["id"]]
        for name,out in by_id[site["id"]+"/airless"]["outputs"].items():
            if production and name=="candidate": continue
            if "error" in out: continue
            if by_id[site["id"]+"/airless"].get("engine_inputs",{}).get(name,{}).get("dut1") is not None:
                # The omitted-term projection is only meaningful for an
                # engine run which has actually omitted that input.
                continue
            for body in ("moon","sun"):
                reference = external[site["id"]][body+"_topocentric"]
                error = (out[body+"_alt_deg"]-reference["alt"])*3600
                # d(h)/d(theta) = cos(latitude)*sin(azimuth). This is a
                # first-order explanatory projection, NOT an adjusted engine
                # output or a replacement for any failing accuracy check.
                predicted = -15.041067*value*math.cos(math.radians(site["position"][0]))*math.sin(math.radians(reference["az"]))
                result.append({"site":site["id"],"engine":name,"body":body,"dut1_seconds":value,
                    "actual_altitude_error_arcsec":error,"predicted_omitted_dut1_arcsec":predicted,
                    "unexplained_after_projection_arcsec":error-predicted})
    return result


def verify_raytrace_table(spec, source):
    pairs = [[float(a),float(b)] for a,b in re.findall(
        r"^\*\*\s+(\d+)\s+(\d+\.\d+)\s+\d+\.\d+\s+\d+\.\d+\s*$",
        source.read_text(),re.MULTILINE)]
    if pairs != spec["raytrace_refraction"]["observed_zenith_deg_and_refraction_arcsec"]:
        raise ValueError("Raytrace table does not match published source")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kernel", type=Path, required=True)
    parser.add_argument("--references", type=Path, default=LAB/"references/accuracy-20260909")
    parser.add_argument("--output", type=Path, default=LAB/".work/accuracy-report.json")
    parser.add_argument("--candidate-dut1", action="store_true",
                        help="Supply verified, dated Earth-orientation input only to the experimental adapter")
    parser.add_argument("--candidate-diurnal-aberration", action="store_true",
                        help="Enable the candidate's station-velocity correction")
    parser.add_argument("--candidate-observer-astrometry", action="store_true",
                        help="Recompute light time and aberration at each candidate station")
    parser.add_argument("--gate", choices=("both", "candidate"), default="both",
                        help="Candidate gate retains baseline failures and A/B differences in the report but gates only candidate accuracy and execution")
    parser.add_argument("--production-build",type=Path,
                        help="Compare the frozen baseline with the actual production target in this CMake build")
    args = parser.parse_args()
    if args.candidate_observer_astrometry and args.candidate_diurnal_aberration:
        parser.error("Observer astrometry already includes diurnal aberration")
    if args.production_build and (args.candidate_dut1 or args.candidate_diurnal_aberration or args.candidate_observer_astrometry):
        parser.error("Production obtains dated DUT1 automatically; lab feature flags do not apply")
    spec = json.loads((LAB/"accuracy-grid.json").read_text())
    corpus = json.loads((LAB/"corpus.json").read_text())
    work = LAB/".work"
    verify_manifest(work/"baseline", work/"baseline.sha256")
    if (work/"baseline-revision.txt").read_text().strip() != corpus["baseline_revision"]:
        raise ValueError("Unexpected baseline revision")
    if digest(args.kernel) != corpus["kernel"]["sha256"]:
        raise ValueError("Unexpected kernel")
    external = verify_accuracy_references(args.references, spec)
    dut1 = verify_dut1_references(LAB/"references/dut1-20260909", spec)
    verify_raytrace_table(spec,work/"baseline/engine/eclipse/third_party/erfa/src/refco.c")
    binaries = {name:work/f"build-{name}/lunar-lab" for name in ("baseline","candidate")}
    if args.production_build:
        args.production_build=args.production_build.resolve()
        binaries["candidate"]=args.production_build/"test/lunar_production_lab"
    probes = {}
    for name in binaries:
        production = name=="candidate" and args.production_build
        build = args.production_build if production else work/f"build-{name}"
        subprocess.run(["cmake","--build",str(build),"-j","2"]+
                       (["--target","lunar_production_lab"] if production else []), check=True)
        probes[name] = work/f"build-{name}/component-probe"
        # A new diagnostic executable; do not edit either snapshot's driver.
        subprocess.run(["c++","-std=c++17","-O2",str(LAB/"component_probe.cpp"),
                        str((LAB.parents[1] if production else work/name/"engine")/"src/LunarDistanceEngine.cpp"),
                        "-I"+str((LAB.parents[1] if production else work/name/"engine")/"src"),"-o",str(probes[name])], check=True)
    runs, coverage, errors = [], [], []
    policy = spec["policy"]

    def pair(label, site, mode, params, category):
        outputs = {}
        engine_inputs = {}
        for name,binary in binaries.items():
            engine_inputs[name] = dict(params)
            if name == "candidate" and args.candidate_dut1:
                engine_inputs[name]["dut1"] = dut1[site["id"]]
            if name == "candidate" and args.candidate_diurnal_aberration:
                engine_inputs[name]["diurnal_aberration"] = 1
            if name == "candidate" and args.candidate_observer_astrometry:
                engine_inputs[name]["observer_astrometry"] = 1
            try:
                outputs[name] = invoke(binary,args.kernel,site,mode,engine_inputs[name])
            except (RuntimeError, ValueError, subprocess.TimeoutExpired) as error:
                outputs[name] = {"error":str(error)}
                errors.append(f"{label}/{name}: {error}")
        entry = {"id":label,"category":category,"utc":site["utc"],"position":site["position"],
                 "mode":mode,"inputs":params,"outputs":outputs,
                 "engine_inputs":engine_inputs,
                 "differences":differences(outputs["baseline"],outputs["candidate"]),"checks":[]}
        runs.append(entry)
        return entry

    def check(entry, name, metric, actual, expected, tolerance, units="arcsec", circular=False):
        delta = actual-expected
        if circular:
            delta = (delta+180)%360-180
        if units == "arcsec":
            delta *= 3600
        entry["checks"].append({"engine":name,"metric":metric,"signed_error":delta,
            "units":units,"limit":tolerance,"passed":math.isfinite(delta) and abs(delta)<=tolerance})

    for site in spec["sites"]:
        ref = external[site["id"]]
        moon,sun = ref["moon_topocentric"],ref["sun_topocentric"]
        settings = {"eye_height":site["height_m"],"artificial_horizon":1,"pressure":0,
                    "index_error":0,"moon_limb":1,"sun_limb":1,"moon_contact":1,"sun_contact":1}
        vacuum = pair(site["id"]+"/airless",site,"reference",settings,"astrometry_parallax")
        expected_distance = separation(moon["ra"],moon["dec"],sun["ra"],sun["dec"])
        for name,output in vacuum["outputs"].items():
            if "error" in output: continue
            for body in ("moon","sun"):
                for axis in ("ra","dec"):
                    check(vacuum,name,body+"_"+axis,output[f"{body}_{axis}_deg"],ref[body+"_geocentric"][axis],
                          policy["geocentric_direction_arcsec"],circular=axis=="ra")
                check(vacuum,name,body+"_altitude",output[body+"_alt_deg"],ref[body+"_topocentric"]["alt"],
                      policy["topocentric_altitude_arcsec"])
            check(vacuum,name,"topocentric_separation",output["distance_deg"],expected_distance,
                  policy["topocentric_separation_arcsec"])

        # Isolate disc radii by differencing centre/limb predictions. A whole
        # altitude discrepancy due to DUT1 must not be mislabelled as SD error.
        if 1 < min(moon["alt"],sun["alt"]) and max(moon["alt"],sun["alt"]) < 89 and 2 < expected_distance < 175:
            for limb in (0,2):
                options = {**settings,"moon_limb":limb,"sun_limb":2-limb,
                           "moon_contact":limb,"sun_contact":limb}
                entry = pair(f"{site['id']}/limb-{limb}",site,"predict",options,"airless_limb_contact")
                sign = (-1,0,1)[limb]
                for name,output in entry["outputs"].items():
                    base = vacuum["outputs"][name]
                    if "error" in output or "error" in base: continue
                    check(entry,name,"moon_limb_increment",output["moon_alt_deg"]/2-base["moon_alt_deg"],
                          sign*moon["diameter"]/7200,policy["limb_increment_arcsec"])
                    check(entry,name,"sun_limb_increment",output["sun_alt_deg"]/2-base["sun_alt_deg"],
                          -sign*sun["diameter"]/7200,policy["limb_increment_arcsec"])
                    check(entry,name,"distance_contact_increment",output["distance_deg"]-base["distance_deg"],
                          sign*(moon["diameter"]+sun["diameter"])/7200,policy["limb_increment_arcsec"])

        if "moon_refracted" in ref:
            valid_bodies = [body for body in ("moon","sun")
                            if policy["refraction_min_altitude_deg"] <= ref[body+"_topocentric"]["alt"] < 85]
            if valid_bodies:
                entry = pair(site["id"]+"/atmosphere",site,"predict",
                             {**settings,"pressure":1010,"temperature":10},"standard_atmosphere_comparison")
                for name,output in entry["outputs"].items():
                    base = vacuum["outputs"][name]
                    if "error" in output or "error" in base: continue
                    for body in valid_bodies:
                        check(entry,name,body+"_refraction_increment",
                              output[body+"_alt_deg"]/2-base[body+"_alt_deg"],
                              ref[body+"_refracted"]["alt"]-ref[body+"_topocentric"]["alt"],
                              policy["refraction_model_difference_arcsec"])

        reasons = inverse_eligibility(moon,sun,policy)
        if site["id"] in ("before-2016-leap", "after-2016-leap"):
            reasons.append("inverse scan crosses a leap second: elapsed-watch leap handling is outside the frozen adapter's domain")
        coverage.append({"site":site["id"],"utc":site["utc"],"position":site["position"],
            "moon_alt_deg":moon["alt"],"sun_alt_deg":sun["alt"],"separation_deg":expected_distance,
            "moon_geocentric_diameter_arcsec":ref["moon_geocentric"]["diameter"],"inverse_exclusions":reasons})
        if not reasons:
            # True epoch is fixed by the external reference; change only the
            # recorded watch label, never any externally supplied angle.
            recorded = {**site,"utc":utc_shift(site["utc"],-23)}
            for limb in (0,1,2):
                expected = expected_sight(moon,sun,limb,limb,limb)
                options = {**settings,**expected,"moon_limb":limb,"sun_limb":2-limb,
                           "moon_contact":limb,"sun_contact":limb,"pressure":1e-6}
                # Production SolveTime rejects P=0 even though Predict accepts
                # it. Use the vacuum limit, not a change to the frozen engine.
                # Quantify this tiny approximation separately below.
                entry = pair(f"{site['id']}/recover-{limb}",recorded,"solve",options,"external_sight_inverse")
                assess_inverse(entry,site,check,policy)
        print("Compared",site["id"],flush=True)

    sequence = spec["sequential_case"]
    site = next(s for s in spec["sites"] if s["id"]==sequence["distance_site"])
    central = external[site["id"]]
    earlier = external[sequence["moon_site"]]["moon_topocentric"]
    later = external[sequence["sun_site"]]["sun_topocentric"]
    index = {s["id"]:s for s in spec["sites"]}
    observed = expected_sight(central["moon_topocentric"],central["sun_topocentric"],1,1,1)
    observed.update(moon_alt=2*earlier["alt"],sun_alt=2*later["alt"])
    params = {**observed,"eye_height":site["height_m"],"artificial_horizon":1,"pressure":1e-6,
        "moon_limb":1,"sun_limb":1,"moon_contact":1,"sun_contact":1,"index_error":0,
        "separate_times":1,"moon_dt":seconds(index[sequence["moon_site"]]["utc"],site["utc"]),
        "sun_dt":seconds(index[sequence["sun_site"]]["utc"],site["utc"])}
    entry = pair("external-sequential-recovery",{**site,"utc":utc_shift(site["utc"],-23)},
                 "solve",params,"external_sight_inverse")
    assess_inverse(entry,site,check,policy)

    atmosphere = spec["raytrace_refraction"]
    # Independent inverse inputs are vacuum angles. Establish that the strictly
    # positive pressure required by the public solver is immaterial here.
    entry = {"id":"inverse-vacuum-limit","category":"harness_approximation_bound","outputs":{},"checks":[]}
    for name,probe in probes.items():
        p = subprocess.run([str(probe),"4.5","0.000001","10"],check=True,
                           capture_output=True,text=True,timeout=10)
        actual = float(p.stdout)
        entry["outputs"][name] = {"apparent_altitude":actual}
        check(entry,name,"negligible_positive_pressure",actual,4.5,0.000001)
    entry["differences"] = differences(entry["outputs"]["baseline"],entry["outputs"]["candidate"])
    runs.append(entry)
    for zd,refr in atmosphere["observed_zenith_deg_and_refraction_arcsec"]:
        observed_altitude = 90-zd
        vacuum_altitude = observed_altitude-refr/3600
        entry = {"id":f"raytrace-zd-{zd}","category":"published_raytrace_refraction",
                 "outputs":{},"checks":[]}
        for name,probe in probes.items():
            process = subprocess.run([str(probe),str(vacuum_altitude),str(atmosphere["pressure_hpa"]),
                                      str(atmosphere["temperature_c"])],check=True,capture_output=True,text=True,timeout=10)
            actual = float(process.stdout)
            entry["outputs"][name] = {"apparent_altitude":actual}
            check(entry,name,"refraction_from_published_raytrace",actual,observed_altitude,atmosphere["tolerance_arcsec"])
        entry["differences"] = differences(entry["outputs"]["baseline"],entry["outputs"]["candidate"])
        runs.append(entry)

    failures = [{"id":r["id"],**c} for r in runs for c in r["checks"] if not c["passed"]]
    mismatches = [r["id"] for r in runs if r["differences"]]
    engine_summary = {name:{"checks":sum(c["engine"]==name for r in runs for c in r["checks"]),
                            "accuracy_failures":sum(c["engine"]==name for c in failures)}
                      for name in binaries}
    gate_failed = bool(errors or (failures or mismatches if args.gate == "both" else
                                  any(c["engine"] == "candidate" for c in failures)))
    # Keep a complete report even when a genuine accuracy check is red.
    report = {"production_build":str(args.production_build) if args.production_build else None,
        "production_source_sha256":{str(p.relative_to(LAB.parents[1])):digest(p)
            for directory in (LAB.parents[1]/"src",LAB.parents[1]/"eclipse")
            for p in directory.rglob("*") if p.is_file() and p.suffix in (".h",".cpp",".c",".inc")} if args.production_build else None,
        "baseline_revision":corpus["baseline_revision"],"created_utc":dt.datetime.now(dt.timezone.utc).isoformat(),
        "grid_sha256":digest(LAB/"accuracy-grid.json"),"reference_manifest_sha256":digest(args.references/"manifest.json"),
        "kernel_sha256":digest(args.kernel),"baseline_manifest_sha256":digest(work/"baseline.sha256"),
        "harness_source_sha256":{p:digest(LAB/p) for p in
                                 ("accuracy.py","accuracy_reference.py","run.py","fetch_accuracy.py",
                                  "production_runner.cpp","adapter_tests.cpp")},
        "dut1_manifest_sha256":digest(LAB/"references/dut1-20260909/manifest.json"),
        "candidate_dut1_enabled":args.candidate_dut1,
        "candidate_diurnal_aberration_enabled":args.candidate_diurnal_aberration,
        "candidate_observer_astrometry_enabled":args.candidate_observer_astrometry,
        "gate":args.gate,"gate_passed":not gate_failed,"engine_summary":engine_summary,
        "binary_sha256":{n:digest(b) for n,b in binaries.items()},
        "probe_sha256":{n:digest(b) for n,b in probes.items()},"probe_source_sha256":digest(LAB/"component_probe.cpp"),
        "candidate_source_sha256":None if args.production_build else {
            str(p.relative_to(work/"candidate")):digest(p)
            for p in sorted((work/"candidate").rglob("*")) if p.is_file()},
        "dut1_diagnostics":dut1_diagnostics(LAB/"references/dut1-20260909",spec,external,runs,bool(args.production_build)),
        "policy":policy,"coverage":coverage,"runs":runs,"accuracy_failures":failures,
        "execution_errors":errors,"equivalence_mismatches":mismatches,
        "limits":["Sun-Moon engines only; optional candidate refinements are explicitly recorded above",
                  ("Production selects/interpolates bundled IERS DUT1 per epoch; no lab-supplied DUT1. No leap-crossing inverse support is claimed"
                   if args.production_build else "DUT1 is a dated external input held constant over each short fixture; no general EOP interpolation or leap-crossing inverse support is claimed"),
                  "Inverse inputs use 1e-6 hPa because the frozen solver rejects zero pressure; the vacuum-limit approximation is separately bounded",
                  "No claim of real-observation absolute accuracy: the recorded Point Judith DR is not independent GNSS truth",
                  "Independent theoretical inverse sights use JPL, not either engine's forward model",
                  "No physical sea-horizon dip, refracted limb contact, moving-observer or planet/star benchmark in this stage"]}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2,allow_nan=False)+"\n")
    print(f"{len(runs)} scenarios, {sum(len(r['checks']) for r in runs)} checks; "
          f"{len(failures)} accuracy failures, {len(errors)} execution errors, {len(mismatches)} A/B differences.")
    print("Report:",args.output)
    print(f"{args.gate} gate: {'FAIL' if gate_failed else 'PASS'}; per-engine:",engine_summary)
    return gate_failed


def assess_inverse(entry, site, check, policy):
    for name,output in entry["outputs"].items():
        if "error" in output: continue
        solutions = []
        for i in range(int(output["candidate_count"])):
            for j in range(int(output[f"candidate_{i}_position_count"])):
                position = [output[f"candidate_{i}_{j}_lat"],output[f"candidate_{i}_{j}_lon"]]
                solutions.append((position_error_nm(position,site["position"]),output[f"candidate_{i}_seconds"],position))
        if not solutions:
            entry["checks"].append({"engine":name,"metric":"solution_exists","passed":False})
            continue
        error,correction,position = min(solutions)
        entry.setdefault("nearest_reference_branch",{})[name] = {"position":position,"position_error_nm":error,
                                                               "clock_correction_seconds":correction}
        # Select the branch nearest independent reference position, then assess
        # BOTH position and time there (not two independently chosen branches).
        check(entry,name,"position_recovery",error,0,policy["position_nm"],units="nm")
        check(entry,name,"clock_recovery",correction,23,policy["clock_seconds"],units="seconds")


if __name__ == "__main__":
    raise SystemExit(main())
