#!/bin/sh
set -eu

root=${1:-.}
limit_gib=${2:-90}
used_kib=$(du -sk "$root" | awk '{print $1}')
limit_kib=$((limit_gib * 1024 * 1024))
if [ "$used_kib" -gt "$limit_kib" ]; then
  echo "Storage budget exceeded: ${used_kib} KiB used, ${limit_kib} KiB allowed" >&2
  exit 1
fi
echo "Storage budget OK: ${used_kib} KiB used of ${limit_kib} KiB"
