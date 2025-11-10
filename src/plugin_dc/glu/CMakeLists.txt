cmake_minimum_required(VERSION 3.5.0)

if (TARGET ocpn::glu_static)
  return()
endif ()

set(SAVE_CMLOC_GLU ${CMLOC})
set(CMLOC "CMakeLists_GLU: ")

project(glu)

set(CMAKE_VERBOSE_MAKEFILE ON)

## set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
## set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
## set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(GLU_SOURCES
    libutil/error.c
    libutil/glue.c
    # libutil/quad.c libutil/mipmap.c
    libutil/project.c
    libutil/registry.c
    libtess/tess.c
    libtess/mesh.c
    libtess/priorityq.c
    libtess/sweep.c
    libtess/tessmono.c
    libtess/normal.c
    libtess/memalloc.c
    libtess/geom.c
    libtess/dict.c
    libtess/render.c
)

add_library(GLU_static STATIC ${GLU_SOURCES})
add_library(ocpn::glu_static ALIAS GLU_static)
set_target_properties(GLU_static PROPERTIES OUTPUT_NAME GLU)

if (NOT WIN32 AND NOT APPLE)
  target_compile_options(GLU_static PRIVATE
    -Wall -Wno-unused -fexceptions -g -fno-strict-aliasing -O2
  )
endif ()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if (QT_ANDROID)
  add_definitions(" -fPIC")
endif (QT_ANDROID)

target_include_directories(
  GLU_static PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (CMAKE_HOST_WIN32)
  if (MSVC)
    set(CMAKE_C_FLAGS_DEBUG "/MDd /Ob0 /Od  /D_DEBUG  /Zi /RTC1")
    set(CMAKE_C_FLAGS_MINSIZEREL "/MD  /O1  /Ob1 /D NDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "/MD  /O2  /Ob2 /D NDEBUG /Zi")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "/MD  /O2  /Ob1 /D NDEBUG /Zi")
    set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Ob0 /Od  /D_DEBUG  /Zi /RTC1 /EHa")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "/MD  /O1  /Ob1 /D NDEBUG /EHa")
    set(CMAKE_CXX_FLAGS_RELEASE "/MD  /O2  /Ob2 /D NDEBUG /Zi /EHa")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MD  /O2  /Ob1 /D NDEBUG /Zi /EHa")
    set(CMAKE_EXE_LINKER_FLAGS "/DEBUG ${CMAKE_EXE_LINKER_FLAGS}")
  endif (MSVC)
endif (CMAKE_HOST_WIN32)

target_include_directories(
  GLU_static PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include
)

message(
  STATUS
    "${CMLOC}CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}, CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}"
)
get_property(
  dirs
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  PROPERTY INCLUDE_DIRECTORIES
)
foreach (dir ${dirs})
  message(STATUS "${CMLOC}include dir='${dir}'")
endforeach ()

set(CMLOC ${SAVE_CMLOC_GLU})
