# WNU's not Unix

A tiny OS in C

Build:

```sh
make
```

Run in QEMU:

```sh
make run
```

Project layout
--------------

- `src/` — kernel sources
	- `src/drivers/` — platform drivers (RTL8139, etc.)
	- `src/network/` — network stack and protocol helpers
	- other components remain in `src/` (console, vfs, shell, arch)

The build system auto-discovers `.c` files under `src/` so moving files
into subdirectories is supported without Makefile changes.

The build expects Limine binaries in `~/limine-bin` and uses `assets/ter-u16n.psf` as the font asset.
