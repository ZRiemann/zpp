# zpp（中文）

zpp(ZetaX plus plus) 是一个 C++ 库。
zpp 致力于提高开发效率，集成了 cmake_util、spdlog 日志库以及若干第三方库。

## 使用 zeta_forge 构建

zpp 现在作为 `zeta_forge` 工作区的一部分统一管理。推荐使用 `zeta_forge/zbuild.py zpp` 作为标准构建入口，由 zeta_forge 统一提供 Conan/CMake toolchain，并协调 zpp 与其它第三方库的版本和依赖路径。

在 `zeta_forge` 仓库根目录执行：

```bash
cd ~/git/zeta_forge
./zbuild.py zpp --rebuild
```

常用选项：

- `./zbuild.py zpp --rebuild --no-tests`：不构建 zpp 测试。
- `./zbuild.py zpp --rebuild --no-examples`：不构建 zpp 示例。
- `./zbuild.py zpp --rebuild --install`：构建并安装到 zeta_forge 配置的安装前缀。

旧的 `cbuild` helper 不再作为 zpp 推荐入口。它会执行独立 CMake 配置，可能拿不到 zeta_forge/Conan 提供的 `fmt`、`GTest`、`spdlog` 等依赖包路径。
