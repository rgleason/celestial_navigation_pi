#!/usr/bin/env bash
set -euo pipefail

abi=${1:?usage: build-android.sh arm64|armhf}
case "$abi" in
  arm64) core_target=Android-arm64; toolchain=android-aarch64; machine=AArch64; compiler=aarch64-linux-android21-clang++ ;;
  armhf) core_target=Android-armhf; toolchain=android-armhf; machine=ARM; compiler=armv7a-linux-androideabi21-clang++ ;;
  *) echo "Unsupported Android ABI" >&2; exit 2 ;;
esac
source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
work_dir=${ANDROID_BUILD_WORKDIR:-"$source_dir/.android-ci/$abi"}
sdk_root=${ANDROID_SDK_ROOT:-"$work_dir/android-sdk"}
core_source=${ANDROID_CORE_SOURCE:-"$work_dir/OpenCPN-5.14"}
core_build=${ANDROID_CORE_BUILD:-"$work_dir/core-build"}
plugin_build=${ANDROID_PLUGIN_BUILD:-"$work_dir/plugin-build"}
support_cache=${ANDROID_SUPPORT_CACHE:-"$work_dir/support-cache"}
artifacts="$source_dir/artifacts/android-$abi"
package_name=celestial_navigation_pi
ndk_version=26.1.10909125
core_commit=91f3b674366068a6ecd61a5e9aba204bba85f57e
support_sha256=c4110c532e9a0bcf071bbd10fe6f7627d7e91380c803c52ac0e89ce5f993db9b
# Google repository2-3.xml: cmdline-tools;12.0, Linux, 153607504 bytes.
# The upstream SHA1 d313adb7aedccf6cf0cfca51ec180f0059f5f8f8 was
# independently verified before recording this stronger archive digest.
tools_sha256=2d2d50857e4eb553af5a6dc3ad507a17adf43d115264b1afc116f95c92e5e258

mkdir -p "$work_dir" "$support_cache" "$artifacts/package"
source "$source_dir/ci/ensure-android-cmake.sh"
celnav_prepare_cmake "$work_dir/tools"
cmake --version | tee "$artifacts/cmake-version.log"

if [[ -z "${NDK_HOME:-}" ]]; then
  sdkmanager="$sdk_root/cmdline-tools/latest/bin/sdkmanager"
  if [[ ! -x "$sdkmanager" ]]; then
    tools_zip="$work_dir/commandlinetools-linux-11076708_latest.zip"
    curl --fail --location --retry 3 \
      https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip \
      --output "$tools_zip"
    printf '%s  %s\n' "$tools_sha256" "$tools_zip" | sha256sum --check
    mkdir -p "$sdk_root/cmdline-tools/latest"
    unzip -q "$tools_zip" -d "$work_dir/cmdline-tools-unpacked"
    cp -a "$work_dir/cmdline-tools-unpacked/cmdline-tools/." \
      "$sdk_root/cmdline-tools/latest/"
  fi
  set +o pipefail
  yes | "$sdkmanager" --sdk_root="$sdk_root" --licenses >/dev/null
  set -o pipefail
  "$sdkmanager" --sdk_root="$sdk_root" "ndk;$ndk_version"
  NDK_HOME="$sdk_root/ndk/$ndk_version"
fi
test -x "$NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/$compiler"
grep -q "Pkg.Revision = $ndk_version" "$NDK_HOME/source.properties"
export NDK_HOME OCPN_TARGET=android-$abi
tool_base="$NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64"

if ! git -C "$core_source" rev-parse --git-dir >/dev/null 2>&1; then
  git clone --depth 1 --branch Release_5.14.0 \
    https://github.com/OpenCPN/OpenCPN.git "$core_source"
fi
test "$(git -C "$core_source" rev-parse HEAD)" = "$core_commit"

support_zip="$support_cache/support.zip"
if [[ ! -s "$support_zip" ]]; then
  curl --fail --location --retry 3 \
    https://github.com/bdbcat/OCPNAndroidCoreBuildSupport/releases/download/v1.2/OCPNAndroidCoreBuildSupport.zip \
    --output "$support_zip"
fi
printf '%s  %s\n' "$support_sha256" "$support_zip" | sha256sum --check

# Always test an extraction from the verified archive, not a repaired Qt cache.
rm -rf "$support_cache/OCPNAndroidCoreBuildSupport"
unzip -q "$support_zip" -d "$support_cache"

if [[ -z "${OCPN_ANDROID_CORE_LIBRARY:-}" ]]; then
# OpenCPN 5.14 unconditionally downloads and extracts this 311 MB archive
# during CMake configure. Use the verified cache above on fresh CI machines.
python3 - "$core_source/libs/AndroidLibs.cmake" \
  "$core_source/buildandroid/build_android.cmake" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
source = path.read_text()
for old, new in (
    ('if (TRUE) #(NOT EXISTS ${OCPN_ANDROID_CACHEDIR}/support.zip)',
     'if (NOT EXISTS ${OCPN_ANDROID_CACHEDIR}/support.zip)'),
    ('if (TRUE) #(NOT EXISTS ${_master_base})',
     'if (NOT EXISTS ${_master_base})'),
):
    if old in source:
        source = source.replace(old, new, 1)
    elif new not in source:
        raise SystemExit(f'Unexpected OpenCPN Android cache logic in {path}')
