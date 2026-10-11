#!/usr/bin/env bash
# Exercise the actual Android UTC/angle adapters on a host with Qt and wxBase.
set -euo pipefail
source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=${CELESTIAL_BOUNDARY_TEST_DIR:-"$source_dir/build-android-boundaries"}
mkdir -p "$test_dir"
# -UNDEBUG ensures assertions run even if a caller passes release flags.
for family in utc angle download planner_time; do
  sources=("$source_dir/test/android_${family}_boundary.cpp")
  if [[ "$family" == utc ]]; then sources+=("$source_dir/src/TimeStatus.cpp"); fi
  if [[ "$family" == planner_time ]]; then sources+=("$source_dir/src/NavigationAlgorithms.cpp"); fi
  c++ -std=c++17 -fPIC -ffunction-sections -fdata-sections -UNDEBUG -D__OCPN__ANDROID__ \
    -I"$source_dir/src" -I"$source_dir/include" -I"$source_dir/opencpn-libs/api-18" \
    -I"$source_dir/src/plugin_dc/dc_utils/include" -I"$source_dir/eclipse/include" -I"$source_dir/compact/include" \
    $(wx-config --cxxflags) $(pkg-config --cflags Qt5Core) \
    "${sources[@]}" -o "$test_dir/$family" \
    -Wl,--gc-sections $(wx-config --libs base) $(pkg-config --libs Qt5Core)
  "$test_dir/$family" 2>&1 | tee "$test_dir/$family.log"
done
