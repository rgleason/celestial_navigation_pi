#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != Darwin ]]; then
  echo "This packaging script requires macOS." >&2
  exit 2
fi

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${repo_dir}/build-celnav-lab-macos"
artifact_dir="${repo_dir}/artifacts/celnav-lab"
mkdir -p "$build_dir" "$artifact_dir"

git -C "$repo_dir" submodule update --init

for package in cmake gettext libexif openssl@3 googletest librsvg wget; do
  if ! brew list --versions "$package" >/dev/null 2>&1; then
    brew install "$package"
  fi
done

# Match the existing OpenCPN Mac plugin build environment.  This archive
# supplies the universal wxWidgets/OpenCPN dependencies; nothing is deleted.
deps_archive="${build_dir}/macos_deps_universal.tar.xz"
if [[ ! -f "$deps_archive" ]]; then
  curl --fail --location --retry 3 \
    'https://dl.cloudsmith.io/public/nohal/opencpn-plugins/raw/files/macos_deps_universal.tar.xz' \
    --output "$deps_archive"
fi
sudo tar -C /usr/local -xJf "$deps_archive"

iconset="${build_dir}/CelestialNavigationLab.iconset"
mkdir -p "$iconset"
for spec in '16 16x16' '32 16x16@2x' '32 32x32' \
            '64 32x32@2x' '128 128x128' '256 128x128@2x' \
            '256 256x256' '512 256x256@2x' '512 512x512' \
            '1024 512x512@2x'; do
  read -r pixels name <<< "$spec"
  rsvg-convert -w "$pixels" -h "$pixels" "$repo_dir/lab/icon.svg" \
    -o "$iconset/icon_${name}.png"
done
icon="${build_dir}/CelestialNavigationLab.icns"
iconutil -c icns "$iconset" -o "$icon"

cmake -S "$repo_dir" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX= \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' \
  '-DOCPN_TARGET_TUPLE=darwin-wx32;10;universal' \
  -DOCPN_BUILD_TEST=ON \
  -DCELESTIAL_BUILD_LAB=ON \
  -DLAB_ICON_ICNS="$icon"

cmake --build "$build_dir" --target celestial_navigation_lab --parallel 2
app="${build_dir}/test/Celestial Navigation Lab.app"
test -f "$app/Contents/Resources/data/vsop87d.txt"
file "$app/Contents/MacOS/Celestial Navigation Lab"
architectures="$(lipo -archs "$app/Contents/MacOS/Celestial Navigation Lab")"
echo "Architectures: $architectures"
if [[ " $architectures " != *" arm64 "* ||
      " $architectures " != *" x86_64 "* ]]; then
  echo "The Lab executable is not universal." >&2
  exit 1
fi
"$app/Contents/MacOS/Celestial Navigation Lab" --smoke-test
test_kernel="${build_dir}/de440s.bsp"
curl --fail --location --retry 3 \
  'https://github.com/pob220/celestial_navigation_pi/releases/download/eclipse-data-2026.1/de440s.bsp' \
  --output "$test_kernel"
expected_kernel_sha='c1c7feeab882263fc493a9d5a5b2ddd71b54826cdf65d8d17a76126b260a49f2'
actual_kernel_sha="$(shasum -a 256 "$test_kernel" | awk '{print $1}')"
if [[ "$actual_kernel_sha" != "$expected_kernel_sha" ]]; then
  echo "The DE440s test kernel failed SHA-256 verification." >&2
  exit 1
fi
CELNAV_LAB_DE440_PATH="$test_kernel" \
  "$app/Contents/MacOS/Celestial Navigation Lab" --smoke-test
cmake -DLAB_APP="$app" -P "$repo_dir/lab/fixup-bundle.cmake"
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict --verbose=2 "$app"

stage="$(mktemp -d "${build_dir}/dmg-stage.XXXXXX")"
trap 'rm -rf "$stage"' EXIT
cp -R "$app" "$stage/"
ln -s /Applications "$stage/Applications"
dmg="${artifact_dir}/Celestial-Navigation-Lab-0.1.0-macOS-universal.dmg"
hdiutil create -volname 'Celestial Navigation Lab 0.1' \
  -srcfolder "$stage" -format UDZO "$dmg"
(cd "$artifact_dir" && shasum -a 256 "$(basename "$dmg")" \
  > "$(basename "$dmg").sha256")
echo "Built: $dmg"
