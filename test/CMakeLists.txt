# test/CMakeLists.txt
cmake_minimum_required(VERSION 3.14)
project(celestial_navigation_pi_tests)
set(CMAKE_CXX_STANDARD 17)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

message(STATUS "Building Celestial Navigation plugin tests")
enable_testing()

add_library(win32_libs INTERFACE)
if (MSVC)
    target_link_libraries(win32_libs
        INTERFACE
            setupapi.lib psapi.lib
            ${CMAKE_SOURCE_DIR}/cache/buildwin/iphlpapi.lib
    )
endif()

# Find required packages
find_package(GTest REQUIRED)
find_package(wxWidgets COMPONENTS core base net html REQUIRED)

add_subdirectory(${CMAKE_SOURCE_DIR}/opencpn-libs/tinyxml ${CMAKE_CURRENT_BINARY_DIR}/tinyxml)

set(SRC
    altitude_tests.cpp
    mock_plugin_api.cpp
    mock_plugin_impl.cpp
    ${CMAKE_SOURCE_DIR}/src/Sight.cpp
    ${CMAKE_SOURCE_DIR}/src/celestial_navigation_pi.cpp
    ${CMAKE_SOURCE_DIR}/src/astrolabe/calendar.cpp
    ${CMAKE_SOURCE_DIR}/src/astrolabe/dicts.cpp
    ${CMAKE_SOURCE_DIR}/src/astrolabe/dynamical.cpp
    ${CMAKE_SOURCE_DIR}/src/astrolabe/elp2000.cpp
    ${CMAKE_SOURCE_DIR}/src/astrolabe/globals.cpp
    ${CMAKE_SOURCE_DIR}/src/astrolabe/nutation.cpp
    ${CMAKE_SOURCE_DIR}/src/astrolabe/sun.cpp
    ${CMAKE_SOURCE_DIR}/src/astrolabe/util.cpp
    ${CMAKE_SOURCE_DIR}/src/astrolabe/vsop87d.cpp
    ${CMAKE_SOURCE_DIR}/src/transform_star.cpp
    ${CMAKE_SOURCE_DIR}/src/CelestialNavigationDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ClockCorrectionDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/CelestialNavigationUI.cpp
    ${CMAKE_SOURCE_DIR}/src/FindBodyDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/celestial_navigation_pi.cpp
    ${CMAKE_SOURCE_DIR}/src/epv00.cpp
    ${CMAKE_SOURCE_DIR}/src/FixDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/SightDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/geodesic.c
    ${CMAKE_SOURCE_DIR}/src/geomag/geomag.c
    ${CMAKE_SOURCE_DIR}/src/icons.cpp
    ${CMAKE_SOURCE_DIR}/src/moon.cpp
)

add_executable(celestial_tests ${SRC})

target_compile_definitions(celestial_tests
    PUBLIC
        USE_MOCK_DEFS
        CMAKE_BINARY_DIR="${CMAKE_BINARY_DIR}"
        TESTDATA="${CMAKE_CURRENT_LIST_DIR}/testdata"
        UNIT_TESTS
        TIXML_USE_STL
)

# Include directories
target_include_directories(celestial_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/opencpn-libs/${PKG_API_LIB}/include
        ${CMAKE_SOURCE_DIR}/opencpn-libs/tinyxml/include
        ${CMAKE_SOURCE_DIR}/opencpn-libs/wxJSON/include
        ${GTEST_INCLUDE_DIRS}
        ${wxWidgets_INCLUDE_DIRS}
)

# Link against required libraries
target_link_libraries(celestial_tests
    PRIVATE
        GTest::GTest
        GTest::Main
        ${wxWidgets_LIBRARIES}
        ocpn::api
        ocpn::tinyxml
        ocpn::wxjson
        ${OPENGL_LIBRARIES}
)

# Set optimization level for debug builds
if (NOT MSVC)
    target_compile_options(celestial_tests PUBLIC "-O0")
endif()

# Add the test
add_test(NAME celestial_tests COMMAND celestial_tests)

# Add a custom target to run tests with more details
add_custom_target(run-tests
    COMMAND ${CMAKE_CTEST_COMMAND} -V
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS celestial_tests
)