path.write_text(source)

# NDK 26 ships llvm-ar; the legacy aarch64-linux-android-ar alias is absent
# on fresh SDK installs, making the first static-library link fail.
path = Path(sys.argv[2])
source = path.read_text()
old = 'set(CMAKE_AR ${tool_base}/bin/aarch64-linux-android-ar)'
new = 'set(CMAKE_AR ${tool_base}/bin/llvm-ar)'
if old in source:
    source = source.replace(old, new, 1)
elif new not in source:
    raise SystemExit(f'Unexpected OpenCPN Android archiver in {path}')
source = source.replace('set(CMAKE_AR ${tool_base}/bin/arm-linux-androideabi-ar)', new)
path.write_text(source)
PY

cmake -S "$core_source" -B "$core_build" \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_TOOLCHAIN_FILE="$core_source/buildandroid/build_android.cmake" \
  "-DOCPN_TARGET_TUPLE:STRING=$core_target;33;$abi" \
  -Dtool_base="$tool_base" \
  -DOCPN_ANDROID_CACHEDIR="$support_cache" \
  -DCMAKE_BUILD_TYPE=Release 2>&1 | tee "$artifacts/configure-core.log"
cmake --build "$core_build" --target lunasvg \
  --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-3}" 2>&1 | tee "$artifacts/build-core-lunasvg.log"
cmake --build "$core_build" --target gorp \
  --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-3}" 2>&1 | tee "$artifacts/build-core.log"

OCPN_ANDROID_CORE_LIBRARY="$core_build/libgorp.so"
fi
test -f "$OCPN_ANDROID_CORE_LIBRARY"
"$tool_base/bin/llvm-readelf" -h "$OCPN_ANDROID_CORE_LIBRARY" | grep -E "Machine:.*$machine"
"$tool_base/bin/llvm-readelf" -d "$OCPN_ANDROID_CORE_LIBRARY" | grep -E 'SONAME.*libgorp.so'

support_root="$support_cache/OCPNAndroidCoreBuildSupport"
# The pinned support archive has Qt 5.12.2 forwarding headers but omits the
# public math3d source headers. Restore the matching, unmodified upstream
# headers explicitly; a developer's populated cache must not be required.
(cd "$source_dir/ci/android-qt-headers" && sha256sum --check SHA256SUMS)
mkdir -p "$support_root/qt5/qtbase/src/gui/math3d"
cp "$source_dir/ci/android-qt-headers/"qvector*.h \
  "$support_root/qt5/qtbase/src/gui/math3d/"
test -f "$support_root/wxWidgets/libs/$abi/lib/wx/include/arm-linux-androideabi-qt-unicode-static-3.1/wx/setup.h"
git -C "$source_dir" submodule update --init opencpn-libs

cmake -S "$source_dir" -B "$plugin_build" \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_TOOLCHAIN_FILE="$source_dir/cmake/$toolchain-toolchain.cmake" \
  -D_wx_selected_config=androideabi-qt-$abi \
  -DOCPN_Android_Common="$support_root" \
  -DOCPN_ANDROID_CORE_LIBRARY="$OCPN_ANDROID_CORE_LIBRARY" \
  -DCMAKE_BUILD_TYPE=Release \
  2>&1 | tee "$artifacts/configure-plugin.log"
cmake --build "$plugin_build" \
  --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-3}" \
  2>&1 | tee "$artifacts/build-plugin.log"
cmake --build "$plugin_build" --target package 2>&1 | tee "$artifacts/package.log"
shopt -s nullglob
packages=("$plugin_build"/"$package_name"-*-android-$abi.tar.gz)
metadata=("$plugin_build"/"$package_name"-*-android-$abi.xml)
test "${#packages[@]}" -eq 1
test "${#metadata[@]}" -eq 1
filename=$(basename "${packages[0]}")
plugin_version=$(python3 -c 'import sys, xml.etree.ElementTree as E; print(E.parse(sys.argv[1]).findtext("version"))' "${metadata[0]}")
base_url=${CELESTIAL_ANDROID_TARBALL_BASE_URL:-https://github.com/pob220/celestial_navigation_pi/releases/download/android-v${plugin_version}-alpha1}
python3 "$source_dir/ci/package-android-import.py" "${packages[0]}" "${metadata[0]}" \
  "$artifacts/package/$filename" --url "$base_url/$filename" --source-sha "$(git -C "$source_dir" rev-parse HEAD)" \
  --library "$plugin_build/lib${package_name}.so"
cp "$plugin_build/lib${package_name}.so" "$artifacts/lib${package_name}-unstripped.so"
"$tool_base/bin/llvm-readelf" -h "$plugin_build/lib${package_name}.so" | grep -E "Machine:.*$machine"
"$tool_base/bin/llvm-readelf" -d "$plugin_build/lib${package_name}.so" | tee "$artifacts/library-dependencies.txt"
(cd "$artifacts/package" && sha256sum ./*.tar.gz ./*.xml > SHA256SUMS)
