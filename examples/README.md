# examples/

Small programs built on the engine. Each folder is one executable.

| Folder | Target | What it shows |
|--------|--------|---------------|
| `headless_smoke/` | `headless_smoke` | An `mge::Application` with **no modules**: no window, no GPU, SDL never initialised. It runs 1000 frames, generating a sandbox chunk per frame and checking terrain invariants, and exits non‑zero on failure (`setExitCode`). Registered in CTest as `smoke: headless 1000 frames`, and run by CI under ASan/UBSan |

Examples are built when `MGE_BUILD_MODULES=ON`, because `headless_smoke` reuses `sandbox_world` as a
realistic workload.
