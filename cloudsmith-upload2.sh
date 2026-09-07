#!/usr/bin/env bash
set -e

PLUGIN_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PLUGIN_ROOT/build"

cd "$BUILD_DIR"

XML_FILE=$(ls celestial_navigation_pi-*.xml)
TARBALL=$(ls celestial_navigation_pi-*.tar.gz)

echo "Using XML:      $XML_FILE"
echo "Using TARBALL:  $TARBALL"

# 1. Prepare metadata.xml inside repack/
gunzip -c "$TARBALL" > inner.tar

rm -rf repack
mkdir repack
tar -xf inner.tar -C repack

# Copy metadata.xml INTO the repack directory (CRITICAL FIX)
cp "$XML_FILE" repack/metadata.xml

# 4. Identify plugin directory inside inner tar
PLUGIN_DIR=$(ls repack | grep celestial_navigation_pi | head -n 1)
echo "Plugin directory inside tar: $PLUGIN_DIR"

# 5. Rebuild inner tar with metadata.xml + plugin directory
tar -cf new_inner.tar -C repack metadata.xml "$PLUGIN_DIR"

# 6. Compress new inner tar back into tar.gz
gzip -c new_inner.tar > "$TARBALL"

# 7. Cleanup
rm -rf repack inner.tar new_inner.tar

echo "SUCCESS: metadata.xml inserted into inner tar."
echo "Tarball rebuilt correctly."
