# Sample Consumer

Sample game demonstrating how to consume `miniGameEngine` as a reusable dependency:
1. via `find_package(mge 0.9 REQUIRED)` (after installing the engine with `-DMGE_INSTALL=ON`).
2. via `FetchContent` (`-DMGE_CONSUMER_FETCH=ON`).

## Building with `find_package(mge)`

Ensure the engine was installed with `MGE_INSTALL=ON`:
```bash
cmake -S /path/to/miniGameEngine -B /path/to/miniGameEngine/build-install \
  -DCMAKE_INSTALL_PREFIX=/path/to/mge-prefix -DMGE_INSTALL=ON
cmake --build /path/to/miniGameEngine/build-install --target install
```

Then configure and build the sample:
```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/mge-prefix
cmake --build build
```

## Building with `FetchContent`

```bash
cmake -S . -B build -DMGE_CONSUMER_FETCH=ON
cmake --build build
```

> [!NOTE]
> `FetchContent_Declare` defaults to `GIT_TAG v0.9.0`. The release git tag `v0.9.0` must be created on the remote repository for this to fetch automatically. During local development or prior to the tag creation, point `FETCHCONTENT_SOURCE_DIR_MGE` to your local clone:
> ```bash
> cmake -S . -B build -DMGE_CONSUMER_FETCH=ON -DFETCHCONTENT_SOURCE_DIR_MGE=/path/to/miniGameEngine
> ```
