# font8x8 (basic Latin)

`font8x8_basic.h` is copied unmodified from
[dhepper/font8x8](https://github.com/dhepper/font8x8) at commit
`8e279d2d864e79128e96188a6b9526cfa3fbfef9` (2019-07-08).

- Author: Daniel Hepper, based on Marcel Sondaar's font8x8 and IBM's public
  domain VGA fonts.
- Licence: **Public Domain**, as stated in the file header and the upstream
  README.

Used by `src/ui/akeno_font.c` to draw the launcher's text. Only U+0020 to
U+007E are drawn; anything else renders as `?`.
