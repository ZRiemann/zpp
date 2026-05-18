from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

from zeta_forge.cmake_builder import CMakeProjectBuilder, CommonBuildArgs, cmake_bool, common_build_argument_parser
from zeta_forge.config import load_repo_config
from zeta_forge.run_targets import (
    build_type_parser,
    discover_run_targets,
    find_run_target,
    print_run_targets,
    run_existing_target,
)


@dataclass(frozen=True)
class ZppBuildArgs(CommonBuildArgs):
    build_tests: bool
    build_examples: bool
    build_hpx_examples: bool
    build_folly_module: bool
    build_nng_module: bool
    build_taskflow_module: bool


class ZppBuilder(CMakeProjectBuilder):
    source_watch_patterns = ("CMakeLists.txt", "*.cmake", "*.cmake.in", "CMakeConfig.h.in", "VERSION")
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
        return "Set ZETA_ZPP_SRC_DIR to a local checkout or run from the zpp checkout with ./zbuild.py"

    @property
    def zeta_deps_cmake_dir(self) -> Path:
        return self.repo_config.install_prefix / "lib" / "cmake" / "zeta_deps" / self.args.build_type

    @property
    def folly_cmake_dir(self) -> Path:
        return self.repo_config.install_prefix / "lib" / "cmake" / "folly"

    @property
    def hpx_cmake_dir(self) -> Path:
        return self.repo_config.install_prefix / "lib" / "cmake" / "HPX"

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
            / "asio-release-x86_64-data.cmake"
        )

    @property
    def hpx_asio_root(self) -> Path:
        if not self.hpx_asio_data_file.is_file():
            raise RuntimeError(
                f"HPX Asio Conan metadata not found: {self.hpx_asio_data_file}\n"
                "Rebuild HPX dependency metadata first, for example: "
                "$ZETAX_ROOT/zeta_forge/zbuild.py hpx --rebuild --install"
            )

        package_folder_prefix = 'set(asio_PACKAGE_FOLDER_RELEASE "'
        for raw_line in self.hpx_asio_data_file.read_text(encoding="utf-8", errors="ignore").splitlines():
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
            f"Unable to parse asio_PACKAGE_FOLDER_RELEASE from: {self.hpx_asio_data_file}\n"
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
                "$ZETAX_ROOT/zeta_forge/zbuild.py deps --BUILD_TYPE="
                f"{self.args.build_type} --install"
            )
        if self.typed_args.build_taskflow_module and not self.taskflow_source_dir.is_dir():
            raise RuntimeError(
                f"Taskflow source directory not found: {self.taskflow_source_dir}\n"
                "Set ZETA_TASKFLOW_SRC_DIR to a local checkout or initialize the zeta_forge submodule with: git -C \"$ZETAX_ROOT/zeta_forge\" submodule update --init --recursive 3rd/taskflow"
            )
        if self.typed_args.build_hpx_examples and not self.typed_args.build_examples:
            raise RuntimeError("--with-hpx-examples requires examples to be enabled")
        if self.typed_args.build_folly_module and not (self.folly_cmake_dir / "folly-config.cmake").is_file():
            raise RuntimeError(
                f"folly package config not found: {self.folly_cmake_dir}\n"
                "Build/install Folly through zeta_forge first, for example: "
                "$ZETAX_ROOT/zeta_forge/zbuild.py folly --rebuild --install"
            )
        if self.typed_args.build_hpx_examples and not self.hpx_config_file.is_file():
            raise RuntimeError(
                f"HPX package config not found: {self.hpx_config_file}\n"
                "Build/install HPX through zeta_forge first, for example: "
                "git -C \"$ZETAX_ROOT/zeta_forge\" submodule update --init --recursive 3rd/hpx && "
                "$ZETAX_ROOT/zeta_forge/zbuild.py hpx --rebuild --install"
            )
        if self.typed_args.build_hpx_examples and not self.hpx_asio_config_file.is_file():
            raise RuntimeError(
                f"HPX Asio compatibility package config not found: {self.hpx_asio_config_file}\n"
                "Build/install HPX through zeta_forge first, for example: "
                "$ZETAX_ROOT/zeta_forge/zbuild.py hpx --rebuild --install"
            )
        if self.typed_args.build_hpx_examples and not self.hpx_hwloc_config_file.is_file():
            raise RuntimeError(
                f"HPX Hwloc compatibility package config not found: {self.hpx_hwloc_config_file}\n"
                "Build/install HPX through zeta_forge first, for example: "
                "$ZETAX_ROOT/zeta_forge/zbuild.py hpx --rebuild --install"
            )
        if self.typed_args.build_hpx_examples:
            _ = self.hpx_asio_root

    def configure_dependencies(self) -> list[Path]:
        return [
            self.script_path,
            Path(__file__),
            self.source_dir / "VERSION",
            self.source_dir / "CMakeConfig.h.in",
        ]

    def configure_command(self) -> list[object]:
        cmake_prefix_paths = [str(self.zeta_deps_cmake_dir), str(self.repo_config.install_prefix)]
        if self.typed_args.build_folly_module:
            cmake_prefix_paths.append(str(self.folly_cmake_dir))
        if self.typed_args.build_hpx_examples:
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
            f"-DCMAKE_PREFIX_PATH={';'.join(cmake_prefix_paths)}",
            f"-DCMAKE_INSTALL_PREFIX={self.repo_config.install_prefix}",
            f"-DCMAKE_CXX_STANDARD={self.repo_config.cxx_standard}",
            f"-DZETA_CMAKE_UTIL_DIR={self.cmake_util_dir}",
            f"-DZETA_DEPS_CMAKE_DIR={self.zeta_deps_cmake_dir}",
            f"-DZPP_BUILD_FOLLY_MODULE={cmake_bool(self.typed_args.build_folly_module)}",
            f"-DZPP_BUILD_NNG_MODULE={cmake_bool(self.typed_args.build_nng_module)}",
            f"-DZPP_BUILD_TASKFLOW_MODULE={cmake_bool(self.typed_args.build_taskflow_module)}",
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
        if self.typed_args.build_hpx_examples:
            command.extend(
                [
                    "-DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON",
                    f"-DHPX_DIR={self.hpx_cmake_dir}",
                    f"-DAsio_DIR={self.hpx_conan_compat_packages_dir}",
                    f"-DHwloc_DIR={self.hpx_conan_compat_packages_dir}",
                    f"-DASIO_ROOT={self.hpx_asio_root}",
                ]
            )
        return command


