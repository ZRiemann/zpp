# zpp（中文）

zpp(ZetaX plus plus) 是一个 C++ 库。
zpp 致力于提高开发效率，集成了 cmake_util、spdlog 日志库以及若干第三方库。

## 使用 zeta_forge 构建

zpp 可以作为 `zeta_forge` 工作区的一部分构建，也可以作为单独 clone 的仓库构建。两种方式都会复用 `zeta_forge` 提供的 Conan/CMake 编译工具库，并协调 zpp 与其它第三方库的版本和依赖路径。

在 `zeta_forge` 仓库根目录执行：

```bash
cd ~/git/zeta_forge
./zbuild.py zpp --rebuild
```

在当前 `zpp` checkout 内，包括单独 clone 的 zpp 仓库，可以直接执行：

```bash
./zbuild.py --rebuild
```

独立入口会依次通过 `ZETA_FORGE_ROOT`、`$ZETAX_ROOT/zeta_forge`、附近工作区路径定位共享编译工具库。
其中 `ZETAX_ROOT` 是 ZetaX 顶层工作区根目录，`ZETA_FORGE_ROOT` 直接指向 `zeta_forge` 仓库。

常用选项：

- `./zbuild.py zpp --rebuild --no-tests`：不构建 zpp 测试。
- `./zbuild.py zpp --rebuild --no-examples`：不构建 zpp 示例。
- `./zbuild.py zpp --rebuild --install`：构建并安装到 zeta_forge 配置的安装前缀。

旧的 `cbuild` helper 不再作为 zpp 推荐入口。它会执行独立 CMake 配置，可能拿不到 zeta_forge/Conan 提供的 `fmt`、`GTest`、`spdlog` 等依赖包路径。
