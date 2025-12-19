plugingl README
===============

This directory contains classes and methods useful for drawing on the charts
surface. It is usable on all platforms.  All required GL dependencies are
handled internally.

The drawing methods could be built with or without OpenGL support. By default
this is enabled. To disable it, use

    set(PLUGINDC_USE_GL OFF CACHE BOOL "")
    add_subdirectory(opencpn-libs/plugin_dc)
    target_link_libraries(SOME_PLUGIN_TARGET PRIVATE ocpn::plugin-dc)

To build using default options, omit the `set(PLUGINDC_USE_GL...` line.

The formerly separate libraries glu and libglu are part of
plugingl and will be retired.
