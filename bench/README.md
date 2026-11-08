# bench/

Microbenchmarks and performance regression tracking for core headless engine modules (`mge_core`, `mge_procgen`, `mge_voxel`, `mge_physics`).

Benchmarks do not depend on graphical drivers, windowing, or audio (`game/` and GPU modules are excluded).

## Executables

- **`mge_bench`**: Unified benchmark runner covering all engine subsystems.
- **`ecs_bench`**: Legacy standalone ECS component iteration benchmark.
- **`noise_bench`**: Legacy standalone noise generation benchmark.

## Building and Running Locally

Always run benchmarks in **Release** mode to enable compiler optimizations:

```bash
# Configure and build the release preset
cmake --preset release
cmake --build --preset release --target mge_bench

# Run all benchmarks
./build-release/bench/mge_bench

# Run with custom options
./build-release/bench/mge_bench --quick
./build-release/bench/mge_bench --filter voxel
./build-release/bench/mge_bench --json results.json
```

### Command Line Options

- `--quick`: Runs fewer iterations per benchmark. Ideal for quick smoke checks during development or CI sanity runs.
- `--filter <substr>`: Filters benchmarks by name substring (e.g., `--filter noise` or `--filter physics`).
- `--json <path>`: Writes the benchmark results in JSON format for automated comparison or baseline archival.
- `--help, -h`: Prints command-line usage information.

## Comparing Benchmark Results

Use `scripts/bench-compare.sh` to compare a current JSON run against a baseline:

```bash
# Generate baseline
./build-release/bench/mge_bench --json baseline.json

# ... make code changes ...

# Generate current
./build-release/bench/mge_bench --json current.json

# Compare
./scripts/bench-compare.sh current.json baseline.json
```

## Reading the Report

The benchmark output prints:
- **Median (ns)**: The median execution time per item across runs. Using the median filters out scheduling spikes and intermittent OS interruptions.
- **Min (ns)**: The minimum time observed across runs (the theoretical best without interference).
- **Iterations**: Number of measured items per timed run.
- **Unit**: Unit of the measurement (e.g. `ns/voxel`, `ns/call`, `ns/entidad`, `ns/cuerpo`).

In the comparison table:
- **Baseline (ns)**: Median time recorded in the baseline run.
- **Current (ns)**: Median time in the current run.
- **Delta %**: Relative difference `((current - baseline) / baseline) * 100`.
- **Status**:
  - `✅ OK`: Within normal tolerance.
  - `🚀 Faster`: Performance improved by more than 10%.
  - `⚠️ Regressed`: Execution time increased by more than 15%.

## CI Warning Rationale (> +15%)

The CI workflow executes `mge_bench` in Release mode and compares the results against the latest baseline generated from `main`.

- **Warn-only policy**: When a regression exceeds **+15%**, the check flags the benchmark with a `⚠️` warning in `$GITHUB_STEP_SUMMARY`, but the step still exits with code `0`.
- **Why warn instead of fail?**
  Shared virtual machines in cloud CI environments (such as GitHub Actions runners) run alongside other virtual machines ("noisy neighbors"). CPU core throttling, hyperthreading contention, and hypervisor scheduling introduce unavoidable execution time variance (typically 5–10%). A hard failure would cause non-deterministic, flaky CI builds. The +15% warning threshold alerts reviewers to potential regressions without blocking harmless pull requests.
