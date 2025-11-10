# ~~~
# Summary:      Plugin OpenGL support files
# License:      GPLv3+
# Copyright (c) 2023 Alec Leamas
#
# Link OpenGL headers and libraries if available, else fall back to GLES2.
# Link glu library from system if available, else use a bundled copy.
# Add some ocpn opengl utilities.
#
# Exports:
#   ocpn::plugin-dc transitive linkage object
# ~~~

cmake_minimum_required(VERSION 3.5)
project("plugin-dc")
option(PLUGINDC_USE_GL "Build using OpenGL" ON)

if (TARGET ocpn::plugin-dc)
  return ()
endif ()

add_library(_PLUGINDC INTERFACE)
add_library(ocpn::plugin-dc ALIAS _PLUGINDC)

find_package(OpenGL)
find_path(GL2_H NAME gl2.h PATH_SUFFIXES GLES2)
if (TARGET OpenGL::GL)
  target_link_libraries(_PLUGINDC INTERFACE OpenGL::GL)
  message(STATUS "Using standard OpenGL (GL/gl.h)")
elseif (TARGET OpenGL::GLES2)
  # Requires cmake >= 3.27
  target_link_libraries(_PLUGINDC INTERFACE OpenGL::GLES2)
  message(STATUS "Using GLES2 OpenGL (GLES2/gl2.h)")
elseif (GL2_H)
  target_include_directories(_PLUGINDC INTERFACE GL2_H)
  # Assuming library is on standard paths.
  message(STATUS "Using GLES2 OpenGL (GLES2/gl2.h)")
else ()
  message(WARNING "Cannot locate a OpenGL stack")
  return ()
endif ()

add_library(_PLUGINGLU INTERFACE)
add_library(ocpn::pluginglu ALIAS _PLUGINGLU)

if (TARGET OpenGL::GLU)
  message(STATUS "Using system glu library")
  target_link_libraries(_PLUGINGLU INTERFACE OpenGL::GLU)
  file(GLOB FLATPAK_INCLUDE /app/extensions/*)
  if (NOT "${FLATPAK_INCLUDE}"  STREQUAL "")
    message(STATUS "Adding Flatpak include path hack")
    target_include_directories(_PLUGINGLU INTERFACE ${FLATPAK_INCLUDE}/include)
  endif ()
  if (MSVC)
    target_link_libraries(_PLUGINGLU INTERFACE windows::headers)
  endif ()
else ()
  message(STATUS "Using bundled glu library")
  add_subdirectory(glu)
  target_link_libraries(_PLUGINGLU INTERFACE ocpn::glu_static)
endif ()

target_link_libraries(_PLUGINDC INTERFACE _PLUGINGLU)

add_subdirectory(dc_utils)
target_link_libraries(_PLUGINDC INTERFACE ocpn::dc-utils)
