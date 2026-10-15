# Contributing to miniGameEngine

Thanks for helping! This guide covers how work is organised, how to build and test, and the rules
every change must follow. Start with [README.md](README.md) and the
[roadmap](docs/ROADMAP.md).

## Project rules (non‑negotiable)

These come from the [architecture decision records](docs/adr/README.md):

1. **No new features on OpenGL.** Rendering moves to the SDL3 GPU API
   ([ADR 0001](docs/adr/0001-sdl3-gpu-api.md)). The OpenGL code is legacy: fix it only when a bug
   blocks you, and it will be deleted after parity (#40).
2. **Zero external asset files** in the shipped game: no images, models, fonts, audio or shader
   files at runtime. Generate content in code ([ADR 0002](docs/adr/0002-zero-asset-policy.md)).
3. **The core stays headless.** Core code must not include SDL or GPU headers and must be
   testable without a display ([ADR 0003](docs/adr/0003-headless-core.md)). Until the split in #8
   lands, keep new logic separable from rendering, like `Chunk` generation vs `buildMesh()`.

## Workflow

1. **Pick an issue.** Every change starts from a GitHub issue. Check the
   [milestones](https://github.com/Mozcko/miniGameEngine/milestones) and the issue's
   **Depends on** section. `good first issue` marks easy entry points.
2. **Branch** from up‑to‑date `main`, named `<type>/<issue>-<slug>`, for example
   `fix/5-tree-spawn` or `chore/4-ci`.
3. **Commit** using [Conventional Commits](https://www.conventionalcommits.org/):
   `type: imperative summary`. Types: `feat`, `fix`, `refactor`, `perf`, `test`, `docs`, `chore`,
   `ci`. Explain *why* in the body and add `Closes #N`.
4. **Open a PR**:
   - one issue per PR;
   - fill in the template;
   - add the matching labels and milestone;
   - the PR must say how the change was verified.
5. **Merge** only when CI is green (gcc, clang, sanitizers, core-only, lint). Use a merge commit and delete the branch.

## Build and test

```bash
cmake --preset dev && cmake --build --preset dev   # Debug, warnings → build/
ctest --preset dev                                 # headless unit tests
scripts/run.sh                             # build + run (works from any directory)
```

- Reproduce CI with `cmake --preset ci -G Ninja && cmake --build --preset ci && ctest --preset ci`
  (`-Werror`).
- Details are in [docs/BUILDING.md](docs/BUILDING.md); tests are described in
  [tests/README.md](tests/README.md).

**Every PR must:**
- build **warning‑free** with GCC and Clang (`-Wall -Wextra -Wshadow -Werror`);
- pass all tests;
- add tests for new logic and for every bug fix. Write a regression test that fails before the fix.

## Code conventions

- **Language:** C++17. No compiler extensions, no C++20 features.
- **Comments are in Spanish**, matching the existing code. Docs, issues and PRs are in English.
- **Namespace:** all engine code lives in `mge`.
- **Naming:** enforced by `.clang-tidy` (`readability-identifier-naming`) in the CI `lint` job.

  | Kind | Style | Example |
  |---|---|---|
  | Types, enums, enum values | `PascalCase` | `ChunkWorld`, `BlockType::Grass` |
  | Functions, methods | `camelCase` | `getBlock()`, `onUpdate()` |
  | Private/protected members | `m_camelCase` | `m_chunks` |
  | Static members | `s_camelCase` | `s_instances` |
  | Static/constexpr constants | `kPascalCase` | `kChunkSize` |
  | Public data fields, locals, parameters | `camelCase` | `vertex.position`, `tileIndex` |
  | Namespaces | `lower_case` | `mge` |
  | Macros | `UPPER_CASE` | `MGE_ASSERT` |

  A third‑party name that must keep its spelling (for example a forward‑declared `SDL_Surface`)
  gets `// NOLINT(readability-identifier-naming)`.
- **Formatting:** `.clang-format`, checked in CI with clang-format **22.1.8** (`pip install
  clang-format==22.1.8` if your distro ships another version).
- **Lint locally** with the same checks as CI: `cmake --preset ci && scripts/lint.sh`
  (`scripts/lint.sh --fix` formats first). New, untracked files are included.
- **Blame:** mass‑formatting commits are listed in `.git-blame-ignore-revs`. Run
  `git config blame.ignoreRevsFile .git-blame-ignore-revs` once.
- **Errors and output:** use the logging macros from `<mge/core/Log.h>` (`MGE_LOG_*`, `MGE_ASSERT`,
  `MGE_VERIFY`). Never `std::cout`, `printf` or `exit()`: log, then return an error to the caller.
- **Ownership:** classes that own GPU or OS resources must not be copyable. Prefer RAII and
  `std::unique_ptr` over raw `new`/`delete` in new code.
- **Determinism:** procedural generation must be deterministic for a given seed. Use
  `<mge/core/Random.h>` (`Pcg32`, `hash2i`/`hash3i`) and never `rand()`/`srand()` or `std::random_device`.
  Seed per entity or coordinate from a hash; don't share one generator across unrelated systems.
- **Documentation:** each folder under `src/` has a `README.md`. Update it, and
  `docs/KNOWN_ISSUES.md` when you fix a listed issue, in the same PR.

## Labels

| Group | Labels |
|---|---|
| Type | `bug`, `enhancement`, `refactor`, `chore`, `performance`, `testing`, `documentation`, `epic` |
| Area | `area:build`, `area:core`, `area:platform`, `area:procgen`, `area:voxel`, `area:render`, `area:ui`, `area:audio`, `area:physics`, `area:game` |
| Priority | `priority:critical` (blocks others), `priority:high`, `priority:medium`, `priority:low` |
| Size | `size:S` (≤ 1 day), `size:M` (a few days), `size:L` (a week or more) |

Each milestone has an `epic` issue that tracks its children.

## Architecture decisions

Significant technical decisions are recorded as ADRs in [docs/adr/](docs/adr/README.md). To
change a decision, open a PR with a new ADR that supersedes the old one; don't edit accepted ADRs.
