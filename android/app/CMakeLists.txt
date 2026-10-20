cmake_minimum_required(VERSION 3.22)
project(acgc_android C CXX)

# Native ARM64 libraries must load on Android devices with 16 KB pages.
if(ANDROID_ABI STREQUAL "arm64-v8a")
    add_link_options("-Wl,-z,max-page-size=16384")
endif()

# Bound peak memory in CI; a compiler cache is optional for local builds.
set(ACGC_COMPILE_JOBS 3 CACHE STRING "Maximum concurrent native compile tasks")
if(CMAKE_GENERATOR MATCHES "Ninja")
    set_property(GLOBAL PROPERTY JOB_POOLS acgc_compile=${ACGC_COMPILE_JOBS})
    set(CMAKE_JOB_POOL_COMPILE acgc_compile)
endif()
find_program(ACGC_CCACHE ccache)
if(ACGC_CCACHE)
    set(CMAKE_C_COMPILER_LAUNCHER "${ACGC_CCACHE}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${ACGC_CCACHE}")
endif()

set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)

set(SDL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../SDL")
set(ACGC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")

if(NOT EXISTS "${SDL_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "SDL2 is missing. Run: git submodule update --init --recursive")
endif()

add_subdirectory("${SDL_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/sdl")
add_subdirectory("${ACGC_ROOT}/pc" "${CMAKE_CURRENT_BINARY_DIR}/acgc")
