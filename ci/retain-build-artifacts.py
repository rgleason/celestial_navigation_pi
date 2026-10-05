#!/usr/bin/env python3
"""Retain platform packages with source/target hashes; never deploy during builds."""
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path
from bundled_guides import check_guides

root = Path.cwd()
job = os.environ.get('CIRCLE_JOB', 'local')
out = root / 'retained-artifacts' / job
out.mkdir(parents=True, exist_ok=True)
files = []
for folder in ('build', 'artifacts'):
    base = root / folder
    if not base.exists():
        continue
    for p in base.rglob('*'):
        if not p.is_file():
            continue
        if p.name.startswith('celestial_navigation_pi-') and (p.name.endswith('.tar.gz') or p.suffix in ('.xml','.json','.exe','.dmg','.deb')):
            target = out / p.name
            if target.exists() and target.read_bytes() != p.read_bytes():
                # A packaged archive with root metadata supersedes raw CPack.
                if folder != 'artifacts':
                    raise RuntimeError('Conflicting package filenames: ' + p.name)
            shutil.copy2(p, target)
        elif folder == 'artifacts' and p.suffix == '.log':
            shutil.copy2(p, out / p.name)
for p in out.iterdir():
    if p.is_file():
        if p.name.endswith('.tar.gz'):
            check_guides(p, root)
        files.append(dict(name=p.name,sha256=hashlib.sha256(p.read_bytes()).hexdigest(),bytes=p.stat().st_size))
if not any(x['name'].endswith('.tar.gz') for x in files):
    raise RuntimeError('No platform plugin archive was retained')
sha = subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
(out / 'build-provenance.json').write_text(json.dumps(dict(source_sha=sha,job=job,
    target=os.environ.get('OCPN_TARGET',''), packages=files),indent=2)+'\n')
print(out)
