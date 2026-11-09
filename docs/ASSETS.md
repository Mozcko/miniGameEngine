# Asset Architecture & Systems

miniGameEngine follows [ADR 0004: Procedural-first, assets optional](adr/0004-procedural-first-assets-optional.md). The engine is **procedural-first**: every subsystem offers a code- or math-driven path so that an entire game can run as a self-contained binary with zero external asset files. At the same time, asset loading is a supported, optional path behind clean APIs. Games can freely choose either path or mix them.

---

## System Overview: Code Path vs File Path

| System | Code Path (Procedural / In-Memory) | File Path (Optional Asset Loading) | Module | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Textures** | `Texture2D::fromPixels(...)`, procedural texture generators (`mge::texgen`, `sandbox::buildProceduralAtlas`) | `Texture2D::fromFile(device, path)` via `mge::assetPath("textures/...")` | `mge_graphics` | Loads RGBA8 PNGs via `stb_image`. Returns `{}` on failure. |
| **Fonts** | `Font::builtin()` (embedded 8×8 bitmap font atlas), `loadTtfFont(bytes, height)` | `loadTtfFontFile(path, height)` via `mge::assetPath("fonts/...")` | `mge_graphics` | TTF rasterization via `stb_truetype`. Returns invalid `Font` on failure. |
| **Meshes** | `Primitives::*` (cubes, spheres, cylinders, quads), `MeshOps::*`, `Sdf::*`, `buildChunkMesh(...)` | *N/A* (procedural-only; export to `.obj` via `writeObj` for Blender/tools) | `mge_procgen`, `mge_voxel` | Meshes are constructed directly in memory as `MeshData`. |
| **Audio (Samples)** | `mge::sfx::*` (synthesizer presets + variations), `AudioModule::addSfx(...)` | `AudioModule::loadWav(path)` via `mge::assetPath("sounds/...")` | `mge_synth`, `mge_audio` | Loads uncompressed WAV via `SDL_LoadWAV`, converted to float32 at mixer rate. Returns `kInvalidSample` on failure. |
| **Audio (Music)** | `mge::songs::*`, procedural synthesizer sequencer (`mge::Song`), `AudioModule::addSong(...)` | *N/A* (music is synthesized in code via instruments and note sequences) | `mge_synth`, `mge_audio` | Synthesized in real time on the audio callback; zero runtime audio files needed. |
| **Levels** | In-code procedural generation (`SandboxTerrain`), `platformer::parseLevel(text)` | `platformer::loadLevel(path)` via `mge::assetPath("levels/...")` | Game layer (e.g. `examples/platformer2d`) | ASCII level maps parsed into tile grids and entity spawn lists. Returns `nullopt` on failure. |
| **Shaders** | `mge_add_shaders(...)` compiles GLSL to embedded SPIR-V headers at build time | *N/A* (always embedded as bytecode, never read from disk at runtime) | CMake / `mge_gpu` | Zero runtime shader files or compilation overhead. |

---

## Path Resolution: `mge::assetPath()`

Asset paths should always be resolved using `mge::assetPath(...)` declared in `<mge/core/Assets.h>`. Do not use hardcoded relative path literals such as `"../assets"` in code.

When `mge::assetPath(relative)` is called, the asset root is resolved **lazily** on the first invocation. Resolution checks the following locations in order:

1. **`MGE_ASSET_DIR` environment variable**: If set and points to an existing directory, it takes precedence.
2. **Adjacent assets**: `<executable_dir>/assets` (used in packaged distribution builds).
3. **Parent assets**: `<executable_dir>/../assets` (standard development layout: `build/sandbox` &rarr; `assets/`).
4. **Source tree fallback**: The compile-time `${PROJECT_SOURCE_DIR}/assets` directory when `MGE_SOURCE_ASSET_FALLBACK=ON` (the default for development).
5. **Relative fallback**: `"assets"` relative to the current working directory. If no assets directory exists, no warnings are emitted; missing files are reported by individual loaders when a file is actually requested.

### Lazy Resolution & Zero-Asset Guarantee

- **Code-only binaries**: A program or game that does not load external assets never calls `mge::assetPath()`. As a result, the asset root is never resolved, and no asset discovery logs or warnings are produced.
- **`MGE_SOURCE_ASSET_FALLBACK` option**: Configured via CMake (`-DMGE_SOURCE_ASSET_FALLBACK=ON|OFF`, default `ON`). When disabled, the source repository path is not compiled into the binary, ensuring that tests and CI checks run against an honest isolated environment without hidden access to the repo's assets.

---

## Loader Rules & Error Handling

All loaders adhere to the following principles:

1. **Module placement**: Loaders live in their respective modules (`mge_graphics`, `mge_audio`, or game logic), never in `mge_core` (ADR 0003). `mge_core` provides only path resolution in `Assets.h`.
2. **Clean error reporting**: Loaders report failures via `MGE_LOG_ERROR` (or structured error output) and return an invalid or empty object (`{}` for `Texture2D`/`Font`, `kInvalidSample` for audio, `std::nullopt` for `Level`). Loaders never throw exceptions, call `std::exit`, or crash.
3. **Path flexibility**: Loaders accept paths built with `mge::assetPath(...)` or relative paths (which they resolve through `mge::assetPath` automatically).

---

## Zero-Asset Verification in CI

The CI pipeline includes a dedicated **`zero-asset`** job (`.github/workflows/ci.yml`) to enforce that the engine's procedural promise is never broken by regressions:

1. Compiles executables in `Release` mode with `MGE_SOURCE_ASSET_FALLBACK=OFF`.
2. Copies only the procedural binaries (`headless_smoke`, `sandbox`, `procgen_gallery`) into an empty temporary directory completely outside the repository.
3. Unsets `MGE_ASSET_DIR` and runs each binary with `SANDBOX_ATLAS=procedural`, `SDL_VIDEODRIVER=offscreen`, and `SDL_AUDIODRIVER=dummy`.
4. Executes under `strace -f -e trace=openat` to verify that:
   - The process exits cleanly (exit code `0`).
   - No log messages contain `Assets` warnings or errors.
   - No `openat` system call attempts to access files under the repository's `assets/` directory.
