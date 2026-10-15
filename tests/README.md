# tests/

Unit tests for the engine and the sandbox world, using **doctest** with **CTest**.

- **No display or GPU needed.** Tests only run CPU code, so they work in CI and over SSH.
- **doctest** is downloaded by CMake (`v2.4.11`, SHA‑256 pinned in `tests/CMakeLists.txt`). It is
  never linked into the game.
- Each `TEST_CASE` is registered as its own CTest test (`doctest_discover_tests`).

```bash
cmake --preset dev && cmake --build --preset dev
ctest --preset dev                      # all tests
./build/tests/sandbox_tests -tc="*determin*" # filter by name (doctest CLI)
```

| File | Covers |
|------|--------|
| `test_main.cpp` | doctest `main()` – no tests here |
| `core/test_time.cpp` | `Time`: scaling, pause, hitch clamp, real clock never negative |
| `sandbox/test_block.cpp` | `Block` active/solid/opaque rules, atlas UV lookup per face (`Block::getUVs`) |
| `sandbox/test_chunk.cpp` | `Chunk` generation: determinism, terrain rules (solid floor, water ≤ level 5), out‑of‑range access |

## Writing tests
- Add `core/test_<topic>.cpp` (engine core) or `sandbox/test_<topic>.cpp`, and list it in `tests/CMakeLists.txt`.
- Link only what you test. `mge_core_tests` (`tests/core/`) links **only `mge_core`**, so it
  also builds with `-DMGE_BUILD_MODULES=OFF`. `sandbox_tests` (`tests/sandbox/`) links `sandbox_world`.
  CTest names are prefixed `core: ` / `sandbox: `.
- **Never** call code that needs a GPU context. For example, `Chunk::buildMesh()` creates OpenGL
  buffers, so construct chunks and test their data without it. (`Chunk(cx, cz)` only generates
  blocks.)
- Allocate chunks on the heap (`std::make_unique<Chunk>`): each one is about 16 KB of blocks.
- Code must build warning‑free under the `ci` preset (`-Werror`) with GCC and Clang. That means
  C++17 only: no C++20 features such as lambda‑captured structured bindings.
- Test names and comments follow the code's convention (Spanish).

## Smoke tests (whole programs)

Besides the unit tests, CTest runs two whole programs:

| CTest name | Program | Needs |
|------------|---------|-------|
| `smoke: headless 1000 frames` | `examples/headless_smoke`: an `Application` with no modules, generating and checking a chunk per frame | nothing (no SDL init, no GPU) |
| `smoke: sandbox offscreen 120 frames` | The real `sandbox`, with `SDL_VIDEODRIVER=offscreen`, `SDL_AUDIODRIVER=dummy`, `MGE_MAX_FRAMES=120` | an OpenGL driver (Mesa llvmpipe is enough). Label **`gpu`**; skip with `ctest -LE gpu` |

CI runs all of them, and runs them again under ASan/UBSan in the `sanitizers` job. For the
sandbox smoke run only, **leak** detection is off: Ubuntu's Mesa EGL driver leaks inside
`eglInitialize`, and LeakSanitizer can't attribute the memory because the driver is already
unloaded. Address and UB checks stay on, and every other test runs with `detect_leaks=1`.

