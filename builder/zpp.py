from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

from zeta_forge.cmake_builder import CMakeProjectBuilder, CommonBuildArgs, cmake_bool, common_build_argument_parser
from zeta_forge.config import load_repo_config


@dataclass(frozen=True)
class ZppBuildArgs(CommonBuildArgs):
    build_tests: bool
    build_examples: bool
    build_hpx_examples: bool
    build_folly_module: bool
    build_nng_module: bool
    build_taskflow_module: bool


class ZppBuilder(CMakeProjectBuilder):
    source_prune_dirs = ("build", "build_debug")

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
    def rapidjson_source_dir(self) -> Path:
        return self.repo_config.source_dir("ZETA_RAPIDJSON_SRC_DIR")

    @property
    def cmake_util_dir(self) -> Path:
        return self.repo_config.forge_root / "cmake_util"

    @property
    def conanfile(self) -> Path:
        return self.script_dir / "builder" / "conanfile.py"

    @property
    def missing_source_hint(self) -> str:
        return "Set ZETA_ZPP_SRC_DIR to a local checkout or run from the zpp checkout with ./zbuild.py"

    @property
    def folly_conan_generators_dir(self) -> Path:
        build_type_dir = self.repo_config.builder_dir / "folly" / "build" / self.args.build_type / "conan" / "build" / self.args.build_type / "generators"
        if build_type_dir.is_dir():
            return build_type_dir
        return self.repo_config.builder_dir / "folly" / "build" / "Release" / "conan" / "build" / "Release" / "generators"

    @property
    def folly_cmake_dir(self) -> Path:
        return self.repo_config.install_prefix / "lib" / "cmake" / "folly"

    def validate(self) -> None:
        super().validate()
        if not self.conanfile.is_file():
            raise RuntimeError(f"zpp Conan recipe not found: {self.conanfile}")
        if not (self.cmake_util_dir / "common.cmake").is_file():
            raise RuntimeError(
                f"zeta_forge cmake_util not found: {self.cmake_util_dir}\n"
                "Ensure $ZETAX_ROOT/zeta_forge/cmake_util exists before building zpp."
            )
        if self.typed_args.build_taskflow_module and not self.taskflow_source_dir.is_dir():
            raise RuntimeError(
                f"Taskflow source directory not found: {self.taskflow_source_dir}\n"
                "Set ZETA_TASKFLOW_SRC_DIR to a local checkout or initialize the zeta_forge submodule with: git -C \"$ZETAX_ROOT/zeta_forge\" submodule update --init --recursive 3rd/taskflow"
            )
        if not (self.rapidjson_source_dir / "include" / "rapidjson").is_dir():
            raise RuntimeError(
                f"RapidJSON source directory not found or invalid: {self.rapidjson_source_dir}\n"
                "Set ZETA_RAPIDJSON_SRC_DIR to a local checkout or initialize the zeta_forge submodule with: git -C \"$ZETAX_ROOT/zeta_forge\" submodule update --init --recursive 3rd/rapidjson"
            )
        if self.typed_args.build_hpx_examples and not self.typed_args.build_examples:
            raise RuntimeError("--with-hpx-examples requires examples to be enabled")

    def conan_input_files(self) -> list[Path]:
        return [self.conanfile]

    def conan_install_command(self) -> list[object]:
        return [
            "conan",
            "install",
            self.conanfile,
            f"--output-folder={self.conan_root}",
            "--build=missing",
            "-s",
            f"build_type={self.args.build_type}",
            "-s",
            f"compiler.cppstd={self.repo_config.cxx_standard}",
            "-c",
            "tools.cmake.cmaketoolchain:generator=Ninja",
        ]

    def configure_command(self) -> list[object]:
        cmake_prefix_paths = [str(self.repo_config.install_prefix)]
        if self.typed_args.build_folly_module:
            cmake_prefix_paths.append(str(self.folly_conan_generators_dir))

        command: list[object] = [
            "cmake",
            "-S",
            self.source_dir,
            "-B",
            self.build_dir,
            "-G",
            "Ninja",
            "-Wno-dev",
            f"-DCMAKE_BUILD_TYPE={self.args.build_type}",
            "-DCMAKE_MAP_IMPORTED_CONFIG_DEBUG=Release",
            f"-DCMAKE_TOOLCHAIN_FILE={self.conan_toolchain_file}",
            f"-DCMAKE_PREFIX_PATH={';'.join(cmake_prefix_paths)}",
            f"-DCMAKE_INSTALL_PREFIX={self.repo_config.install_prefix}",
            f"-DCMAKE_CXX_STANDARD={self.repo_config.cxx_standard}",
            f"-DZETA_CMAKE_UTIL_DIR={self.cmake_util_dir}",
            f"-DRAPIDJSON_ROOT={self.rapidjson_source_dir / 'include'}",
            "-DZPP_USE_CONAN=ON",
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
                    f"-DOpenSSL_DIR={self.folly_conan_generators_dir}",
                ]
            )
        if self.typed_args.build_taskflow_module:
            command.append(f"-DTASKFLOW_ROOT={self.taskflow_source_dir}")
        return command


def build_parser() -> argparse.ArgumentParser:
    parser = common_build_argument_parser("Build zpp")
    parser.add_argument("--no-tests", action="store_true")
    parser.add_argument("--no-examples", action="store_true")
    parser.add_argument("--with-hpx-examples", action="store_true")
    parser.add_argument("--with-folly", action="store_true")
    parser.add_argument("--with-nng", action="store_true")
    parser.add_argument("--with-taskflow", action="store_true")
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


def main(script_path: Path, *, argv: list[str] | None = None, source_dir_default: Path | None = None) -> int:
    project_source_defaults = {}
    if source_dir_default is not None:
        project_source_defaults["ZETA_ZPP_SRC_DIR"] = source_dir_default

    args = parse_args(argv)
    repo_config = load_repo_config(script_path, project_source_defaults=project_source_defaults)
    ZppBuilder(script_path=script_path, repo_config=repo_config, args=args).run()
    return 0


def cli(script_path: Path, *, source_dir_default: Path | None = None) -> int:
    try:
        return main(script_path, source_dir_default=source_dir_default)
    except Exception as exc:
        print(exc, file=sys.stderr)
        return 1
