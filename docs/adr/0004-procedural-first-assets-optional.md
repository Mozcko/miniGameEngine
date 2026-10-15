# 0004. Procedural‑first, assets optional

- Status: Accepted
- Date: 2026-10-15
- Supersedes: [0002](0002-zero-asset-policy.md)
- Issues: #34, #41, #45, #46, #48, #49, #62, #65

## Context
[ADR 0002](0002-zero-asset-policy.md) made "zero external asset files" mandatory: everything
generated in code, and every file loader (images, fonts, audio) removed by M8.

The project owner clarified the goal. The engine's distinguishing feature is that a game **can**
be built entirely from code and math. That should be a choice, though, not a restriction. Real
projects mix the two: a procedural world with a hand‑made logo, a sampled sound, a TTF font, or a
texture. Forbidding assets would push those users away without benefiting the procedural ones.

## Decision
- The engine is **procedural‑first**. Every system offers a code/math path: procedural meshes,
  materials, sky, SDFs, an embedded font, synthesized audio, and so on. Docs and templates lead
  with it.
- **Asset loading is a supported, optional path.** Textures, fonts, sounds/music and shader files
  can be loaded through clean engine APIs (`mge::assetPath`, loaders in the relevant module).
  Games choose per feature, and can mix both.
- A **zero‑asset single‑binary build stays possible**: shaders are embedded as bytecode, and no
  system *requires* a file. CI keeps a check that a code‑only game runs from an empty directory.
- Systems must not assume either path. A voxel block, for example, may define a procedural
  colour, an atlas texture, or both.

## Alternatives considered
- **Keep 0002 (zero assets mandatory).** Rejected by the owner: too restrictive for real games.
- **Assets‑first engine with optional procedural helpers.** Loses the engine's identity and the
  tiny, code‑only builds.

## Consequences
- **Roadmap re‑scoped:**
  - #65 no longer deletes loaders. It keeps them behind clean APIs and verifies a zero‑asset build.
  - #34 adds procedural materials *alongside* textures.
  - #41 makes the embedded font the default, with TTF still possible.
  - #45 removes only the legacy `Sprite`/`Animation`.
  - #46/#48/#49 add synthesis *plus* sample playback.
  - #62 lets the showcase use either path.
- Loaders stay in modules (render, audio, UI), never in `mge_core`. That keeps the core headless
  ([ADR 0003](0003-headless-core.md)).
- Binary size (#60) is still tracked, measured for a code‑only build.
