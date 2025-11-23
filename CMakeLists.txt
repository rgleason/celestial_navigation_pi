##---------------------------------------------------------------------------
## Author:      Sean d'Epagnier
##***************************************************************************
## *   Copyright (C) 2010 by Sean d'Epagnier                              *
## *   License:     GPLv3+
## *   This program is free software; you can redistribute it and/or modify  *
## *   it under the terms of the GNU General Public License as published by  *
## *   the Free Software Foundation; either version 2 of the License, or     *
## *   (at your option) any later version.                                   *
## *                                                                         *
## *   This program is distributed in the hope that it will be useful,       *
## *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
## *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
## *   GNU General Public License for more details.                          *
## *                                                                         *
## *   You should have received a copy of the GNU General Public License     *
## *   along with this program; if not, write to the                         *
## *   xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx,                                       *
## *   xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx                                       *
#-----------------------------------------------------------------------------
# Frontend2 Author: Jon Gough Testplugin(TP) v1.0.345.0 15/10/2025 DD/MM/YYYY
# buildCI:yes useODAPI:no validateJSON:no
# ---------------------------------------------------------------------------
## -- When changing this file do NOT change the order of the sections.    -- ##
## -- Make changes only between the section blocks identified             -- ##
## -- The whole configuration process relies on this sequence for success.-- ##
##
## -- Modify section below to include all the details for your plugin.    -- ##

set(CMLOC "CMakeLists: ")

