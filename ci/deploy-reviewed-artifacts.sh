#!/usr/bin/env bash
# Publish only artifacts retained by the parameter- and approval-gated workflow.
set -euo pipefail
set +x

if [[ -z "${CLOUDSMITH_API_KEY:-}" ]]; then
  echo "CLOUDSMITH_API_KEY is not configured for the deployment context." >&2
  exit 2
fi
if [[ -z "${CELESTIAL_CLOUDSMITH_REPO:-}" ]]; then
  echo "CELESTIAL_CLOUDSMITH_REPO must name the approved owner/repository." >&2
  exit 2
fi

artifact_root=${CELESTIAL_DEPLOY_ARTIFACT_ROOT:-retained-artifacts}
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT HUP INT TERM
published=0

while IFS= read -r -d '' archive; do
  package_dir=$(dirname "$archive")
  metadata=""
  for candidate in "$package_dir"/*.xml; do
    if [[ -f "$candidate" ]] && grep -q '<plugin version=' "$candidate"; then
      metadata=$candidate
      break
    fi
  done
  if [[ -z "$metadata" ]]; then
    echo "No plugin metadata XML found beside $archive" >&2
    exit 1
  fi

  plugin_version=$(sed -n 's:.*<version>[[:space:]]*\([^[:space:]<]*\)[[:space:]]*</version>.*:\1:p' "$metadata")
  target=$(sed -n 's:.*<target>[[:space:]]*\([^[:space:]<]*\)[[:space:]]*</target>.*:\1:p' "$metadata")
  target_version=$(sed -n 's:.*<target-version>[[:space:]]*\([^[:space:]<]*\)[[:space:]]*</target-version>.*:\1:p' "$metadata")
  target_arch=$(sed -n 's:.*<target-arch>[[:space:]]*\([^[:space:]<]*\)[[:space:]]*</target-arch>.*:\1:p' "$metadata")
  if [[ ! "$plugin_version" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Invalid or missing plugin version in $metadata" >&2
    exit 1
  fi
  test -n "$target"
  test -n "$target_version"
  test -n "$target_arch"

  short_sha=${CIRCLE_SHA1:-$(git rev-parse HEAD)}
  short_sha=${short_sha:0:7}
  cloudsmith_version="${plugin_version}+${CIRCLE_BUILD_NUM:-0}.${short_sha}"
  identity=$(printf '%s-%s-%s' "$target" "$target_arch" "$target_version" \
    | tr '/_ ' '---')
  package_name="celestial_navigation_pi-${plugin_version}-${identity}-tarball"
  metadata_name="celestial_navigation_pi-${plugin_version}-${identity}-metadata"

  work="$stage/$published"
  mkdir -p "$work/unpacked"
  staged_archive="$work/$(basename "$archive")"
  cp "$archive" "$staged_archive"
  gzip -d "$staged_archive"
  staged_tar=${staged_archive%.gz}
  tar -xf "$staged_tar" -C "$work/unpacked"
  rm -rf "$work/unpacked/root"

  staged_xml="$work/metadata.xml"
  sed -e "s|--pkg_repo--|$CELESTIAL_CLOUDSMITH_REPO|g" \
      -e "s|--name--|$package_name|g" \
      -e "s|--version--|$cloudsmith_version|g" \
      -e "s|--filename--|$(basename "$archive")|g" \
      "$metadata" > "$staged_xml"
  cp "$staged_xml" "$work/unpacked/metadata.xml"
  tar -C "$work/unpacked" -cf "$staged_tar" .
  gzip "$staged_tar"

  cloudsmith push raw --republish --no-wait-for-sync \
    --name "$metadata_name" --version "$cloudsmith_version" \
    --summary "Celestial Navigation OpenCPN alpha metadata for $identity" \
    "$CELESTIAL_CLOUDSMITH_REPO" "$staged_xml"
  cloudsmith push raw --republish --no-wait-for-sync \
    --name "$package_name" --version "$cloudsmith_version" \
    --summary "Celestial Navigation OpenCPN alpha package for $identity" \
    "$CELESTIAL_CLOUDSMITH_REPO" "$staged_archive"
  published=$((published + 1))
done < <(find "$artifact_root" -type f -name '*.tar.gz' -print0)

if (( published == 0 )); then
  echo "No retained .tar.gz packages found under $artifact_root" >&2
  exit 1
fi
echo "Published $published reviewed Celestial Navigation package set(s)."