def build_parser() -> argparse.ArgumentParser:
    parser = common_build_argument_parser("Build zpp")
    parser.add_argument("--no-tests", action="store_true")
    parser.add_argument("--no-examples", action="store_true")
    parser.add_argument("--with-hpx-examples", action="store_true")
    parser.add_argument("--with-folly", action="store_true")
    parser.add_argument("--with-nng", action="store_true")
    parser.add_argument("--with-taskflow", action="store_true")
    parser.epilog = (
        "Run entry commands:\n"
        "  ./zbuild.py runs\n"
        "  ./zbuild.py run zpp_core\n"
        "  ./zbuild.py run --BUILD_TYPE=Debug zpp_core"
    )
    parser.formatter_class = argparse.RawDescriptionHelpFormatter
    return parser


def parse_args(argv: list[str] | None = None) -> ZppBuildArgs:
    namespace = build_parser().parse_args(argv)
    return ZppBuildArgs(
        build_type=namespace.build_type,
        install=namespace.install,
        rebuild=namespace.rebuild,
        build_tests=not namespace.no_tests,
        build_examples=not namespace.no_examples,
        build_hpx_examples=namespace.with_hpx_examples,
        build_folly_module=namespace.with_folly,
        build_nng_module=namespace.with_nng,
        build_taskflow_module=namespace.with_taskflow,
    )


def _project_source_defaults(source_dir_default: Path | None) -> dict[str, Path]:
    project_source_defaults = {}
    if source_dir_default is not None:
        project_source_defaults["ZETA_ZPP_SRC_DIR"] = source_dir_default
    return project_source_defaults


def _build_dir(script_path: Path, build_type: str) -> Path:
    return script_path.resolve().parent / "build" / build_type


def run_entries(script_path: Path, argv: list[str], *, source_dir_default: Path | None = None) -> int:
    parser = build_type_parser("./zbuild.py runs", "List zpp add_run_target entries")
    namespace = parser.parse_args(argv)
    repo_config = load_repo_config(script_path, project_source_defaults=_project_source_defaults(source_dir_default))
    source_dir = repo_config.source_dir("ZETA_ZPP_SRC_DIR")
    build_dir = _build_dir(script_path, namespace.build_type)
    print_run_targets(discover_run_targets(source_dir, build_dir))
    return 0


def run_entry(script_path: Path, argv: list[str], *, source_dir_default: Path | None = None) -> int:
    parser = build_type_parser("./zbuild.py run", "Run a zpp add_run_target entry from an existing build tree")
    parser.add_argument("name", help="Run entry name, for example zpp_core")
    namespace = parser.parse_args(argv)
    repo_config = load_repo_config(script_path, project_source_defaults=_project_source_defaults(source_dir_default))
    source_dir = repo_config.source_dir("ZETA_ZPP_SRC_DIR")
    build_dir = _build_dir(script_path, namespace.build_type)
    target = find_run_target(discover_run_targets(source_dir, build_dir), namespace.name)
    return run_existing_target(target)


def main(script_path: Path, *, argv: list[str] | None = None, source_dir_default: Path | None = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    if argv and argv[0] == "runs":
        return run_entries(script_path, argv[1:], source_dir_default=source_dir_default)
    if argv and argv[0] == "run":
        return run_entry(script_path, argv[1:], source_dir_default=source_dir_default)

    args = parse_args(argv)
    repo_config = load_repo_config(script_path, project_source_defaults=_project_source_defaults(source_dir_default))
    ZppBuilder(script_path=script_path, repo_config=repo_config, args=args).run()
    return 0


def cli(script_path: Path, *, source_dir_default: Path | None = None) -> int:
    try:
        return main(script_path, source_dir_default=source_dir_default)
    except Exception as exc:
        print(exc, file=sys.stderr)
        return 1
