cmake_minimum_required(VERSION 3.15)

project(celestial_eclipse_engine VERSION 0.1.0 LANGUAGES C CXX)

option(ECLIPSE_BUILD_CLI "Build the standalone eclipse command-line tool" ON)
option(ECLIPSE_BUILD_TESTS "Build the standalone eclipse tests" ON)

file(GLOB ERFA_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/erfa/src/*.c"
)
list(FILTER ERFA_SOURCES EXCLUDE REGEX "/t_erfa_c(_extra)?\\.c$")
list(FILTER ERFA_SOURCES EXCLUDE REGEX "/erfaversion\\.c$")
add_library(eclipse_erfa STATIC ${ERFA_SOURCES})
target_include_directories(eclipse_erfa PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/erfa/src"
)
set_target_properties(eclipse_erfa PROPERTIES
  C_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON
)

add_library(eclipse_core STATIC
  src/astronomy.cpp
  src/besselian.cpp
  src/geometry.cpp
  src/spk.cpp
)
add_library(celestial::eclipse_core ALIAS eclipse_core)

target_include_directories(eclipse_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(eclipse_core PUBLIC cxx_std_11)
target_link_libraries(eclipse_core PUBLIC eclipse_erfa)
set_target_properties(eclipse_core PROPERTIES
  CXX_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON
)

if(MSVC)
  target_compile_options(eclipse_core PRIVATE /W4)
else()
  target_compile_options(eclipse_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ECLIPSE_BUILD_CLI)
  add_executable(eclipse_cli apps/eclipse_cli.cpp)
  target_link_libraries(eclipse_cli PRIVATE celestial::eclipse_core)
  target_compile_features(eclipse_cli PRIVATE cxx_std_11)
  set_target_properties(eclipse_cli PROPERTIES OUTPUT_NAME eclipse-cli)
endif()

if(ECLIPSE_BUILD_TESTS)
  enable_testing()
  add_executable(eclipse_core_tests tests/besselian_tests.cpp)
  target_sources(eclipse_core_tests PRIVATE tests/geometry_tests.cpp)
  target_link_libraries(eclipse_core_tests PRIVATE celestial::eclipse_core)
  target_compile_features(eclipse_core_tests PRIVATE cxx_std_11)
  add_test(NAME eclipse_core_tests COMMAND eclipse_core_tests)
endif()