# define minimum cmake version
cmake_minimum_required(VERSION 3.15)
if(COMMAND cmake_policy)
  if (POLICY CMP0043)
    cmake_policy(SET CMP0043 NEW)
  endif (POLICY CMP0043)
  cmake_policy(SET CMP0048 NEW)
  if (POLICY CMP0068)
    cmake_policy(SET CMP0068 NEW)
  endif (POLICY CMP0068)
  if (POLICY CMP0076)
    cmake_policy(SET CMP0076 NEW)
  endif (POLICY CMP0076)
  if (POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
  endif (POLICY CMP0077)
  if (POLICY CMP0079)
    cmake_policy(SET CMP0079 NEW)
  endif (POLICY CMP0079)
  if (POLICY CMP0175)
    cmake_policy(SET CMP0175 NEW)
  endif (POLICY CMP0175)
  if (POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
  endif (POLICY CMP0177)
endif (COMMAND cmake_policy)

# set for vscode/vscodium
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

# define plugin name, owner and versions
set(VERBOSE_NAME Celestial_Navigation)
set(COMMON_NAME "Celestial Navigation")
set(TITLE_NAME "CELESTIALNAVIGATION")
set(PACKAGE_CONTACT  "Sean D'Epagnier ")
set(PACKAGE "celestial_navigation")
set(SHORT_DESCRIPTION
	"Celestial Navigation Plugin for OpenCPN"
)
set(LONG_DESCRIPTION
	"Nautical almanac: sun, moon, planets and navigational stars. Compute position fix from celestial measurements."
)

set(VERSION_MAJOR "2")
set(VERSION_MINOR "4")
set(VERSION_PATCH "65")
set(VERSION_TWEAK "18")
set(VERSION_DATE  "23/11/2025" )# DD/MM/YYYY format
set(OCPN_MIN_VERSION "improve")

set(OCPN_API_VERSION_MAJOR "1")
set(OCPN_API_VERSION_MINOR "18")
set(TP_COMMENT "  * Release using CI")

set(PARENT "opencpn")

# Use to set up a local webserver with git installed. Default is github.com.
# Now set in pluginconfigure.cmake set(GIT_REPOSITORY_SERVER "github.com")

## Specifies Cloudsmith upload repository suffix for each catalog
set(PROD "prod")   #Standard Repos
set(BETA "beta")   #Standard Repos
set(ALPHA "alpha") #Standard Repos

# Set if  Cloudsmith Base Repository name does not match your Git Repository name.
#set (CLOUDSMITH_BASE_REPOSITORY "${GIT_REPOSITORY_NAME}")
#set (CLOUDSMITH_BASE_REPOSITORY "opencpn-plugins")  #Ricks's customization
set (CLOUDSMITH_BASE_REPOSITORY "celestial-navigation")  # without the pi 

# Set the following to use OpenCPN cloudsmith repositories instead of own repositories
set(CLOUDSMITH_USER "opencpn")
# set (CLOUDSMITH_USER "rick-gleason")

set(XML_INFO_URL
	"https://opencpn.org/OpenCPN/plugins/celestialnav.html"
)
set(XML_SUMMARY ${SHORT_DESCRIPTION})
set(XML_DESCRIPTION ${LONG_DESCRIPTION})

## ----- Modify section above to include all the details for your plugin ------- ##
## ----- Modify section below if there are special requirements for the plugin --##

option(PLUGIN_USE_SVG "Use SVG graphics" ON)

set(CMAKE_CXX_STANDARD 11)

# Use local version of GLU library requires libs/glu directory
set(USE_LOCAL_GLU FALSE)
# Prefer libGL.so to libOpenGL.so, see CMP0072
set(OpenGL_GL_PREFERENCE "LEGACY") # not operational

option(USE_GL "Enable OpenGL support" ON)
message(STATUS "${CMLOC}USE_GL: ${USE_GL}")

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
  set(CMAKE_BUILD_TYPE
      "Release"
      CACHE STRING "Choose the type of build" FORCE
  )
endif ("${CMAKE_BUILD_TYPE}" STREQUAL "")
set_property(
  CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "" "Debug" "Release" "RelWithDebInfo"
                                  "MinSizeRel"
)
message(STATUS "${CMLOC}Build type: ${CMAKE_BUILD_TYPE}")

# ----- Statements below used to setup standard variables required by the CMAKE  -##
# ----- process -- Do not remove -##

project(${PACKAGE})

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

##========================================================

include("PluginSetup")

# Option to enable/disable testing
option(OCPN_BUILD_TEST "Build plugin tests" OFF)

##========================================================
## ----- Above sets up standard variables required by CMAKE - Do not remove -----##
## ----- Modify section below if there are special requirements for the plugin --##

set(CMAKE_VERBOSE_MAKEFILE "Activate verbose mode for make files" ON)

option(Plugin_CXX11 "Use c++11" ON)

## ----- Modify section above if there are special requirements for the plugin --##
## ----- Do not change next section - needed to configure build process ---------##
##===========================================================

include("PluginConfigure")

##===========================================================
## ----- Do not change section above - needed to configure build process --------##
## ----- Change below to match project requirements for source, headers, etc. ---##

add_definitions(-DUSE_S57)

## ---- Section Below - Add your project SET(SRCS and SET(HDRS  etc..

SET(SRCS
        src/astrolabe/astrolabe.hpp
        src/astrolabe/calendar.cpp
        src/astrolabe/dicts.cpp
        src/astrolabe/dynamical.cpp
        src/astrolabe/elp2000.cpp
        src/astrolabe/equinox.cpp
        src/astrolabe/globals.cpp
        src/astrolabe/nutation.cpp
        src/astrolabe/riseset.cpp
        src/astrolabe/sun.cpp
        src/astrolabe/util.cpp
        src/astrolabe/vsop87d.cpp
        src/geomag/geomag.c
        src/CelestialNavigationDialog.cpp
        src/CelestialNavigationUI.cpp
        src/SightDialog.cpp
        src/Sight.cpp
        src/icons.cpp
        src/FindBodyDialog.cpp
        src/LunarResultsDialog.cpp
        src/FixDialog.cpp
        src/ClockCorrectionDialog.cpp
        src/geodesic.c
        src/transform_star.cpp
        src/epv00.cpp
        src/celestial_navigation_pi.cpp
        src/moon.cpp
        )

SET(HDRS
        src/celestial_navigation_pi.h
        src/CelestialNavigationDialog.h
        src/CelestialNavigationUI.h
        src/ClockCorrectionDialog.h
        src/FindBodyDialog.h
        src/FixDialog.h
        src/geodesic.h
        src/icons.h
        src/Sight.h
        src/SightDialog.h
        src/moon.h
        )

add_definitions(-DPLUGIN_USE_SVG)

## ----- Section above - Add your project SET(SRCS and SET(HDRS  etc.-------##
## ----- If using JSON validation in plugin section below is needed --------##

# Do not need Json Schema Validation

## ----- If using JSON validation in plugin section above is needed --------##

if (QT_ANDROID)
endif (QT_ANDROID)

##=============================================================
## Change as required to collect all the set( headers and SRCS
## This add_library  can be used in conditionals below.
##=============================================================

set(SRC ${SRCS} ${HDRS} )

##=============================================================
## Modify/configure 'Include' Directories below for the Plugin
##=============================================================

# Private include_directories
include_directories(BEFORE ${PROJECT_SOURCE_DIR}/src/astrolabe)
include_directories(BEFORE ${PROJECT_SOURCE_DIR}/src/geomag)


# Change target_sources to match your project needs.
# ==============================================================

target_sources(${PACKAGE_NAME} PUBLIC ${SRC} )


if (NOT OCPN_FLATPAK_CONFIG)
  # Build environment not available when flatpak is being configured so
  # following statements will not work
  message(STATUS "${CMLOC}Adding target link libraries to ${PACKAGE_NAME}")

  if (WIN32)
    add_subdirectory(opencpn-libs/WindowsHeaders)
    target_link_libraries(${PACKAGE_NAME} windows::headers)
  endif (WIN32)

  add_subdirectory(opencpn-libs/api-${OCPN_API_VERSION_MINOR})
  target_link_libraries(${PACKAGE_NAME} ocpn::api)

  add_subdirectory(opencpn-libs/tinyxml)
  target_link_libraries(${PACKAGE_NAME} ocpn::tinyxml)

  add_subdirectory(opencpn-libs/jsoncpp)
  target_link_libraries(${PACKAGE_NAME} ocpn::jsoncpp)

  add_subdirectory(opencpn-libs/wxJSON)
  target_link_libraries(${PACKAGE_NAME} ocpn::wxjson)

#  Note this is a local copy of plugin_dc from opencpn-libs,
#  Note remove this in the future when fixes are upstreamed
  add_subdirectory(src/plugin_dc)
#  add_subdirectory(opencpn-libs/plugin_dc)

  target_link_libraries(${PACKAGE_NAME} ocpn::plugin-dc)

  add_subdirectory(opencpn-libs/bzip2)
  target_link_libraries(${PACKAGE_NAME} ocpn::bzip2)

endif (NOT OCPN_FLATPAK_CONFIG)

add_definitions(-DTIXML_USE_STL)

##
## ----- If using JSON validation section below is needed ---------------##
##

# Do not need Json Schema Validation


##------- Change below to match project requirements --------------------##

# Needed for android builds
if (QT_ANDROID)
  include_directories(BEFORE ${qt_android_include})
endif (QT_ANDROID)

## Needed for all builds Do not change - needed to build app -------------##
##===============================================================

include("PluginInstall")
include("PluginLocalization")
include("PluginPackage")

##================================================================
## ----- Do not change section above - needed to configure build process --##

message(STATUS "${CMLOC}include directories: ")
get_property(
  dirs
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  PROPERTY INCLUDE_DIRECTORIES
)
foreach (dir ${dirs})
  message(STATUS "${CMLOC}dir='${dir}'")
endforeach ()

# Add the test directory if testing is enabled
if(OCPN_BUILD_TEST)
  message(STATUS "Building with tests enabled")
  enable_testing()
  add_subdirectory(test)
endif(OCPN_BUILD_TEST)
