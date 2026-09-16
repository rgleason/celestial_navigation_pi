#!/usr/bin/env bash
# Fetch immutable, checksum-pinned eclipse test/runtime data from the public
# GitHub release. The default downloads DE440s only; --all also retrieves the
# optional lunar-orientation and LOLA refinement packs.
set -euo pipefail

release_url="https://github.com/pob220/celestial_navigation_pi/releases/download/eclipse-data-2026.1"
output_dir=${1:-eclipse/data}
mode=${2:-}

if [[ -n "$mode" && "$mode" != "--all" ]]; then
  echo "Usage: $0 [output-directory] [--all]" >&2
  exit 2
fi

mkdir -p "$output_dir"

fetch_verified() {
  local name=$1
  local expected=$2
  local destination="$output_dir/$name"
  local temporary
  temporary=$(mktemp "$output_dir/.${name}.XXXXXX")

  if ! curl --fail --location --retry 3 --retry-all-errors \
      --connect-timeout 30 --max-time 1800 \
      --output "$temporary" "$release_url/$name"; then
    rm -f "$temporary"
    return 1
  fi
  if ! printf '%s  %s\n' "$expected" "$temporary" \
      | sha256sum --check --strict; then
    rm -f "$temporary"
    return 1
  fi
  mv -f "$temporary" "$destination"
  printf 'Installed verified %s\n' "$destination"
}

fetch_verified de440s.bsp \
  c1c7feeab882263fc493a9d5a5b2ddd71b54826cdf65d8d17a76126b260a49f2

if [[ "$mode" == "--all" ]]; then
  fetch_verified moon_pa_de440_200625.bpc \
    60cd55aa401ea2ea97360636f567554bfe4e37bb829f901b4460a455dfaf783f
  fetch_verified lola64-pa.bin \
    f59edf8437442b05525345b3c29b65f0f31af8fc96420abf2dd18af3480f7ff4
fi
