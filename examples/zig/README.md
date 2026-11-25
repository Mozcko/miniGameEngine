# Zig Example (`examples/zig`)

A minimal **Zig** program using the miniGameEngine C API (`<mge/c/mge_c.h>`) directly via `@cImport`.
It renders a spinning procedural cube with a dynamic sky, plays a synthesized coin sound on `Space`,
displays 2D bitmap text, and exits on `Esc`.

## Prerequisites

- **Zig** 0.14 or 0.15
- `libmge_c` built as a shared library (`-DMGE_BUILD_C_SHARED=ON`)

## 1. Build the `mge_c` shared library

From the repository root:

```bash
cmake --preset dev -DMGE_BUILD_C_SHARED=ON
cmake --build --preset dev --target mge_c_shared
```

This produces `build/src/mge/c/libmge_c.so` (or `libmge_c.dylib` on macOS / `mge_c.dll` on Windows).

## 2. Build and run

From `examples/zig/`:

```bash
zig build run
```

If you built the engine in a different directory (for example `build-release/`), pass `-Dmge-lib`:

```bash
zig build run -Dmge-lib=../../build-release/src/mge/c
```

You can also override the header include root with `-Dmge-include=<path>` (defaults to `../../src`).

## Headless & offscreen smoke test

Run 60 frames offscreen (no display or audio device required):

```bash
zig build
MGE_MAX_FRAMES=60 SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy ./zig-out/bin/zig_hello
```

Or run completely headless (no SDL/GPU modules initialized):

```bash
zig build run -- --headless
```
