#!/usr/bin/env bash
set -euo pipefail
lab_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(git -C "$lab_dir" rev-parse --show-toplevel)
baseline_revision=85029280f8b224dccb55ee48f779e4373b870b82
work_dir="$lab_dir/.work"

# Refuse to overwrite either an established baseline or experimental work.
if test -e "$work_dir"; then
  echo "Already exists: $work_dir. Nothing overwritten." >&2
  exit 1
fi
git -C "$repo_dir" cat-file -e "$baseline_revision^{commit}"
mkdir -p "$work_dir/baseline/engine" "$work_dir/baseline/driver"
git -C "$repo_dir" archive "$baseline_revision" src eclipse |
  tar -x -C "$work_dir/baseline/engine"
cp "$lab_dir/CMakeLists.txt" "$lab_dir/runner.cpp" "$work_dir/baseline/driver/"
cp -a "$work_dir/baseline" "$work_dir/candidate"
(
  cd "$work_dir/baseline"
  find engine driver -type f -print0 | sort -z | xargs -0 sha256sum > ../baseline.sha256
)
printf '%s\n' "$baseline_revision" > "$work_dir/baseline-revision.txt"
echo "Frozen baseline and editable candidate created in $work_dir"
echo "No plugin sources, installed binaries or kernel files have been changed."
