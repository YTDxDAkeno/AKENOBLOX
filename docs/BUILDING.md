# Building

## On a host (development and tests)

Linux x86-64 with CMake 3.20+, Ninja or Make, Python 3, a C compiler (GCC or
Clang) **and** Clang with lld: the loader's test fixtures are always built
with Clang for `x86_64-linux-android26`, whatever builds the rest.

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/akenoblox-host                      # the launcher, headless
./build/akenoblox-host --diagnostics d.txt  # probes + self-test, report, exit
```

The host program takes scripted input and writes its last frame as a PPM:

```bash
./build/akenoblox-host --fast --script "down,down,cross,cross,wait:3" --screenshot shot.ppm
```

Sanitizers and fuzzing (Clang):

```bash
cmake -S . -B build-asan -G Ninja -DCMAKE_C_COMPILER=clang -DAKENO_SANITIZE=ON
cmake -S . -B build-fuzz -G Ninja -DCMAKE_C_COMPILER=clang -DAKENO_FUZZ=ON
cmake --build build-fuzz && ./build-fuzz/tests/fuzz_elf_load -max_total_time=60 build-fuzz/fixtures
```

## For the console

```bash
tools/build/build-ps5.sh            # -> dist/PPSA99256/ and release/*.zip
```

It runs `tools/build/setup-ps5-deps.sh` first, which fetches and builds, each
pinned and checked:

| What | Pin | Check |
|---|---|---|
| ps5-payload-dev SDK v0.42 release | sha256 `8cfbc7cd…02da` | the fork's installer verifies it |
| mihawk-99/PS5_PayloadSDK (platform layer) | `611893fc25ef` | git revision |
| mihawk-99/PS5_Vulkan (CRT, link recipe, native tool, libc.prx) | `252bf912a745` | libc.prx rebuilt twice and checked against PS5_Vulkan's digests |
| zlib 1.3.2 (for the native tool) | sha256 `bb329a0a…9d16` | fetched from zlib.net or GitHub, hash decides |

Host requirements beyond the host build: `git curl unzip sha256sum make
flock clang clang++` and Clang's builtins archive
(`libclang_rt.builtins-x86_64.a`; on Ubuntu `libclang-rt-18-dev`).

The title is linked the way every PS5_VulkanTemplate title is: PS5_Vulkan's
`app_crt.cpp`, its `radv-link.sh` recipe (given an empty archive where RADV
goes, until the engine needs RADV), `ps5-native-tool link` and `self --sign`.
The link refuses any import only `libkernel_sys` or `libScePosixForWebKit`
define, because a title gets those as null (runtime-surface.md).

A green build is a **cross-build**, nothing more: see
[TEST_REPORTS.md](TEST_REPORTS.md) for what has run where.

## Bumping a pin

Change it in `tools/build/setup-ps5-deps.sh`, rebuild, rerun the host tests,
and record a console run of the new build before calling the bump done.
