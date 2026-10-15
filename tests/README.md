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
