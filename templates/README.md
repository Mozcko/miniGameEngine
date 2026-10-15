# templates/

Game templates used by [`scripts/new-game.sh`](../scripts/new-game.sh). Later the `mge new` CLI
and wizard (#104–#106) will use them too.

| Template | Command | Result |
|----------|---------|--------|
| `window/` | `scripts/new-game.sh <name>` | A window whose clear colour pulses. Shows modules (platform + audio), input actions (Esc quits, hold Space to speed up), an ECS entity updated in the Fixed phase, and logging. Smoke test: 30 frames offscreen |
| `headless/` | `scripts/new-game.sh <name> --headless` | No window, GPU or SDL. A deterministic run (`ManualClock`, 600 frames) with a Fixed‑phase ECS system; it checks its own result and exits 1 on failure. Smoke test: 600 frames |

Files are `*.in` with placeholders, substituted by the script:

| Placeholder | Example for `space_shooter` |
|-------------|-----------------------------|
| `@GAME_NAME@` | `space_shooter` (CMake target, log category) |
| `@GAME_CLASS@` | `SpaceShooter` (class and `.cpp` name) |
| `@GAME_TITLE@` | `Space Shooter` (window title) |

The `.in` suffix keeps the templates out of clang-format and clang-tidy, since placeholders aren't
valid C++. Generated code is formatted with clang-format when it's installed. CI generates both
templates on every PR, then builds and runs them, so they can't rot.
