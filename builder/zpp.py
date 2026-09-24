from __future__ import annotations

import subprocess
from dataclasses import dataclass
from pathlib import Path

from zeta_forge.build_cli import Product, Project, Request
from zeta_forge.cmake_builder import CMakeProjectBuilder, CommonBuildArgs, cmake_bool
from zeta_forge.cmake_engine import CMAKE_ACTIONS, CMakeEngine
from zeta_forge.config import load_repo_config
from zeta_forge.run_targets import discover_run_targets


@dataclass(frozen=True)
class ZppBuildArgs(CommonBuildArgs):
    build_tests: bool
    build_examples: bool
    build_hpx_examples: bool
    build_hpx_module: bool
    build_folly_module: bool
    build_nng_module: bool
    build_taskflow_module: bool


class ZppBuilder(CMakeProjectBuilder):
    source_watch_patterns = (
        "CMakeLists.txt",
        "*.cmake",
        "*.cmake.in",
        "CMakeConfig.h.in",
        "VERSION",
    )
    source_prune_dirs = ("build", "build_debug")
    uses_conan = False

    @property
    def project_name(self) -> str:
        return "zpp"

    @property
    def typed_args(self) -> ZppBuildArgs:
        return self.args  # type: ignore[return-value]

    @property
    def source_dir(self) -> Path:
        return self.repo_config.source_dir("ZETA_ZPP_SRC_DIR")

    @property
    def taskflow_source_dir(self) -> Path:
        return self.repo_config.source_dir("ZETA_TASKFLOW_SRC_DIR")

    @property
    def cmake_util_dir(self) -> Path:
        return self.repo_config.forge_root / "cmake_util"

    @property
    def missing_source_hint(self) -> str:
        return (
            "Set ZETA_ZPP_SRC_DIR to a local checkout or run from the zpp checkout with ./zbuild.py"
        )

    @property
    def zeta_deps_cmake_dir(self) -> Path:
        return (
            self.repo_config.install_prefix / "lib" / "cmake" / "zeta_deps" / self.args.build_type
        )

    @property
    def folly_cmake_dir(self) -> Path:
        return self.repo_config.install_prefix / "lib" / "cmake" / "folly"

    @property
    def hpx_cmake_dir(self) -> Path:
        return self.repo_config.install_prefix / "lib" / "cmake" / "HPX"

    @property
    def hpx_source_dir(self) -> Path:
        return self.repo_config.source_dir("ZETA_HPX_SRC_DIR")

    @property
    def hpx_git_commit(self) -> str:
        try:
            result = subprocess.run(
                ["git", "-C", str(self.hpx_source_dir), "rev-parse", "HEAD"],
                check=True,
                capture_output=True,
                text=True,
            )
        except (OSError, subprocess.CalledProcessError) as exc:
            raise RuntimeError(
                f"Unable to read the zeta_forge HPX source commit from {self.hpx_source_dir}"
            ) from exc
        return result.stdout.strip()

    def configured_hpx_git_commit(self) -> str | None:
        cache_path = self.build_dir / "CMakeCache.txt"
        if not cache_path.is_file():
            return None
        cache_lines = cache_path.read_text(encoding="utf-8", errors="ignore").splitlines()
        for raw_line in cache_lines:
            if raw_line.startswith("ZPP_HPX_EXPECTED_GIT_COMMIT:"):
                return raw_line.split("=", 1)[1].strip()
        return None

    @property
    def hpx_config_file(self) -> Path:
        return self.hpx_cmake_dir / "HPXConfig.cmake"

    @property
    def hpx_conan_compat_packages_dir(self) -> Path:
        return self.repo_config.builder_dir / "hpx" / "cmake" / "conan_compat" / "packages"

    @property
    def hpx_asio_config_file(self) -> Path:
        return self.hpx_conan_compat_packages_dir / "AsioConfig.cmake"

    @property
    def hpx_hwloc_config_file(self) -> Path:
        return self.hpx_conan_compat_packages_dir / "HwlocConfig.cmake"

    @property
    def hpx_asio_data_file(self) -> Path:
        return (
            self.repo_config.builder_dir
            / "hpx"
            / "build"
            / self.args.build_type
            / "conan"
            / "build"
            / self.args.build_type
            / "generators"
            / f"asio-{self.args.build_type.lower()}-x86_64-data.cmake"
        )

    @property
    def hpx_asio_root(self) -> Path:
        if not self.hpx_asio_data_file.is_file():
            raise RuntimeError(
                f"HPX Asio Conan metadata not found: {self.hpx_asio_data_file}\n"
                "Rebuild HPX dependency metadata first, for example: "
                "$ZETAX_ROOT/zeta_forge/zbuild.py install hpx"
            )

        package_folder_prefix = f'set(asio_PACKAGE_FOLDER_{self.args.build_type.upper()} "'
        for raw_line in self.hpx_asio_data_file.read_text(
            encoding="utf-8", errors="ignore"
        ).splitlines():
            line = raw_line.strip()
            if line.startswith(package_folder_prefix) and line.endswith('")'):
                package_root = Path(line[len(package_folder_prefix) : -2]).resolve()
                include_dir = package_root / "include"
                if (include_dir / "asio.hpp").is_file():
                    return package_root
                raise RuntimeError(
                    f"HPX Asio include directory is invalid: {include_dir}\n"
                    "Re-run zeta_forge HPX build/install to refresh Conan packages."
                )

        raise RuntimeError(
            f"Unable to parse profile-specific asio package folder from: {self.hpx_asio_data_file}\n"
            "Re-run zeta_forge HPX build/install to refresh Conan packages."
        )

    def validate(self) -> None:
        super().validate()
        if not (self.cmake_util_dir / "common.cmake").is_file():
            raise RuntimeError(
                f"zeta_forge cmake_util not found: {self.cmake_util_dir}\n"
                "Ensure $ZETAX_ROOT/zeta_forge/cmake_util exists before building zpp."
            )
        if not self.zeta_deps_cmake_dir.is_dir():
            raise RuntimeError(
                f"ZetaX dependency package configs not found: {self.zeta_deps_cmake_dir}\n"
                "Install the shared dependency environment first with: "
                "$ZETAX_ROOT/zeta_forge/zbuild.py install deps --profile "
                f"{self.args.build_type.lower()}"
            )
        if self.typed_args.build_taskflow_module and not self.taskflow_source_dir.is_dir():
            raise RuntimeError(
                f"Taskflow source directory not found: {self.taskflow_source_dir}\n"
                'Set ZETA_TASKFLOW_SRC_DIR to a local checkout or initialize the zeta_forge submodule with: git -C "$ZETAX_ROOT/zeta_forge" submodule update --init --recursive 3rd/taskflow'
            )
        if self.typed_args.build_hpx_examples and not self.typed_args.build_examples:
            raise RuntimeError("HPX example selection requires native examples to be enabled")
        if (
            self.typed_args.build_folly_module
            and not (self.folly_cmake_dir / "folly-config.cmake").is_file()
        ):
            raise RuntimeError(
                f"folly package config not found: {self.folly_cmake_dir}\n"
                "Build/install Folly through zeta_forge first, for example: "
                "$ZETAX_ROOT/zeta_forge/zbuild.py install folly"
            )
        if self.typed_args.build_hpx_module and not self.hpx_source_dir.is_dir():
            raise RuntimeError(
                f"zeta_forge HPX source directory not found: {self.hpx_source_dir}\n"
                'Initialize it with: git -C "$ZETAX_ROOT/zeta_forge" '
                "submodule update --init --recursive 3rd/hpx"
            )
        if self.typed_args.build_hpx_module and not self.hpx_config_file.is_file():
            raise RuntimeError(
                f"HPX package config not found: {self.hpx_config_file}\n"
                "Build/install HPX through zeta_forge first, for example: "
                'git -C "$ZETAX_ROOT/zeta_forge" submodule update --init --recursive 3rd/hpx && '
                "$ZETAX_ROOT/zeta_forge/zbuild.py install hpx"
            )
        if self.typed_args.build_hpx_module and not self.hpx_asio_config_file.is_file():
            raise RuntimeError(
                f"HPX Asio compatibility package config not found: {self.hpx_asio_config_file}\n"
                "Build/install HPX through zeta_forge first, for example: "
                "$ZETAX_ROOT/zeta_forge/zbuild.py install hpx"
            )
        if self.typed_args.build_hpx_module and not self.hpx_hwloc_config_file.is_file():
            raise RuntimeError(
                f"HPX Hwloc compatibility package config not found: {self.hpx_hwloc_config_file}\n"
                "Build/install HPX through zeta_forge first, for example: "
                "$ZETAX_ROOT/zeta_forge/zbuild.py install hpx"
            )
        if self.typed_args.build_hpx_module:
            _ = self.hpx_asio_root
            _ = self.hpx_git_commit

    def configure_dependencies(self) -> list[Path]:
        dependencies = [
            self.script_path,
            Path(__file__),
            self.source_dir / "VERSION",
            self.source_dir / "CMakeConfig.h.in",
        ]
        if self.typed_args.build_hpx_module:
            dependencies.extend(
                [
                    self.hpx_config_file,
                    self.hpx_asio_config_file,
                    self.hpx_hwloc_config_file,
                ]
            )
        return dependencies

    def should_configure(self) -> bool:
        if (
            self.typed_args.build_hpx_module
            and self.configured_hpx_git_commit() != self.hpx_git_commit
        ):
            return True
        return super().should_configure()

    def configure_command(self) -> list[object]:
        cmake_prefix_paths = [str(self.zeta_deps_cmake_dir), str(self.repo_config.install_prefix)]
        if self.typed_args.build_folly_module:
            cmake_prefix_paths.append(str(self.folly_cmake_dir))
        if self.typed_args.build_hpx_module:
            cmake_prefix_paths.append(str(self.hpx_cmake_dir))
            cmake_prefix_paths.append(str(self.hpx_conan_compat_packages_dir))

        command: list[object] = [
            "cmake",
            "-U",
            "CMAKE_MAP_IMPORTED_CONFIG_DEBUG",
            "-S",
            self.source_dir,
            "-B",
            self.build_dir,
            "-G",
            "Ninja",
            "-Wno-dev",
            f"-DCMAKE_BUILD_TYPE={self.args.build_type}",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            f"-DCMAKE_PREFIX_PATH={';'.join(cmake_prefix_paths)}",
            f"-DCMAKE_INSTALL_PREFIX={self.repo_config.install_prefix}",
            f"-DCMAKE_CXX_STANDARD={self.repo_config.cxx_standard}",
            f"-DZETA_CMAKE_UTIL_DIR={self.cmake_util_dir}",
            f"-DZETA_DEPS_CMAKE_DIR={self.zeta_deps_cmake_dir}",
            f"-DZPP_BUILD_FOLLY_MODULE={cmake_bool(self.typed_args.build_folly_module)}",
            f"-DZPP_BUILD_NNG_MODULE={cmake_bool(self.typed_args.build_nng_module)}",
            f"-DZPP_BUILD_TASKFLOW_MODULE={cmake_bool(self.typed_args.build_taskflow_module)}",
            f"-DZPP_BUILD_HPX_MODULE={cmake_bool(self.typed_args.build_hpx_module)}",
            f"-DZPP_BUILD_TESTS={cmake_bool(self.typed_args.build_tests)}",
            f"-DZPP_BUILD_EXAMPLES={cmake_bool(self.typed_args.build_examples)}",
            f"-DZPP_BUILD_HPX_EXAMPLES={cmake_bool(self.typed_args.build_hpx_examples)}",
        ]
        if self.typed_args.build_folly_module:
            command.extend(
                [
                    f"-Dfolly_DIR={self.folly_cmake_dir}",
                ]
            )
        if self.typed_args.build_taskflow_module:
            command.append(f"-DTASKFLOW_ROOT={self.taskflow_source_dir}")
        if self.typed_args.build_hpx_module:
            command.extend(
                [
                    "-DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON",
                    f"-DHPX_DIR={self.hpx_cmake_dir}",
                    f"-DAsio_DIR={self.hpx_conan_compat_packages_dir}",
                    f"-DHwloc_DIR={self.hpx_conan_compat_packages_dir}",
                    f"-DASIO_ROOT={self.hpx_asio_root}",
                    f"-DZPP_HPX_DEPENDENCY_CMAKE_DIR={self.hpx_conan_compat_packages_dir}",
                    f"-DZPP_HPX_EXPECTED_GIT_COMMIT={self.hpx_git_commit}",
                ]
            )
        return command


