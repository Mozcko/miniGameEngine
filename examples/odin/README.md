# Odin Example (`examples/odin`)

A minimal **Odin** program using the miniGameEngine C API (`<mge/c/mge_c.h>`) through the foreign
bindings in [`mge/mge_c.odin`](mge/mge_c.odin).
It renders a spinning procedural cube with a dynamic sky, plays a synthesized coin sound on `Space`,
displays 2D bitmap text, and exits on `Esc`.

## Prerequisites

- **Odin** compiler (`odin`)
- `libmge_c` built as a shared library (`-DMGE_BUILD_C_SHARED=ON`)

## 1. Build the `mge_c` shared library

From the repository root:

```bash
cmake --preset dev -DMGE_BUILD_C_SHARED=ON
cmake --build --preset dev --target mge_c_shared
```

This produces `build/src/mge/c/libmge_c.so` (or `libmge_c.dylib` on macOS / `mge_c.dll` on Windows).

## 2. Build and run

From `examples/odin/`:

```bash
# Linux / macOS
MGE_LIB="$(cd ../../build/src/mge/c && pwd)"
odin build . -out:odin_hello -extra-linker-flags:"-L${MGE_LIB} -Wl,-rpath,${MGE_LIB}"
./odin_hello
```

If you built the engine in a different directory (for example `build-release/`), point `MGE_LIB` to
that directory's `src/mge/c` folder.

## Headless & offscreen smoke test

Run 60 frames offscreen (no display or audio device required):

```bash
MGE_MAX_FRAMES=60 SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy ./odin_hello
```

Or run completely headless (no SDL/GPU modules initialized):

```bash
./odin_hello --headless
```
