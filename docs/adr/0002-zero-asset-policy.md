# 0002. Zero external asset files

- Status: **Superseded by [0004](0004-procedural-first-assets-optional.md)** (2026-10-15): procedural‑first, assets optional
- Date: 2026-10-14
- Issues: #29, #34, #41, #46–#49, #65

## Context
The engine's purpose is to make games from **pure math and code**: no 3D models, images, fonts or
recorded audio. The resulting games should be extremely lightweight and easy to distribute.

Today the sandbox loads several files at runtime:
- a PNG tileset (stb_image);
- TTF fonts (SDL_ttf);
- WAV and MP3 audio (SDL_mixer);
- GLSL shader text files;
- ASCII level files.

They all come from hard‑coded paths relative to the working directory.

## Decision
A shipped game is **a single executable that reads no files at runtime**.

| Content | Comes from |
|---|---|
| Geometry | Generated procedurally (primitives, mesh operations, voxels, SDF isosurfaces) |
| Surface detail | Procedural shaders: noise, triplanar patterns, palettes (#34) |
| Shaders | Compiled at build time and embedded as bytecode (#29) |
| Text | Bitmap font stored as data in code (#41) |
| Audio | Synthesized at runtime (#46–#49) |
| Configuration | Code (C++ data) |

## Alternatives considered
- **Allow small text files** (shaders, config). Easier iteration, but it breaks single‑file
  distribution and reintroduces working‑directory bugs.
- **Keep file loaders as an optional feature.** Adds dependencies and code paths that
  contradict the engine's identity. It can be revisited with a new ADR if a real use case appears.

## Consequences
- stb_image, SDL_ttf and SDL_mixer are removed, and so is `assets/` (#65). CI verifies that the
  game runs from an empty directory.
- Iterating on shaders and content means recompiling. Hot reload, if ever needed, would be a
  development‑only feature.
- Content creation becomes an engineering problem, so the procgen modules (M2) and procedural
  materials and audio are core priorities, not extras.
- Binary size becomes a tracked metric (#60).
