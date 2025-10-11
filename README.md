# zpp

zpp (ZetaX plus plus) is a C++ library.
zpp aims to improve development efficiency and integrates cmake_util, the spdlog logging library, and several other third-party libraries.

This repository uses `cbuild` as the canonical build entry. See the Chinese translation in the `doc` directory: [中文 README](doc/README.zh.md).

## cbuild usage

Use the `cbuild` script at the repository root to configure and build zpp. Examples:

- `./cbuild r r v` — first `r` selects Release (`d` for Debug); second `r` removes existing `build` or `build_debug` and rebuilds; third `v` sets CMake `VERBOSE=1` to show detailed compiler/linker commands.

- `./cbuild run <exe>` — run an executable target `<exe>` defined via `add_run_target()` in the corresponding `CMakeLists.txt`. Example: `./cbuild run my_example` runs the `my_example` run target (provided it exists and has been built).

For more options and advanced usage, see the header comment at the top of the `cbuild` script or contact the project maintainer.

