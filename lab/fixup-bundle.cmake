cmake_minimum_required(VERSION 3.15)
include(BundleUtilities)

if(NOT DEFINED LAB_APP OR NOT IS_DIRECTORY "${LAB_APP}")
  message(FATAL_ERROR "LAB_APP must name the built .app bundle")
endif()

# Bring non-system wxWidgets and support dylibs into the app.  The DMG must not
# depend on Bob having Homebrew or an OpenCPN development environment.
fixup_bundle("${LAB_APP}" "" "/usr/local/lib;/opt/homebrew/lib;/usr/local/opt/wxwidgets/lib;/opt/homebrew/opt/wxwidgets/lib")
verify_app("${LAB_APP}")