def project(script_path: Path) -> Project:
    root = script_path.resolve().parent
    config = load_repo_config(script_path, project_source_defaults={"ZETA_ZPP_SRC_DIR": root})
    examples = {
        entry.name: entry.cmake_file.relative_to(root).parts[1]
        for entry in discover_run_targets(root, root / "build" / "Release")
        if entry.cmake_file.relative_to(root).parts[0] == "examples" and "$" not in entry.name
    }
    libraries = ("zpp", "zpp_nng", "zpp_folly", "zpp_hpx")
    names = (*libraries, *examples)

    def enabled(selected: tuple[str, ...]) -> set[str]:
        return {name.removeprefix("zpp_") for name in selected if name in libraries[1:]} | {
            examples[name] for name in selected if name in examples
        }

    def factory(request: Request, selected: tuple[str, ...]) -> ZppBuilder:
        modules = enabled(selected)
        args = ZppBuildArgs(
            request.cmake_profile,
            build_tests=request.action == "test",
            build_examples=any(name in examples for name in selected),
            build_hpx_examples=any(examples.get(name) == "hpx" for name in selected),
            build_hpx_module="hpx" in modules,
            build_folly_module="folly" in modules,
            build_nng_module="nng" in modules,
            build_taskflow_module="taskflow" in modules or "example_hpx_taskflow" in selected,
        )
        return ZppBuilder(script_path=script_path, repo_config=config, args=args)

    def tests(selected: tuple[str, ...]) -> tuple[str, ...]:
        modules = enabled(selected)
        return (
            "gtest_tsc",
            *(("gtest_nng",) if "nng" in modules else ()),
            *(("gtest_folly", "gtest_coro") if "folly" in modules else ()),
            *(("gtest_hpx_exec", "gtest_hpx_exec_runtime") if "hpx" in modules else ()),
        )

    engine = CMakeEngine(
        root,
        factory,
        names,
        test_targets=tests,
        native_targets=lambda selected: tuple(
            dict.fromkeys("zpp" if n == "zpp_hpx" else n for n in selected)
        ),
        components={name: tuple(dict.fromkeys(("zpp", name))) for name in libraries},
    )
    products = tuple(
        Product(
            name,
            "cmake",
            "Library" if name in libraries else "Example",
            (*CMAKE_ACTIONS, *(("install",) if name in libraries else ("run", "dev"))),
        )
        for name in names
    )
    return Project("zpp", products, {"cmake": engine}, ("zpp",))
