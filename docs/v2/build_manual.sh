#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
output_dir="$script_dir/output"
source_html="$script_dir/Celestial_Navigation_Manual_v2.html"
plugin_html="$output_dir/Celestial_Navigation_Information.html"
plugin_data_dir="$(cd "$script_dir/../.." && pwd)/data"

python3 "$script_dir/generate_diagrams.py"
mkdir -p "$output_dir/images"
cp "$script_dir"/images/*.png "$output_dir/images/"
mkdir -p "$plugin_data_dir/images"
python3 "$script_dir/prepare_plugin_html.py"

lo_profile_docx="$(mktemp -d /tmp/celnav-lo-docx.XXXXXX)"
lo_profile_pdf="$(mktemp -d /tmp/celnav-lo-pdf.XXXXXX)"
trap 'rm -rf "$lo_profile_docx" "$lo_profile_pdf"' EXIT

libreoffice "-env:UserInstallation=file://$lo_profile_docx" --headless \
  --convert-to 'docx:Office Open XML Text' --outdir "$lo_profile_docx" \
  "$source_html"
cp "$lo_profile_docx/Celestial_Navigation_Manual_v2.docx" \
  "$output_dir/Celestial_Navigation_Manual_v2.docx"

python3 "$script_dir/embed_docx_images.py" \
  "$output_dir/Celestial_Navigation_Manual_v2.docx"

libreoffice "-env:UserInstallation=file://$lo_profile_pdf" --headless \
  --convert-to pdf --outdir "$lo_profile_pdf" \
  "$output_dir/Celestial_Navigation_Manual_v2.docx"
cp "$lo_profile_pdf/Celestial_Navigation_Manual_v2.pdf" \
  "$output_dir/Celestial_Navigation_Manual_v2.pdf"

cp "$output_dir/Celestial_Navigation_Manual_v2.pdf" \
  "$plugin_data_dir/Celestial_Navigation_Manual_v2.pdf"

python3 "$script_dir/validate_manual.py"

printf '%s\n' \
  "$plugin_html" \
  "$output_dir/Celestial_Navigation_Manual_v2.docx" \
  "$output_dir/Celestial_Navigation_Manual_v2.pdf"
