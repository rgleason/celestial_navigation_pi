#!/usr/bin/env bash
# Add the plugin metadata to a CI-built tarball for manual OpenCPN import.
# This does not publish the package or modify the original CPack archive.
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 BUILD_DIRECTORY OUTPUT_DIRECTORY" >&2
  exit 2
fi
build_dir=$1
output_dir=$2
shopt -s nullglob
archives=("$build_dir"/*.tar.gz)
metadata=("$build_dir"/*.xml)
if [[ ${#archives[@]} -ne 1 || ${#metadata[@]} -ne 1 ]]; then
  echo "Expected exactly one .tar.gz and one .xml in $build_dir" >&2
  exit 1
fi

stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT HUP INT TERM
tar -xzf "${archives[0]}" -C "$stage"
if [[ -e "$stage/metadata.xml" ]]; then
  echo "Input archive already has metadata.xml; refusing an ambiguous package" >&2
  exit 1
fi

# A manually imported package has no catalogue download URL. OpenCPN's
# metadata parser accepts this field being absent, whereas unresolved
# Cloudsmith template placeholders would be misleading.
sed '/<tarball-url>/,/<\/tarball-url>/d' "${metadata[0]}" > "$stage/metadata.xml"
python3 - "$stage/metadata.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET

root = ET.parse(sys.argv[1]).getroot()
required = ("name", "version", "api-version", "target", "target-version", "target-arch")
if root.tag != "plugin" or any(not (root.findtext(tag) or "").strip() for tag in required):
    raise SystemExit("Incomplete OpenCPN plugin metadata")
PY

mkdir -p "$output_dir"
output="$output_dir/$(basename "${archives[0]}")"
tar -czf "$output" -C "$stage" .
python3 - "$output" <<'PY'
import sys
import tarfile
import xml.etree.ElementTree as ET

with tarfile.open(sys.argv[1], "r:gz") as archive:
    members = archive.getmembers()
    metadata = [m for m in members if m.name.lstrip("./") == "metadata.xml"]
    if len(metadata) != 1:
        raise SystemExit("Package must contain exactly one metadata.xml")
    root = ET.fromstring(archive.extractfile(metadata[0]).read())
    target = root.findtext("target", "").strip()
    extension = ".dylib" if target == "macos" or target.startswith("darwin") else ".so"
    if not any(m.name.endswith("libcelestial_navigation_pi" + extension) for m in members):
        found = [m.name for m in members if m.name.endswith((".dylib", ".so"))]
        raise SystemExit(f"Package does not contain the plugin binary for {target}: {found}")
PY
echo "Importable OpenCPN test package: $output"
