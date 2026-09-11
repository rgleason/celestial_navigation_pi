# test/CMakeLists.txt
cmake_minimum_required(VERSION 3.14)
project(celestial_navigation_pi_tests)
if(UNIX)
    add_test(NAME ci_apt_settings COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/ci_apt_settings_test.sh)
endif()
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
find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_subdirectory(${CMAKE_SOURCE_DIR}/opencpn-libs/tinyxml ${CMAKE_CURRENT_BINARY_DIR}/tinyxml)

set(SRC
    atomic_xml_file_tests.cpp
    dialog_geometry_tests.cpp
    dialog_transaction_state_tests.cpp
    eclipse_data_files_tests.cpp
    eclipse_verification_worker_tests.cpp
    altitude_tests.cpp
    azimuth_tests.cpp
    lunar_tests.cpp
    lunar_fiji_tests.cpp
    lunar_solution_record_tests.cpp
    lunar_ui_smoke_tests.cpp
    find_body_ui_tests.cpp
    lunar_distance_engine_tests.cpp
    lunar_session_engine_tests.cpp
    lunar_session_worker_tests.cpp
    sextant_calibration_engine_tests.cpp
    lunar_de440_tests.cpp
    lunar_observer_tests.cpp
    dut1_update_tests.cpp
    coastal_navigation_tests.cpp
    coastal_ui_smoke_tests.cpp
    horizon_event_tests.cpp
    html_help_tests.cpp
    navigation_algorithms_tests.cpp
    numerical_audit_tests.cpp
    waypoint_position_source_tests.cpp
    almanac_generator_tests.cpp
    time_status_tests.cpp
    utf8_translation_tests.cpp
    common.cpp
    mock_plugin_api.cpp
    mock_plugin_impl.cpp
    ${CMAKE_SOURCE_DIR}/src/Sight.cpp
    ${CMAKE_SOURCE_DIR}/src/AtomicXmlFile.cpp
    ${CMAKE_SOURCE_DIR}/src/Dut1UpdatePanel.cpp
    ${CMAKE_SOURCE_DIR}/src/DialogGeometry.cpp
    ${CMAKE_SOURCE_DIR}/src/EclipseDataFiles.cpp
    ${CMAKE_SOURCE_DIR}/src/EclipseVerificationWorker.cpp
    ${CMAKE_SOURCE_DIR}/src/LunarDistanceEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/LunarSessionEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/LunarSessionWorker.cpp
    ${CMAKE_SOURCE_DIR}/src/SextantCalibrationEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/LunarToolsDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/CoastalNavigationEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/CoastalNavigationDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/BodyCatalog.cpp
    ${CMAKE_SOURCE_DIR}/src/NavigationAlgorithms.cpp
    ${CMAKE_SOURCE_DIR}/src/AlmanacGenerator.cpp
    ${CMAKE_SOURCE_DIR}/src/AlmanacPaperTables.cpp
    ${CMAKE_SOURCE_DIR}/src/AlmanacDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/PlannerDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/WaypointPositionSource.cpp
    ${CMAKE_SOURCE_DIR}/src/WaypointPickerDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/SightAnalysisDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/TimeStatus.cpp
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
    ${CMAKE_SOURCE_DIR}/src/HorizonEventDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/HtmlHelp.cpp
    ${CMAKE_SOURCE_DIR}/src/EclipseDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ClockCorrectionDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/CelestialNavigationUI.cpp
    ${CMAKE_SOURCE_DIR}/src/FindBodyDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/celestial_navigation_pi.cpp
    ${CMAKE_SOURCE_DIR}/src/epv00.cpp
    ${CMAKE_SOURCE_DIR}/src/FixDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/LunarResultsDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/SightDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/geodesic.c
    ${CMAKE_SOURCE_DIR}/src/geomag/geomag.c
    ${CMAKE_SOURCE_DIR}/src/icons.cpp
    ${CMAKE_SOURCE_DIR}/src/moon.cpp
)

add_executable(celestial_tests ${SRC})
if(GTK3_LIBRARIES)
    target_link_libraries(celestial_tests PRIVATE ${GTK3_LIBRARIES})
endif()

option(CELESTIAL_ENGINE_LAB_TESTS "Compare experimental headless adapter with production" OFF)
if(CELESTIAL_ENGINE_LAB_TESTS)
    target_sources(celestial_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/validation/engine-lab/adapter_tests.cpp)
    add_executable(lunar_production_lab
        ${CMAKE_SOURCE_DIR}/validation/engine-lab/production_runner.cpp
        ${CMAKE_SOURCE_DIR}/src/LunarDistanceEngine.cpp
        ${CMAKE_SOURCE_DIR}/src/LunarSessionEngine.cpp)
    target_include_directories(lunar_production_lab PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(lunar_production_lab PRIVATE celestial::eclipse_core)
endif()

target_compile_definitions(celestial_tests
    PUBLIC
        USE_MOCK_DEFS
        CMAKE_BINARY_DIR="${CMAKE_BINARY_DIR}"
        TESTDATA="${CMAKE_CURRENT_LIST_DIR}/testdata"
        UNIT_TESTS
        ECLIPSE_DE440_TEST_PATH="${CMAKE_SOURCE_DIR}/eclipse/data/de440s.bsp"
        TIXML_USE_STL
)

# Include directories
target_include_directories(celestial_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/opencpn-libs/${PKG_API_LIB}/include
        ${CMAKE_SOURCE_DIR}/opencpn-libs/tinyxml/include
        ${CMAKE_SOURCE_DIR}/opencpn-libs/wxJSON/include
        ${CMAKE_SOURCE_DIR}/src/plugin_dc/dc_utils/include
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
        ocpn::plugin-dc
        celestial::eclipse_core
)

# Set optimization level for debug builds
if (NOT MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
    target_compile_options(celestial_tests PUBLIC "-O0")
endif()

# Add the test
add_test(NAME celestial_tests COMMAND celestial_tests)
if(UNIX)
    # Separate processes avoid wx/libc timezone caches masking UTC-boundary bugs.
    foreach(zone UTC Europe/London America/New_York Asia/Kolkata)
        string(REPLACE "/" "_" test_zone "${zone}")
        add_test(NAME numerical_audit_${test_zone}
            COMMAND ${CMAKE_COMMAND} -E env "TZ=${zone}"
                $<TARGET_FILE:celestial_tests> --gtest_filter=NumericalAudit.*)
    endforeach()
endif()
add_test(
    NAME ui_utf8_literals
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_LIST_DIR}/check_utf8_ui_literals.py
            ${CMAKE_SOURCE_DIR}/src
)

# Add a custom target to run tests with more details
add_custom_target(run-tests
    COMMAND ${CMAKE_CTEST_COMMAND} -V
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS celestial_tests
)
