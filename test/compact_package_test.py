#!/usr/bin/env python3
"""Verify the bundled qualified data and third-party notices without downloads."""
import hashlib
import json
import pathlib
import sys
root = pathlib.Path(sys.argv[1])
data = root / 'data' / 'compact'
manifest = json.loads((data / 'manifest.json').read_text())
assert manifest['component_version'] == '0.2.0'
for name, expected in manifest['files'].items():
    content = (data / name).read_bytes()
    assert len(content) == expected['bytes'], name
    assert hashlib.sha256(content).hexdigest() == expected['sha256'], name
for name in ['GPL-3.0.txt', 'ERFA.txt', 'ELP.txt']:
    assert (data / 'licenses' / name).stat().st_size > 100, name
provenance = json.loads((root / 'compact' / 'PROVENANCE.json').read_text())
for name, sha in provenance['files'].items():
    assert hashlib.sha256((root / name).read_bytes()).hexdigest() == sha, name
print('Qualified compact source/data hashes and runtime licences verified')
