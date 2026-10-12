# AkenoBlox PS5 - host tests.
# Copyright (C) 2026 AkenoBlox contributors
# SPDX-License-Identifier: GPL-3.0-or-later

function(akeno_test name)
  add_executable(${name} unit/${name}.c)
  akeno_defaults(${name})
  target_link_libraries(${name} PRIVATE akeno_core)
  target_include_directories(${name} PRIVATE "${PROJECT_SOURCE_DIR}/tests/unit")
  target_compile_definitions(${name} PRIVATE AKENO_FIXTURE_DIR="${AKENO_FIXTURE_DIR}"
                             AKENO_REGRESS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/regress")
  add_dependencies(${name} akeno_fixtures)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

akeno_test(test_elf)
akeno_test(test_input)
akeno_test(test_settings)
akeno_test(test_menu)
akeno_test(test_canvas)
akeno_test(test_runtime)
akeno_test(test_selftest)
akeno_test(test_tiling)

# The program itself, driven by scripted input through every screen.
add_test(NAME host_journey
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/integration/host_journey.py"
          "$<TARGET_FILE:akenoblox-host>" "${CMAKE_BINARY_DIR}/journey")

if(AKENO_FUZZ)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "AKENO_FUZZ needs clang (libFuzzer)")
  endif()
  foreach(target fuzz_elf_parse fuzz_elf_load fuzz_settings)
    add_executable(${target} fuzz/${target}.c)
    akeno_defaults(${target})
    target_link_libraries(${target} PRIVATE akeno_core)
    target_compile_options(${target} PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_options(${target} PRIVATE -fsanitize=fuzzer,address,undefined)
  endforeach()
endif()
