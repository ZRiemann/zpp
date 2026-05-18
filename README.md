# zpp

zpp (ZetaX plus plus) is a C++ library.
zpp aims to improve development efficiency and integrates the spdlog logging library and several other third-party libraries.

This repository is built as an independent ZetaX project. It owns only zpp-specific build logic under `builder/`, while shared Conan dependency versions and package configs are provided by `zeta_forge deps`.

See the Chinese translation in the `doc` directory: [中文 README](doc/README.zh.md).
Release history: [release_history.org](release_history.org).

## Build

From this `zpp` checkout:

```bash
cd "$ZETAX_ROOT/zeta_forge"
./zbuild.py deps --BUILD_TYPE=Debug --install

cd ~/git/zpp
./zbuild.py --BUILD_TYPE=Debug --rebuild
```

The entrypoint locates the shared build framework with `$ZETAX_ROOT/zeta_forge`, then nearby workspace paths. `ZETAX_ROOT` is the top-level ZetaX workspace root; with the standard layout, `zpp` and `zeta_forge` are sibling checkouts under it.

zpp-specific build logic lives in `builder/zpp.py`; the common build lifecycle still runs through `zeta_forge.cmake_builder.CMakeProjectBuilder`.
The legacy `cmake_util` submodule has been removed; zpp now consumes `zeta_forge/cmake_util` through the build entrypoint.
zpp does not run Conan. If `spdlog`, `fmt`, or `GTest` package configs are missing, install the shared environment with `zeta_forge/zbuild.py deps --install`.
zpp no longer performs fallback auto-discovery for `ZETA_CMAKE_UTIL_DIR` or `ZETA_DEPS_CMAKE_DIR` inside CMake. Prepare the zeta_forge environment first, then build zpp through `./zbuild.py`.
Current project version is recorded in `VERSION`. This file is the canonical
version source; CMake reads it and derives `project(zpp VERSION ...)` from it.

### Build Modules

By default, zpp builds only the `core` module. Optional modules are available via builder flags:

- `--with-folly`: Enable folly module and related examples/tests.
- `--with-nng`: Enable nng (nanomsg) module and examples.
- `--with-taskflow`: Enable taskflow examples.

Example builds all optional modules:

```bash
./zbuild.py --rebuild --with-folly --with-nng --with-taskflow
```

### Common Options

- `./zbuild.py --rebuild --no-tests` builds without zpp tests.
- `./zbuild.py --rebuild --no-examples` builds without zpp examples.
- `./zbuild.py --rebuild --install` builds and installs zpp into the configured zeta_forge install prefix.

### Run Entries

zpp can list and execute the runnable entries declared with `add_run_target(...)`
in its `CMakeLists.txt` files:

```bash
./zbuild.py runs
./zbuild.py run zpp_core
./zbuild.py run --BUILD_TYPE=Debug zpp_core
```

`runs` prints each run entry and the equivalent CMake command. `run` directly
executes the already built binary with its configured arguments for the selected
build type; it does not configure the project or call `cmake --build`.

The old `cbuild` helper is no longer the recommended entry for zpp. It runs a standalone CMake configure and may miss the installed zeta_forge dependency package environment.
Direct CMake usage is for advanced/manual scenarios only and must pass explicit dependency paths such as `-DZETA_CMAKE_UTIL_DIR=...` and `-DZETA_DEPS_CMAKE_DIR=...`.
