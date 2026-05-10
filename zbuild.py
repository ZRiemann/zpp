#!/usr/bin/env python3
from __future__ import annotations

import os
from pathlib import Path
import sys


def _is_forge_root(candidate: Path) -> bool:
    return (
        (candidate / "common" / "zeta_forge" / "config.py").is_file()
        and (candidate / "builder").is_dir()
        and (candidate / "3rd").is_dir()
    )


def _expand_path(raw_path: str) -> Path:
    return Path(os.path.expandvars(os.path.expanduser(raw_path))).resolve()


def _find_forge_root(start: Path) -> Path:
    candidates: list[Path] = []
    forge_root_raw = os.environ.get("ZETA_FORGE_ROOT")
    if forge_root_raw:
        candidates.append(_expand_path(forge_root_raw))

    zetax_root_raw = os.environ.get("ZETAX_ROOT")
    if zetax_root_raw:
        candidates.append(_expand_path(zetax_root_raw) / "zeta_forge")

    for candidate in [start, *start.parents]:
        candidates.append(candidate)
        candidates.append(candidate / "zeta_forge")

    seen: set[Path] = set()
    for candidate in candidates:
        resolved = candidate.resolve()
        if resolved in seen:
            continue
        seen.add(resolved)
        if _is_forge_root(resolved):
            return resolved

    raise RuntimeError(
        "Unable to locate zeta_forge root. "
        "Set ZETA_FORGE_ROOT=/path/to/zeta_forge or load ZETAX_ROOT so "
        "$ZETAX_ROOT/zeta_forge is available."
    )


SCRIPT_PATH = Path(__file__).resolve()
FORGE_ROOT = _find_forge_root(SCRIPT_PATH.parent)
sys.path.insert(0, str(FORGE_ROOT / "common"))

from zeta_forge.projects.zpp import cli


if __name__ == "__main__":
    raise SystemExit(cli(SCRIPT_PATH, source_dir_default=SCRIPT_PATH.parent))
