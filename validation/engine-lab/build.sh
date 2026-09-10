#!/usr/bin/env bash
set -euo pipefail
lab_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
if ! test -e "$lab_dir/.work"; then
  bash "$lab_dir/setup.sh"
fi
for variant in baseline candidate; do
  cmake -S "$lab_dir/.work/$variant/driver" -B "$lab_dir/.work/build-$variant" \
    -DENGINE_SOURCE="$lab_dir/.work/$variant/engine" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$lab_dir/.work/build-$variant" --parallel 2
done
