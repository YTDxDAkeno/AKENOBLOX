# AkenoBlox PS5
# Copyright (C) 2026 AkenoBlox contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# One tree, two targets:
#   host  - cmake -S . -B build && cmake --build build && ctest --test-dir build
#   PS5   - tools/build/build-ps5.sh (cmake/ps5-toolchain.cmake, then the
#           title link and packaging in tools/build/link-ps5-title.sh)
cmake_minimum_required(VERSION 3.20)
project(AkenoBlox VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(AKENO_TARGET_PS5 "Build for the PlayStation 5 (set by cmake/ps5-toolchain.cmake)" OFF)
option(AKENO_BUILD_TESTS "Build the host tests" ON)
option(AKENO_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(AKENO_FUZZ "Build libFuzzer targets (clang only)" OFF)
option(AKENO_WERROR "Treat warnings as errors" ON)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(AKENO_TARGET_PS5)
  set(AKENO_BUILD_TESTS OFF)
endif()

# Version and build identity: the version from this file, the commit from git,
# both compiled in and printed first at start-up so a log names its binary.
execute_process(
  COMMAND git rev-parse --short=9 HEAD
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  OUTPUT_VARIABLE AKENO_GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(
  COMMAND git status --porcelain --untracked-files=no
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  OUTPUT_VARIABLE AKENO_GIT_DIRTY OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT AKENO_GIT_COMMIT)
  set(AKENO_GIT_COMMIT "unknown")
elseif(AKENO_GIT_DIRTY)
  set(AKENO_GIT_COMMIT "${AKENO_GIT_COMMIT}-dirty")
endif()
set(AKENO_VERSION_SUFFIX "alpha")
configure_file(src/app/akeno_version.h.in "${CMAKE_BINARY_DIR}/generated/akeno_version.h" @ONLY)

set(AKENO_WARNINGS -Wall -Wextra -Wshadow -Wstrict-prototypes -Wmissing-prototypes
    -Wformat=2 -Wundef -Wno-unused-parameter)
if(AKENO_WERROR)
  list(APPEND AKENO_WARNINGS -Werror)
endif()

function(akeno_defaults target)
  # -fno-math-errno: __builtin_sqrtf becomes sqrtss rather than a call into
  # libm, one fewer import whose presence on the console must be proved.
  target_compile_options(${target} PRIVATE ${AKENO_WARNINGS} -fno-math-errno)
  target_include_directories(${target} PUBLIC
    "${PROJECT_SOURCE_DIR}/src/platform"
    "${PROJECT_SOURCE_DIR}/src/compat/elf"
    "${PROJECT_SOURCE_DIR}/src"
    "${CMAKE_BINARY_DIR}/generated")
  if(AKENO_TARGET_PS5)
    # Console backtraces are frame-pointer walks (PS5_VulkanTemplate toolchain.md).
    target_compile_options(${target} PRIVATE -fno-omit-frame-pointer -ffunction-sections
                           -fdata-sections)
    target_compile_definitions(${target} PRIVATE AKENO_PS5=1)
  endif()
  if(AKENO_FUZZ)
    # Coverage for libFuzzer in every library, not only the fuzz entry points:
    # the code under test is the loader, not the harness.
    target_compile_options(${target} PRIVATE -fsanitize=fuzzer-no-link,address,undefined)
    target_link_options(${target} PUBLIC -fsanitize=address,undefined)
  endif()
  if(AKENO_SANITIZE)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer
                           -fno-sanitize-recover=undefined)
    target_link_options(${target} PUBLIC -fsanitize=address,undefined)
  endif()
endfunction()

# ------------------------------------------------------------- libraries

add_library(akeno_elf STATIC
  src/compat/elf/elf_parse.c
  src/compat/elf/elf_load.c
  src/compat/elf/elf_symtab.c)
akeno_defaults(akeno_elf)

if(AKENO_TARGET_PS5)
  add_library(akeno_platform STATIC
    src/platform/ps5/platform_ps5.c
    src/platform/ps5/ps5_memory.c
    src/platform/ps5/ps5_display.c
    src/platform/ps5/ps5_pad.c)
else()
  add_library(akeno_platform STATIC
    src/platform/host/host_memory.c
    src/platform/host/platform_host.c)
  target_include_directories(akeno_platform PUBLIC "${PROJECT_SOURCE_DIR}/src/platform/host")
endif()
akeno_defaults(akeno_platform)

add_library(akeno_core STATIC
  src/app/akeno_log.c
  src/app/akeno_settings.c
  src/app/akeno_menu.c
  src/input/akeno_input.c
  src/ui/akeno_canvas.c
  src/ui/akeno_font.c
  src/ui/akeno_screens.c
  src/graphics/akeno_tiling.c
  src/diagnostics/akeno_cpu.c
  src/diagnostics/akeno_selftest.c
  src/diagnostics/akeno_report.c
  src/diagnostics/akeno_udp.c
  src/runtime/akeno_runtime.c)
akeno_defaults(akeno_core)
target_link_libraries(akeno_core PUBLIC akeno_elf akeno_platform)
if(NOT AKENO_TARGET_PS5)
  find_package(Threads REQUIRED)
  target_link_libraries(akeno_core PUBLIC Threads::Threads m)
endif()

# The self-test's fixtures, embedded: the same bytes the host tests load.
include(cmake/AkenoFixtures.cmake)
akeno_embed_fixture(akeno_core basic_aps2_16k.so akeno_selftest_aps2)
akeno_embed_fixture(akeno_core basic_relr_4k.so akeno_selftest_relr)
akeno_embed_fixture(akeno_core canary.so akeno_selftest_canary)
add_dependencies(akeno_core akeno_fixtures)

# ------------------------------------------------------------ the program

if(AKENO_TARGET_PS5)
  # The title: objects and archives, linked by tools/build/link-ps5-title.sh
  # with PS5_Vulkan's CRT and recipe, converted and signed into eboot.bin, and
  # assembled with sce_sys/ and libc.prx into dist/<TITLE_ID>/.
  add_library(akenoblox_title OBJECT src/app/main.c src/app/akeno_app.c)
  akeno_defaults(akenoblox_title)
  target_link_libraries(akenoblox_title PUBLIC akeno_core)
  set(AKENO_PS5_VULKAN_DIR "" CACHE PATH "A PS5_Vulkan checkout with its native tool and libc.prx built")
  if(NOT AKENO_PS5_VULKAN_DIR)
    message(FATAL_ERROR "set AKENO_PS5_VULKAN_DIR (tools/build/build-ps5.sh does)")
  endif()
  set(AKENO_DIST_DIR "${PROJECT_SOURCE_DIR}/dist" CACHE PATH "Where the title folder is assembled")
  add_custom_target(title ALL
    COMMAND bash "${PROJECT_SOURCE_DIR}/tools/build/link-ps5-title.sh"
            "${CMAKE_BINARY_DIR}/title" "${AKENO_PS5_VULKAN_DIR}" "${AKENO_PS5_SDK}" "${AKENO_DIST_DIR}"
            "$<TARGET_OBJECTS:akenoblox_title>"
            "$<TARGET_FILE:akeno_core>" "$<TARGET_FILE:akeno_elf>" "$<TARGET_FILE:akeno_platform>"
    DEPENDS akenoblox_title akeno_core akeno_elf akeno_platform
            "${PROJECT_SOURCE_DIR}/tools/build/link-ps5-title.sh" "${PROJECT_SOURCE_DIR}/sce_sys/param.json"
    COMMAND_EXPAND_LISTS
    VERBATIM
    COMMENT "Linking and signing eboot.bin")
else()
  add_executable(akenoblox-host src/app/main.c src/app/akeno_app.c)
  akeno_defaults(akenoblox-host)
  target_link_libraries(akenoblox-host PRIVATE akeno_core)
endif()

# ----------------------------------------------------------------- tests

if(AKENO_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
