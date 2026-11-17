# Notes for AI coding agents

This repository is libgui, the GUI toolkit of PureC OS, a 64-bit operating
system written from scratch in C. It is a submodule of the main PureC OS tree
(`PureC-OS-LibGui/`) and is built only from there.

## Ground rules

- PureC OS is not Linux. The library runs on PureC libc and the `videod`
  display server through `libvb`; do not add other dependencies.
- Write C, no comments in code, match the style of the surrounding file.
- Do not commit, push or open pull requests unless the maintainer asks.
- Do not change the public API in `include/pg/` without saying so; programs
  `login`, `terminal` and `settings` link this library statically and must be
  rebuilt after an API or behavior change.
- Talk to the maintainer in Russian.

## Build and check

    make -C PureC-OS-LibGui                 # cross build, must have no warnings
    make -C PureC-OS-LibGui package         # signed package in bin/libgui/
    python3 mk/pkg/publish.py --registry Trusted bin/libgui/libgui-VERSION-1-x86_64.pkg.tar.gz

Host testing: compile `src/*.c` with host `gcc -fsanitize=address,undefined`
together with a fake `libvb` (windows in malloc'd memory, a scripted event
queue) and a test program; save window pixels to PPM to look at them.

## Where things are

- `src/window.c`: windows, client-side decorations, input dispatch, damage,
  the private back buffer copied to the shared `videod` buffer on flush.
- `src/widget.c`, `src/containers.c`, `src/widgets_*.c`: widget tree, layout
  and widgets.
- `src/app.c`: event loop (`poll` on the videod socket, timers, fd watches).
- `src/theme.c`: palettes and the `appear.ini` watcher.
- `src/canvas.c`, `src/font_data.c`: drawing and generated fonts and icons
  (regenerate with `tools/gen_font.py`, never edit `font_data.*` by hand).

## Publishing a package

Before saying a package is published: old versions removed from `Trusted/bin`,
`repository.conf` lists only existing archives with `.sig` and a description,
`registry.conf` has the repository URL, both `.conf` files re-signed and their
signatures verified.
