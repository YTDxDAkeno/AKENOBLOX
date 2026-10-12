# AkenoBlox PS5 - the loader's test fixtures.
# Copyright (C) 2026 AkenoBlox contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Built with the host's clang and lld for x86_64-linux-android26, whatever
# compiler builds the rest: the fixtures are freestanding x86-64 objects, the
# same bytes on a host and on the console, so the PS5 build embeds the ones
# it self-tests with (akeno_embed_fixture) from this same rule.

find_program(AKENO_FIXTURE_CC NAMES clang clang-18 clang-19 clang-20 REQUIRED)
find_program(AKENO_FIXTURE_LLD NAMES ld.lld ld.lld-18 ld.lld-19 ld.lld-20 REQUIRED)

set(AKENO_FIXTURE_DIR "${CMAKE_BINARY_DIR}/fixtures")
file(MAKE_DIRECTORY "${AKENO_FIXTURE_DIR}")
set(AKENO_FIXTURE_SOURCE_DIR "${PROJECT_SOURCE_DIR}/tests/fixtures")
set(AKENO_FIXTURE_COMMON
    -fPIC -shared -nostdlib -ffreestanding -fno-builtin -O2
    -fuse-ld=lld -Wl,--build-id=none -Wl,-z,noexecstack)

set(AKENO_FIXTURE_OUTPUTS "")

# akeno_fixture(<output> <source> [TARGET triple] [FLAGS ...] [DEPENDS other.so])
function(akeno_fixture output source)
  cmake_parse_arguments(F "" "TARGET;SONAME" "FLAGS;DEPENDS" ${ARGN})
  if(NOT F_TARGET)
    set(F_TARGET x86_64-linux-android26)
  endif()
  if(NOT F_SONAME)
    set(F_SONAME "libakeno_fixture_${output}")
  endif()
  set(out "${AKENO_FIXTURE_DIR}/${output}")
  set(deps "")
  foreach(dep IN LISTS F_DEPENDS)
    list(APPEND deps "${AKENO_FIXTURE_DIR}/${dep}")
  endforeach()
  add_custom_command(
    OUTPUT "${out}"
    COMMAND "${AKENO_FIXTURE_CC}" --target=${F_TARGET} ${AKENO_FIXTURE_COMMON}
            --ld-path=${AKENO_FIXTURE_LLD}
            -Wl,-soname,${F_SONAME} ${F_FLAGS}
            "${AKENO_FIXTURE_SOURCE_DIR}/${source}" ${deps} -o "${out}"
    DEPENDS "${AKENO_FIXTURE_SOURCE_DIR}/${source}" "${AKENO_FIXTURE_SOURCE_DIR}/fixture_common.h" ${deps}
    COMMENT "fixture ${output}"
    VERBATIM)
  set(AKENO_FIXTURE_OUTPUTS ${AKENO_FIXTURE_OUTPUTS} "${out}" PARENT_SCOPE)
endfunction()

set(nossp -fno-stack-protector)
akeno_fixture(basic_rela_4k.so fixture_basic.c SONAME libakeno_fixture_basic.so
  FLAGS ${nossp} -Wl,--pack-dyn-relocs=none -Wl,-z,max-page-size=4096)
akeno_fixture(basic_rela_16k.so fixture_basic.c SONAME libakeno_fixture_basic.so
  FLAGS ${nossp} -Wl,--pack-dyn-relocs=none -Wl,-z,max-page-size=16384)
akeno_fixture(basic_aps2_4k.so fixture_basic.c SONAME libakeno_fixture_basic.so
  FLAGS ${nossp} -Wl,--pack-dyn-relocs=android -Wl,-z,max-page-size=4096)
akeno_fixture(basic_aps2_16k.so fixture_basic.c SONAME libakeno_fixture_basic.so
  FLAGS ${nossp} -Wl,--pack-dyn-relocs=android -Wl,-z,max-page-size=16384)
akeno_fixture(basic_relr_4k.so fixture_basic.c SONAME libakeno_fixture_basic.so
  FLAGS ${nossp} -Wl,--pack-dyn-relocs=relr -Wl,-z,max-page-size=4096)
akeno_fixture(basic_android_relr.so fixture_basic.c SONAME libakeno_fixture_basic.so
  FLAGS ${nossp} -Wl,--pack-dyn-relocs=android+relr -Wl,--use-android-relr-tags)
akeno_fixture(basic_sysv.so fixture_basic.c SONAME libakeno_fixture_basic.so
  FLAGS ${nossp} -Wl,--hash-style=sysv)
akeno_fixture(basic_both_hash.so fixture_basic.c SONAME libakeno_fixture_basic.so
  FLAGS ${nossp} -Wl,--hash-style=both)
akeno_fixture(dep.so fixture_dep.c SONAME libakeno_fixture_dep.so FLAGS ${nossp})
akeno_fixture(needs_dep.so fixture_needs_dep.c FLAGS ${nossp} DEPENDS dep.so)
akeno_fixture(ifunc.so fixture_ifunc.c FLAGS ${nossp})
akeno_fixture(tls_gnu.so fixture_tls.c TARGET x86_64-linux-gnu FLAGS ${nossp})
akeno_fixture(tls_android.so fixture_tls.c FLAGS ${nossp})
akeno_fixture(missing.so fixture_missing.c FLAGS ${nossp})
akeno_fixture(weak.so fixture_weak.c FLAGS ${nossp})
akeno_fixture(canary.so fixture_canary.c FLAGS -fstack-protector-all)

add_custom_target(akeno_fixtures ALL DEPENDS ${AKENO_FIXTURE_OUTPUTS})

# akeno_embed_fixture(<target> <fixture.so> <c symbol>): a generated C source
# holding the fixture's bytes, added to <target>.
function(akeno_embed_fixture target fixture symbol)
  set(in "${AKENO_FIXTURE_DIR}/${fixture}")
  set(out "${CMAKE_BINARY_DIR}/generated/${symbol}.c")
  add_custom_command(
    OUTPUT "${out}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/build/embed.py"
            "${in}" "${out}" "${symbol}"
    DEPENDS "${in}" "${PROJECT_SOURCE_DIR}/tools/build/embed.py"
    COMMENT "embed ${fixture} as ${symbol}"
    VERBATIM)
  target_sources(${target} PRIVATE "${out}")
endfunction()
