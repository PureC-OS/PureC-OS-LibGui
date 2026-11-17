# Contributing to libgui

## Code

- C11, compiled with the PureC OS cross toolchain (`x86_64-elf-gcc`,
  hosted PureC libc headers). No other libraries besides `libvb` and libc.
- Public names start with `pg_` (API), `pgc_` (canvas), `pgf_`/`PGI_`
  (fonts, icons); internal helpers with `pg__`. Public headers live in
  `include/pg/`, private ones in `src/`.
- No comments in the code: names and small functions explain it.
- Formatting follows the existing files (4 spaces, opening braces of function
  bodies and blocks on their own line).
- The build must stay free of warnings with `-Wall -Wextra`.
- A new widget is a `struct pg_widget_class` in `src/widgets_*.c` with a
  constructor and setters in `include/pg/widgets.h`. Colors come from the
  theme (`pg_theme()`, `PG_COLOR_*` roles), never fixed values, so themes keep
  working.
- Anything a callback may destroy (a widget, a window) must not be touched
  after the callback returns; see how `window.c` checks `win->grab`.

## Checks before a pull request

1. `make -C PureC-OS-LibGui` without warnings.
2. Run a program that uses the change on the host with a fake `libvb`
   (AddressSanitizer on) or in PureC OS; describe what you tried.
3. If the API changed: bump `VERSION` and `PG_VERSION_*` in
   `include/pg/app.h`, and rebuild the programs that link libgui
   (`login`, `terminal`, `settings`) — it is linked statically.
4. Packages for the registry are signed and built for PureC OS x86_64; see
   CONTRIBUTING.md of `PureC-OS-Trusted-PKG-URL`.

Bugs and ideas: open an issue with one of the templates.
