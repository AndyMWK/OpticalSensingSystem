# ---------------------------------------------------------------------------
# Host-side unit tests for the IR sensor firmware.
#
# This is a SEPARATE CMake project from the firmware in ../CMakeLists.txt.
# That is deliberate: the firmware build is pinned to the arm-none-eabi
# cross-compiler through cmake/gcc-arm-none-eabi.cmake, and a cross-compiled
# binary cannot run on your PC. These tests are compiled with your native
# compiler instead, so they build and run in about a second with no hardware
# attached.
#
# Configure / build / run:
#     cmake -S tests -B build/host-tests
#     cmake --build build/host-tests
#     ctest --test-dir build/host-tests --output-on-failure
# ---------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.22)

project(ir_sensor_host_tests LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Turns on CTest support. Without this, add_test() further down is ignored.
enable_testing()

# Where the firmware lives, relative to this file.
set(FW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

# ---------------------------------------------------------------------------
# 1. The test framework: Unity
#
# Unity (github.com/ThrowTheSwitch/Unity) is a small C-only assertion library
# built for embedded work - three source files, no C++, no dynamic allocation.
# FetchContent downloads it at configure time and adds its CMakeLists.txt to
# this build, which gives us a linkable target called `unity`. The source is
# cached under build/host-tests/_deps, so it is downloaded once, not per build.
#
# Pinning GIT_TAG to a release (not a branch) keeps the build reproducible.
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(unity)

# ---------------------------------------------------------------------------
# 2. The code under test
#
# Only hardware-independent modules go in here. fifo.c, ir_processing.c and
# fsm.c call nothing but the C standard library, which is why they can run on
# the host at all. ir_adc.c, logger.c and rs485.c call HAL functions directly,
# so they would need those functions faked out before they could be added.
# ---------------------------------------------------------------------------
add_library(fw_under_test STATIC
    ${FW_ROOT}/Core/Src/modules/fifo.c
    ${FW_ROOT}/Core/Src/modules/ir_processing.c
    ${FW_ROOT}/Core/Src/modules/fsm.c
)

# PUBLIC: anything that links fw_under_test also gets these include paths,
# so the test files can `#include "fsm.h"` without repeating this.
target_include_directories(fw_under_test PUBLIC
    ${FW_ROOT}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes
)

# Force-include the HAL stub into every file compiled here, and into every
# file in a target that links against this one. See fakes/hal_stub.h for the
# full explanation of why this is needed.
set(HAL_STUB "${CMAKE_CURRENT_SOURCE_DIR}/fakes/hal_stub.h")
if(MSVC)
    target_compile_options(fw_under_test PUBLIC "/FI${HAL_STUB}")
else()
    target_compile_options(fw_under_test PUBLIC -include "${HAL_STUB}")
endif()

# ir_processing.c uses sqrtf(); on most Unix-like toolchains that lives in
# libm rather than libc.
if(NOT MSVC)
    target_link_libraries(fw_under_test PUBLIC m)
endif()

# Warnings are worth having on the firmware sources too - the host compiler
# often spots things the ARM build's flags let through. Not -Werror, so an
# existing warning never blocks you from running tests.
if(MSVC)
    target_compile_options(fw_under_test PRIVATE /W3)
else()
    target_compile_options(fw_under_test PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# 3. The tests themselves
# ---------------------------------------------------------------------------
add_subdirectory(src)
