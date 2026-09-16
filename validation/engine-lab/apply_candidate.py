#!/usr/bin/env python3
"""Apply the saved experiment only to a verified duplicate; never the baseline."""
import json
from pathlib import Path
import subprocess
from run import LAB, digest, verify_manifest


def apply(candidate=None):
    work = LAB/".work"
    candidate = candidate or work/"candidate"
    baseline = work/"baseline"
    verify_manifest(baseline,work/"baseline.sha256")
    if candidate.is_symlink() or not candidate.resolve().is_relative_to(work.resolve()) or candidate.resolve().is_relative_to(baseline.resolve()):
        raise ValueError("Destination must be a physical experimental copy inside .work")
    manifest = json.loads((LAB/"experiments/observer-astrometry.json").read_text())["files"]
    original = {str(p.relative_to(baseline)):digest(p) for p in baseline.rglob("*") if p.is_file()}
    enhanced = dict(original)
    for path,sha in manifest.items():
        if original[path] != sha["baseline"]:
            raise ValueError("Experiment is for a different frozen baseline")
        enhanced[path] = sha["candidate"]
    if any(p.is_symlink() for p in candidate.rglob("*")):
        raise ValueError("Experimental source copy must not contain symlinks")
    actual = {str(p.relative_to(candidate)):digest(p) for p in candidate.rglob("*") if p.is_file()}
    if actual == enhanced:
        print("Saved observer-astrometry experiment already applied; nothing overwritten.")
        return
    if actual != original:
        raise ValueError("Candidate contains other changes; refusing to overwrite experimental work")
    directory = str(candidate.resolve().relative_to(LAB.parents[1].resolve()))
    patch = str(LAB/"experiments/observer-astrometry.patch")
    command = ["git","apply","--directory="+directory,patch]
    subprocess.run(command[:2]+["--check"]+command[2:],cwd=LAB.parents[1],check=True)
    subprocess.run(command,cwd=LAB.parents[1],check=True)
    actual = {str(p.relative_to(candidate)):digest(p) for p in candidate.rglob("*") if p.is_file()}
    if actual != enhanced:
        raise ValueError("Applied experiment does not match saved source checksums")
    verify_manifest(baseline,work/"baseline.sha256")
    print("Applied observer-astrometry experiment to",candidate)


if __name__ == "__main__":
    apply()
