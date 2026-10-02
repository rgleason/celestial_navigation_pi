#!/usr/bin/env bash
# Exercise the actual Android UTC/angle adapters on a host with Qt and wxBase.
set -euo pipefail
source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=${CELESTIAL_BOUNDARY_TEST_DIR:-"$source_dir/build-android-boundaries"}
mkdir -p "$test_dir"
# -UNDEBUG ensures assertions run even if a caller passes release flags.
for family in utc angle download; do
  sources=("$source_dir/test/android_${family}_boundary.cpp")
  if [[ "$family" == utc ]]; then sources+=("$source_dir/src/TimeStatus.cpp"); fi
  c++ -std=c++17 -fPIC -UNDEBUG -D__OCPN__ANDROID__ \
    $(wx-config --cxxflags) $(pkg-config --cflags Qt5Core) \
    "${sources[@]}" -o "$test_dir/$family" \
    $(wx-config --libs base) $(pkg-config --libs Qt5Core)
  "$test_dir/$family" 2>&1 | tee "$test_dir/$family.log"
done
