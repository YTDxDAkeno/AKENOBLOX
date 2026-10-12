# AkenoBlox PS5 - CMake toolchain for the console (x86_64-sie-ps5).
# Copyright (C) 2026 AkenoBlox contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The shape PS5_RetroArch's and PS5_VulkanTemplate's toolchains use
# (PS5_VulkanTemplate skills/ps5-homebrew/references/toolchain.md), with the
# payload SDK fork tools/build/setup-ps5-deps.sh installs. Set PS5_PAYLOAD_SDK
# to that SDK; tools/build/build-ps5.sh does.

if(NOT DEFINED ENV{PS5_PAYLOAD_SDK})
  message(FATAL_ERROR "set PS5_PAYLOAD_SDK to the payload SDK (tools/build/setup-ps5-deps.sh installs it)")
endif()
set(AKENO_PS5_SDK "$ENV{PS5_PAYLOAD_SDK}" CACHE PATH "The PS5 payload SDK")

set(CMAKE_SYSTEM_NAME FreeBSD)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER "${AKENO_PS5_SDK}/bin/prospero-clang")
set(CMAKE_CXX_COMPILER "${AKENO_PS5_SDK}/bin/prospero-clang++")
set(CMAKE_AR "${AKENO_PS5_SDK}/bin/prospero-ar")
set(CMAKE_RANLIB "${AKENO_PS5_SDK}/bin/prospero-ranlib")
set(CMAKE_FIND_ROOT_PATH "${AKENO_PS5_SDK}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# Nothing built for the console runs on the build host.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(AKENO_TARGET_PS5 ON CACHE BOOL "" FORCE)
set(AKENO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
