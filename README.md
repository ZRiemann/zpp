# zpp

zpp (ZetaX plus plus) is a C++ library.
zpp aims to improve development efficiency and integrates cmake_util, the spdlog logging library, and several other third-party libraries.

This repository is managed as part of the larger `zeta_forge` workspace. The recommended build entry is `zeta_forge/zbuild.py zpp`, which provides the shared Conan/CMake toolchain and keeps zpp's third-party dependency versions aligned with the rest of the workspace.

See the Chinese translation in the `doc` directory: [中文 README](doc/README.zh.md).

## Build With zeta_forge

Run builds from the `zeta_forge` repository root:

```bash
cd ~/git/zeta_forge
./zbuild.py zpp --rebuild
```

Common options:

- `./zbuild.py zpp --rebuild --no-tests` builds without zpp tests.
- `./zbuild.py zpp --rebuild --no-examples` builds without zpp examples.
- `./zbuild.py zpp --rebuild --install` builds and installs zpp into the configured zeta_forge install prefix.

The old `cbuild` helper is no longer the recommended entry for zpp. It runs a standalone CMake configure and may miss dependency paths that are provided by zeta_forge, such as the Conan-generated `fmt`, `GTest`, and `spdlog` package files.
