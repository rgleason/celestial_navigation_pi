#!/usr/bin/env bash
set -e

###############################################################################
# Celestial Navigation Plugin — MSVC Build Script (Git Bash)
###############################################################################

# ------------------------------------------------------------
# 1. Define paths
# ------------------------------------------------------------
PLUGIN_ROOT="$(pwd)"
BUILD_DIR="$PLUGIN_ROOT/build"
PLUGIN_BUILD="$BUILD_DIR/RelWithDebInfo"

OCPN_ROOT="/c/Users/fcgle/source/opencpn"
OCPN_BUILD="$OCPN_ROOT/build/RelWithDebInfo"
OCPN_SOLUTION="$OCPN_ROOT/build/OpenCPN.sln"

# wxWidgets (used by CMake)
wxDIR="/c/Users/fcgle/source/ocpn_wxWidgets"
wxWIN="/c/Users/fcgle/source/ocpn_wxWidgets"
export wxWidgets_ROOT_DIR="$wxWIN"
export wxWidgets_LIB_DIR="$wxWIN/lib/vc_dll"

echo "PLUGIN_ROOT: $PLUGIN_ROOT"
echo "BUILD_DIR:   $BUILD_DIR"

# ------------------------------------------------------------
# 2. Remove build directory if it exists
# ------------------------------------------------------------
if [ -d "$BUILD_DIR" ]; then
    echo "Removing existing build directory..."
    rm -rf "$BUILD_DIR"
fi

echo "Creating fresh build directory..."
mkdir -p "$BUILD_DIR"

# ------------------------------------------------------------
# 3. Run CMake configure + build
# ------------------------------------------------------------
cd "$BUILD_DIR"

echo "Configuring plugin build..."
cmake -T v143 -A Win32 -DOCPN_TARGET=MSVC "$PLUGIN_ROOT"

echo "Building plugin (RelWithDebInfo)..."
cmake --build . --config RelWithDebInfo

# ------------------------------------------------------------
# 4. Run CPack to generate tarball + XML metadata
# ------------------------------------------------------------
echo "Running CPack to generate tarball..."
cmake --build . --config RelWithDebInfo --target package

# ------------------------------------------------------------
# 5. Copy plugin DLL + PDB into OpenCPN plugin folder
# ------------------------------------------------------------
echo "Deploying plugin DLL and PDB into OpenCPN plugin directory..."

cp -f "$PLUGIN_BUILD/celestial_navigation_pi.dll" "$OCPN_BUILD/plugins/"
cp -f "$PLUGIN_BUILD/celestial_navigation_pi.pdb" "$OCPN_BUILD/plugins/"

echo "Plugin deployed successfully."

# ------------------------------------------------------------
# 6. Launch Visual Studio (optional)
# ------------------------------------------------------------
echo "Launching Visual Studio for debugging..."
# Uncomment if desired:
# cmd.exe /c "\"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe\" \"$OCPN_SOLUTION\""

echo
echo "Visual Studio is now open."
echo "Select RelWithDebInfo and press F5 to debug OpenCPN with your plugin."
echo
