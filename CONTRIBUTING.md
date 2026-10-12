# Contributing

Thank you for considering it. AkenoBlox is experimental and honest about it;
contributions keep it that way.

## The most valuable contribution: a console run

If you have a jailbroken PS5, follow [docs/HARDWARE_TESTING.md](docs/HARDWARE_TESTING.md)
and open a **Hardware test report** issue. A failed run reported in detail is
worth as much as a pass.

## Code

1. Open an issue first for anything larger than a fix, so the approach can be
   agreed before the work.
2. Branch, change, and run all three builds (host, ASan/UBSan, PS5 cross-build;
   commands in [AGENTS.md](AGENTS.md)). Add tests for what you change; a
   behaviour without a test is a claim.
3. Keep commits small and their messages short: what changed, what you
   measured, what is not yet verified.
4. Update the documentation the change makes wrong, in the same pull request.
5. Fill in the pull request template, including how it was tested and where.

## What will not be accepted

- Roblox binaries, APKs, assets or decompiled material, anywhere.
- Patching, hooking or injecting into the engine; spoofing device integrity;
  forging tokens; evading detection; anything that helps cheat.
- Streaming or remote-play substitutes for running the engine locally.
- Stubs that report success for something not implemented.
- Code copied from a source whose licence is unknown or incompatible.
- Claims of console behaviour without a console log behind them.

## Licence

By contributing you agree that your contribution is licensed under
GPL-3.0-or-later, the project's licence.
