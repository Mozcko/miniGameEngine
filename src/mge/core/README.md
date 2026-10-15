# src/mge/core/  →  target `mge_core`

The **headless core**: standard library only (GLM later). **No SDL, no GPU headers** – enforced
by the CI `core-only` job, which builds this target on a machine without SDL/GL installed
(`-DMGE_BUILD_MODULES=OFF`). See [ADR 0003](../../../docs/adr/0003-headless-core.md).

| File | Namespace | Purpose |
|------|-----------|---------|
| `Time.h/.cpp` | `mge` | Static clock (`std::chrono::steady_clock`) with time scale and hitch clamping |

Upcoming in M1: logging (#13), application runner + modules (#11), fixed‑timestep clock (#12),
input state (#15), RNG (#14), ECS (#16).

Tests: `tests/core/` → `mge_core_tests` (links only `mge_core`).

---

## `mge::Time`

All‑static clock. Must be driven by the game: call `Time::init()` once and `Time::update()`
once per frame (SandboxApp does this at the start of `onUpdate`).

| Method | Returns / effect |
|--------|------------------|
| `init()` | Stores the current `steady_clock` time. |
| `update()` | Computes raw dt; **clamps to 0.1 s** (protects against window drags / breakpoints); `deltaTime = raw * timeScale`. |
| `getDeltaTime()` | Scaled dt – use for gameplay (0 when paused). |
| `getUnscaledDeltaTime()` | Real dt – use for UI, animations that run while paused. |
| `getTimeScale()` / `setTimeScale(s)` | 1 = normal, 0 = paused, 0.5 = slow motion. |
| `advance(raw)` | Applies a raw delta: clamps to [0, `kMaxDeltaTime` = 0.1 s], then scales. `update()` calls it with the real clock; tests call it directly. |
