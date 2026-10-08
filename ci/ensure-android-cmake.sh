#!/usr/bin/env bash
# Source from build-android.sh. Kitware CMake binaries retain their bundled
# copyright/license files. Checksum: upstream v3.31.6 SHA-256 manifest.
celnav_prepare_cmake() {
  local tools_dir=$1
  local version=3.31.6
  local digest=5a1133ff103c71eb5120e2cc3de922733e7d8a26a98ae716397e8676adb367bf
  local archive="$tools_dir/cmake-$version-linux-x86_64.tar.gz"
  local install_dir="$tools_dir/cmake-$version-linux-x86_64"
  mkdir -p "$tools_dir"
  if [[ ! -f "$archive" ]]; then
    curl --fail --location --retry 3 \
      "https://github.com/Kitware/CMake/releases/download/v$version/cmake-$version-linux-x86_64.tar.gz" \
      --output "$archive"
  fi
  printf '%s  %s\n' "$digest" "$archive" | sha256sum --check
  if [[ ! -x "$install_dir/bin/cmake" ]]; then
    tar --extract --gzip --file "$archive" --directory "$tools_dir"
  fi
  export PATH="$install_dir/bin:$PATH"
  [[ $(cmake --version | head -1) == "cmake version $version" ]]
}
