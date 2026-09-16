#!/usr/bin/env python3
"""Run all controlled ablations, retaining expected intermediate failures."""
import argparse
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from run import LAB, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kernel",type=Path,required=True)
    parser.add_argument("--summary",type=Path,default=LAB/".work/experiment-summary.json")
    args = parser.parse_args()
    stages = [("disabled",[]),
              ("dut1",["--candidate-dut1"]),
              ("dut1-diurnal",["--candidate-dut1","--candidate-diurnal-aberration"]),
              ("observer",["--candidate-dut1","--candidate-observer-astrometry"])]
    summaries = []
    baseline_checks = None
    candidate_sources = None
    directory = Path(tempfile.mkdtemp(prefix="experiment-",dir=LAB/".work"))
    for name,flags in stages:
        output = directory/f"{name}.json"
        command = [sys.executable,str(LAB/"accuracy.py"),"--kernel",str(args.kernel),
                   "--gate","candidate","--output",str(output),*flags]
        process = subprocess.run(command)
        if process.returncode not in (0,1):
            raise RuntimeError(f"{name} did not complete")
        report = json.loads(output.read_text())
        if process.returncode != (0 if report["gate_passed"] else 1) or report["execution_errors"]:
            raise RuntimeError(f"{name} has execution/report errors")
        checks = [(r["id"],c) for r in report["runs"] for c in r["checks"] if c["engine"]=="baseline"]
        if baseline_checks is not None and checks != baseline_checks:
            raise RuntimeError("Frozen baseline checks changed between ablations")
        baseline_checks = checks
        if candidate_sources is not None and candidate_sources != report["candidate_source_sha256"]:
            raise RuntimeError("Candidate source changed between ablations")
        candidate_sources = report["candidate_source_sha256"]
        if name == "disabled" and report["equivalence_mismatches"]:
            raise RuntimeError("Disabling enhancements did not restore exact equivalence")
        metrics = {}
        for r in report["runs"]:
            for c in r["checks"]:
                if "signed_error" in c:
                    key = c["engine"]+"/"+c["metric"]
                    metrics[key] = max(metrics.get(key,0),abs(c["signed_error"]))
        summaries.append({"stage":name,"flags":flags,"gate_passed":report["gate_passed"],
            "engine_summary":report["engine_summary"],"max_absolute_errors":metrics,
            "failures":report["accuracy_failures"],"ab_difference_scenarios":len(report["equivalence_mismatches"]),
            "report_file":str(output.relative_to(LAB)),"report_sha256":digest(output),
            "baseline_manifest_sha256":report["baseline_manifest_sha256"],
            "binary_sha256":report["binary_sha256"]})
    summary = {"created_utc":dt.datetime.now(dt.timezone.utc).isoformat(),
        "experiment_patch_sha256":digest(LAB/"experiments/observer-astrometry.patch"),
        "grid_sha256":digest(LAB/"accuracy-grid.json"),"kernel_sha256":digest(args.kernel),
        "reference_manifest_sha256":report["reference_manifest_sha256"],
        "dut1_manifest_sha256":report["dut1_manifest_sha256"],
        "candidate_source_sha256":candidate_sources,
        "harness_source_sha256":{**report["harness_source_sha256"],"run_experiment.py":digest(Path(__file__))},
        "baseline_unchanged_across_stages":True,"stages":summaries,
        "enhanced_candidate_passed":summaries[-1]["gate_passed"]}
    output = args.summary
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(summary,indent=2,allow_nan=False)+"\n")
    print("Controlled experiment summary:",output)
    return not summary["enhanced_candidate_passed"]


if __name__ == "__main__":
    raise SystemExit(main())
