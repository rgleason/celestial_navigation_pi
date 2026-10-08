cmake_minimum_required(VERSION 3.15)
project(celestial_engine_lab LANGUAGES C CXX)

# Standalone by design: never configure the plugin, wxWidgets or OpenCPN.
if(NOT IS_DIRECTORY "${ENGINE_SOURCE}/eclipse")
  message(FATAL_ERROR "Set ENGINE_SOURCE to an exported engine snapshot")
endif()
set(ECLIPSE_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(ECLIPSE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ECLIPSE_BUILD_LOLA_TOOL OFF CACHE BOOL "" FORCE)
add_subdirectory("${ENGINE_SOURCE}/eclipse" eclipse)
add_executable(lunar-lab runner.cpp
  "${ENGINE_SOURCE}/src/LunarDistanceEngine.cpp"
  "${ENGINE_SOURCE}/src/LunarSessionEngine.cpp")
target_include_directories(lunar-lab PRIVATE "${ENGINE_SOURCE}/src")
target_link_libraries(lunar-lab PRIVATE celestial::eclipse_core)
target_compile_features(lunar-lab PRIVATE cxx_std_17)
if(NOT MSVC)
  target_compile_options(lunar-lab PRIVATE -Wall -Wextra -Wpedantic)
endif()
