## What changed

## How it was tested

- [ ] Host build and `ctest` (GCC)
- [ ] Clang with `-DAKENO_SANITIZE=ON` and `ctest`
- [ ] `tools/build/build-ps5.sh --no-zip` (cross-build)
- [ ] Fuzzed the loader (if `src/compat/elf/` changed)
- [ ] Ran on a console: _paste the klog lines, or leave unticked_

Verification level reached (docs/TEST_REPORTS.md): HOST VERIFIED / CROSS-BUILD VERIFIED / HARDWARE VERIFIED

## What is still unverified

## Documentation updated

- [ ] PROJECT_STATE.md / TEST_REPORTS.md / DECISIONS.md as needed
- [ ] NOTICE.md if third-party code was incorporated

No Roblox files, no engine patching or hooking, no stubs that report success.
