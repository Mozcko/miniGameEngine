# examples/

Small programs built on the engine, one folder each. `examples/CMakeLists.txt` lists them, and
`scripts/new-game.sh` appends new ones there.

| Folder | Target | What it shows |
|--------|--------|---------------|
| `procgen_gallery/` | `procgen_gallery` | Exports procedurally generated meshes to `<build>/gallery/*.obj` (view them in Blender): primitives, mesh ops, noise terrain, SDFs, a colour‑only voxel world (`voxel_terrain`, `voxel_water`: greedy + AO; the triangle saving is logged), `texture_generators.png` (one sample per `mge::texgen` generator) and `font_builtin.png` (the embedded 8×8 font atlas, ×4). Headless, built even without modules; its smoke test fails if a mesh is invalid |
| `audio_gallery/` | `audio_gallery` | **Sounds from code (#50):** renders the synthesizer offline (no device) to `<build>/gallery/audio/*.wav`, each with a spectrogram `.png`: every waveform, ADSR shapes, a resonant filter sweep, echo + vibrato + pitch slide. Headless, built even without modules; its smoke test fails if a file can't be written or a sound comes out silent |
| `gpu_triangle/` | `gpu_triangle` | The first SDL3 GPU program: `PlatformModule` without OpenGL + `GpuModule`; a rotating triangle with GLSL shaders compiled to SPIR‑V at build time and embedded (`mge_add_shaders`). Smoke test: 60 frames offscreen (fails if nothing was presented) |
| `platformer2d/` | `platformer2d` | **The 2D test program (#45)**: loads the ASCII levels in `assets/levels` (`Level.h/.cpp`), runs a box player with gravity, jump and tile collisions on the fixed step (`Platformer.h/.cpp`, headless, tested in `tests/examples`), and draws everything with `Renderer2D` as coloured rects plus the built‑in font (`PlatformerView.h/.cpp`; golden `platformer_level4`). Plays `jump`/`coin`/`hurt`/`power_up` from `assets/sounds`. A/D or arrows move, Space/W jump, 1–4 pick a level, F1 overlay. Smoke test: 120 frames offscreen |
| `tone/` | `tone` | **Sound with no files (#46):** only the `AudioModule`, no window. A C‑major arpeggio of sine tones panned left to right, then the chord; it stops after 2 s. Smoke test with the dummy audio driver |
| `headless_smoke/` | `headless_smoke` | An `mge::Application` with **no modules**: no window, no GPU, SDL never initialised. It runs 1000 frames, generating and meshing a sandbox chunk column (4 chunks tall) per frame (`mge_voxel` + `SandboxTerrain`), checking terrain invariants and mesh validity. It also travels 4 000 blocks with a `ChunkStreamer` on the job pool, fails if loaded chunks exceed the unload ring, and logs the worst main‑thread update. It exits non‑zero on failure (`setExitCode`). CTest: `smoke: headless_smoke headless 1000 frames`, also run under ASan/UBSan in CI |

New game or tool: `scripts/new-game.sh <name> [--headless]` creates `examples/<name>/` from
[`templates/`](../templates/README.md). Every program is declared with `mge_add_game()` (see
`docs/BUILDING.md`, "Adding a game"). Examples are built when `MGE_BUILD_MODULES=ON`.
