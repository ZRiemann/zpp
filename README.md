# zpp

zpp (ZetaX plus plus) is a C++ library.
zpp aims to improve development efficiency and integrates cmake_util, the spdlog logging library, and several other third-party libraries.

This repository can be built inside the larger `zeta_forge` workspace or as a standalone checkout. In both cases it reuses the shared Conan/CMake build library from `zeta_forge`.

See the Chinese translation in the `doc` directory: [中文 README](doc/README.zh.md).

## Build

From the `zeta_forge` repository root:

```bash
cd ~/git/zeta_forge
./zbuild.py zpp --rebuild
```

From this `zpp` checkout, including a standalone clone:

```bash
./zbuild.py --rebuild
```

The standalone entrypoint locates the shared build library with `ZETA_FORGE_ROOT`, then `$ZETAX_ROOT/zeta_forge`, then nearby workspace paths. `ZETAX_ROOT` is the top-level ZetaX workspace root, while `ZETA_FORGE_ROOT` points directly at the `zeta_forge` repository.

Common options:

- `./zbuild.py zpp --rebuild --no-tests` builds without zpp tests.
- `./zbuild.py zpp --rebuild --no-examples` builds without zpp examples.
- `./zbuild.py zpp --rebuild --install` builds and installs zpp into the configured zeta_forge install prefix.

The old `cbuild` helper is no longer the recommended entry for zpp. It runs a standalone CMake configure and may miss dependency paths that are provided by zeta_forge, such as the Conan-generated `fmt`, `GTest`, and `spdlog` package files.
