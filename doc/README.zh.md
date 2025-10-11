# zpp（中文）

zpp(ZetaX plus plus) 是一个 C++ 库。
zpp 致力于提高开发效率，集成了 cmake_util、spdlog 日志库以及若干第三方库。

## cbuild 使用说明

仓库使用 `cbuild` 作为标准构建入口（脚本位于仓库根目录）。常用示例：

- `./cbuild r r v`：第一个 `r` 表示 Release（`d` 表示 Debug）；第二个 `r` 表示删除现有 `build` 或 `build_debug` 目录并重新构建；第三个 `v` 表示传递给 CMake 的 `VERBOSE=1`（显示详细编译命令）。

- `./cbuild run <exe>`：运行由 `CMakeLists.txt` 中 `add_run_target()` 定义的可执行目标 `<exe>`。例如：`./cbuild run my_example` 会执行 `my_example` 的运行目标（前提是该目标存在且已构建）。

更多使用说明和脚本内部参数请查看 `cbuild` 顶部注释或联系维护者。

## cbuild 使用说明

仓库使用 `cbuild` 作为标准构建入口（脚本位于仓库根目录）。常用示例：

- `./cbuild r r v`：第一个 `r` 表示 Release（`d` 表示 Debug）；第二个 `r` 表示删除现有 `build` 或 `build_debug` 目录并重新构建；第三个 `v` 表示传递给 CMake 的 `VERBOSE=1`（显示详细编译命令）。

- `./cbuild run <exe>`：运行由 `CMakeLists.txt` 中 `add_run_target()` 定义的可执行目标 `<exe>`。例如：`./cbuild run my_example` 会执行 `my_example` 的运行目标（前提是该目标存在且已构建）。

更多使用说明和脚本内部参数请查看 `cbuild` 顶部注释或联系维护者。

