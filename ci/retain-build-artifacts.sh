#!/usr/bin/env bash
set -euo pipefail
# CircleCI Windows supplies python.exe/py.exe rather than the Unix python3
# alias. Require Python 3 and use the same provenance script on all targets.
for interpreter in python3 python py; do
  command -v "$interpreter" >/dev/null 2>&1 || continue
  # macOS Bash 3.2 treats an empty array as unset under nounset.
  if [[ $interpreter == py ]]; then
    if "$interpreter" -3 -c 'import sys; sys.exit(sys.version_info.major != 3)' >/dev/null 2>&1; then
      exec "$interpreter" -3 ci/retain-build-artifacts.py
    fi
  elif "$interpreter" -c 'import sys; sys.exit(sys.version_info.major != 3)' >/dev/null 2>&1; then
    exec "$interpreter" ci/retain-build-artifacts.py
  fi
done
echo 'Python 3 is required to retain platform packages and source provenance.' >&2
exit 1
