# zpp（中文）

zpp(ZetaX plus plus) 是一个 C++ 库。
zpp 致力于提高开发效率，集成了 spdlog 日志库以及若干第三方库。

## 使用 zeta_forge 构建

zpp 作为独立 ZetaX 项目构建。它在自己的 `builder/` 目录下维护 zpp 特有 Builder 和 Conan 配方，同时复用 `zeta_forge` 提供的 Conan/CMake 公共编译框架和 CMake helper 模块。
当前项目版本记录在仓库根目录 `VERSION`，发布历史见 `release_history.org`。

在当前 `zpp` checkout 内执行：

```bash
cd ~/git/zpp
./zbuild.py --rebuild
```

入口会依次通过 `$ZETAX_ROOT/zeta_forge`、附近工作区路径定位共享编译框架。
其中 `ZETAX_ROOT` 是 ZetaX 顶层工作区根目录；标准布局下，`zpp` 与 `zeta_forge` 是它下面的兄弟目录。

zpp 特有构建逻辑位于 `builder/zpp.py`；公共构建生命周期仍由 `zeta_forge.cmake_builder.CMakeProjectBuilder` 执行。
遗留的 `cmake_util` submodule 已移除；zpp 现在通过构建入口接入 `zeta_forge/cmake_util`。

### 可选模块

zpp 默认只构建 core 模块。可选模块通过 builder 参数启用：

- `--with-folly`：启用 folly 模块及相关示例/测试。
- `--with-nng`：启用 nng (nanomsg) 模块及示例。
- `--with-taskflow`：启用 taskflow 示例。

示例：同时构建所有可选模块：

```bash
./zbuild.py --rebuild --with-folly --with-nng --with-taskflow
```

### 常用选项

- `./zbuild.py --rebuild --no-tests`：不构建 zpp 测试。
- `./zbuild.py --rebuild --no-examples`：不构建 zpp 示例。
- `./zbuild.py --rebuild --install`：构建并安装到 zeta_forge 配置的安装前缀。

### 运行入口

zpp 可以列出并执行各个 `CMakeLists.txt` 中通过 `add_run_target(...)` 预置的运行入口：

```bash
./zbuild.py runs
./zbuild.py run zpp_core
./zbuild.py run --BUILD_TYPE=Debug zpp_core
```

`runs` 会输出运行入口名称和对应的 CMake 命令。`run` 会直接执行所选构建类型下已经编译好的二进制文件及其预置参数，不自动 configure，也不调用 `cmake --build`。

旧的 `cbuild` helper 不再作为 zpp 推荐入口。它会执行独立 CMake 配置，可能拿不到 zeta_forge/Conan 提供的 `fmt`、`GTest`、`spdlog` 等依赖包路径。
