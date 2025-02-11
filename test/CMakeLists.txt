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
find_package(wxWidgets COMPONENTS core base net REQUIRED)

set(SRC
    altitude_tests.cpp
    mock_plugin_api.cpp
    ${CMAKE_SOURCE_DIR}/src/Sight.cpp
    ${CMAKE_SOURCE_DIR}/src/celestial_navigation_pi.cpp
)

add_executable(celestial_tests ${SRC})

target_compile_definitions(celestial_tests
    PUBLIC
        USE_MOCK_DEFS
        CMAKE_BINARY_DIR="${CMAKE_BINARY_DIR}"
        TESTDATA="${CMAKE_CURRENT_LIST_DIR}/testdata"
        UNIT_TESTS
)

# Include directories
target_include_directories(celestial_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/opencpn-libs/${PKG_API_LIB}/include
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