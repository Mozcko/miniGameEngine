# examples/

Small programs built on the engine, one folder each. `examples/CMakeLists.txt` lists them, and
`scripts/new-game.sh` appends new ones there.

| Folder | Target | What it shows |
|--------|--------|---------------|
| `procgen_gallery/` | `procgen_gallery` | Exports procedurally generated meshes to `<build>/gallery/*.obj` (view them in Blender): primitives, mesh ops, noise terrain, SDFs, and a colour‑only voxel world (`voxel_terrain`, `voxel_water`: greedy + AO; the triangle saving is logged). Headless, built even without modules; its smoke test fails if a mesh is invalid |
| `gpu_triangle/` | `gpu_triangle` | The first SDL3 GPU program: `PlatformModule` without OpenGL + `GpuModule`; a rotating triangle with GLSL shaders compiled to SPIR‑V at build time and embedded (`mge_add_shaders`). Smoke test: 60 frames offscreen (fails if nothing was presented) |
| `headless_smoke/` | `headless_smoke` | An `mge::Application` with **no modules**: no window, no GPU, SDL never initialised. It runs 1000 frames, generating and meshing a sandbox chunk column (4 chunks tall) per frame (`mge_voxel` + `SandboxTerrain`), checking terrain invariants and mesh validity. It also travels 4 000 blocks with a `ChunkStreamer` on the job pool, fails if loaded chunks exceed the unload ring, and logs the worst main‑thread update. It exits non‑zero on failure (`setExitCode`). CTest: `smoke: headless_smoke headless 1000 frames`, also run under ASan/UBSan in CI |

New game or tool: `scripts/new-game.sh <name> [--headless]` creates `examples/<name>/` from
[`templates/`](../templates/README.md). Every program is declared with `mge_add_game()` (see
`docs/BUILDING.md`, "Adding a game"). Examples are built when `MGE_BUILD_MODULES=ON`.
