#!/bin/bash

# FE2 Testplugin

# Use ${bold}"./build-win.sh c"${normal} to run cmake.
# Use "./build.sh c pi" to run cmake and copy a DLL for testing.
# Adjust this command for your setup and Plugin.
# Requires wxWidgets setup
# - /home/fcgle/source/wxWidgets-3.1.2
# - /home/fcgle/source/wxWidgets-3.2.2
# - /home/fcgle/source/ is where all the plugins and OpenCPN repos are kept.
# Visual Studio 15 2017 installed
# Visual Studio 17 2022 installed
# --------------------------------------
# For Opencpn 5.8 and wxWidgets-3.2.2 using Visual Studio 15 2017
# --------------------------------------
# Used for local builds and testing.
# Create an empty "[plugin]/build" directory
# Use a terminal from the [plugin] root directory: "./bld.sh"
# Find the errors in the build/output.txt file
# Then use bash prompt to run cloudsmith-upload.sh command: "bash ./bld.sh"
# This adds the metadata file to the tarball gz file.
# Set local environment to find and use wxWidgets-3.2.2

# Enable command tracing

set -x 

# wxWidgets settings 

wxDIR=$WXWIN
wxWidgets_ROOT_DIR=$WXWIN
wxWidgets_LIB_DIR="$WXWIN/lib/vc14x_dll"
WXWIN="/home/fcgle/source/ocpn-wxWidgets"

# build the plugin with cmake

cd build
cmake -T v143 -A Win32 -DOCPN_TARGET=MSVC ..
cmake --build . --target package --config relwithdebinfo >output.txt
	
# Bash script completes tarball prep adding metadata into it.

bash ./cloudsmith-upload.sh

# Find ${bold}"build/output.txt"${normal} file if the build is not successful.
# Other examples below.
