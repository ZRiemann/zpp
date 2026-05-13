# zpp

zpp (ZetaX plus plus) is a C++ library.
zpp aims to improve development efficiency and integrates the spdlog logging library and several other third-party libraries.

This repository is built as an independent ZetaX project. It owns its zpp-specific builder and Conan recipe under `builder/`, while reusing the shared Conan/CMake build framework and CMake helper modules from `zeta_forge`.

See the Chinese translation in the `doc` directory: [中文 README](doc/README.zh.md).
Release history: [release_history.org](release_history.org).

## Build

From this `zpp` checkout:

```bash
cd ~/git/zpp
./zbuild.py --rebuild
```

The entrypoint locates the shared build framework with `$ZETAX_ROOT/zeta_forge`, then nearby workspace paths. `ZETAX_ROOT` is the top-level ZetaX workspace root; with the standard layout, `zpp` and `zeta_forge` are sibling checkouts under it.

zpp-specific build logic lives in `builder/zpp.py`; the common build lifecycle still runs through `zeta_forge.cmake_builder.CMakeProjectBuilder`.
The legacy `cmake_util` submodule has been removed; zpp now consumes `zeta_forge/cmake_util` through the build entrypoint.
Current project version is recorded in `VERSION`.

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

The old `cbuild` helper is no longer the recommended entry for zpp. It runs a standalone CMake configure and may miss dependency paths that are provided by zeta_forge, such as the Conan-generated `fmt`, `GTest`, and `spdlog` package files.
