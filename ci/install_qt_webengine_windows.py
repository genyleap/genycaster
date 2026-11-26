#!/usr/bin/env python3
"""Install official separately versioned Qt WebEngine 6.140 for Qt 6.12.

The Qt 6.12 addons repository has no WebEngine. The official Extensions
repository contains pinned architecture-specific archives instead.
Only official HTTPS packages with upstream SHA1 verification are accepted.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import sys
from urllib.parse import urljoin

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from ci.install_qt_windows import (
    ARCHIVE_LAYOUTS, VERSION, archives_from_index, download_archive, read_url,
    webengine_archive_layout,
)

ARCHITECTURES = ARCHIVE_LAYOUTS
WEBENGINE_STAMP = VERSION + "-0-"


def extension_layout(architecture: str) -> tuple[str, tuple[str, ...]]:
    base, groups, _ = webengine_archive_layout(architecture)
    package, components = next(iter(groups.items()))
    return urljoin(base, package + "/"), components


def checked_archives(html: str, required: tuple[str, ...]) -> tuple[str, ...]:
    return tuple(archives_from_index(html, required, stamp=WEBENGINE_STAMP))


def check_installation(root: Path) -> None:
    for relative in (
        "lib/cmake/Qt6Positioning/Qt6PositioningConfig.cmake",
        "lib/cmake/Qt6WebEngineCore/Qt6WebEngineCoreConfig.cmake",
        "lib/cmake/Qt6WebEngineWidgets/Qt6WebEngineWidgetsConfig.cmake",
        "lib/cmake/Qt6WebEngineQuick/Qt6WebEngineQuickConfig.cmake",
    ):
        if not (root / relative).is_file():
            raise ValueError(f"Official Qt WebEngine SDK component missing: {root / relative}")
    if not any((root / child).is_file() for child in (
        "bin/QtWebEngineProcess.exe", "libexec/QtWebEngineProcess.exe",
    )):
        raise ValueError("QtWebEngineProcess.exe missing from official extension")
    if not any(root.rglob("qtwebenginequickplugin.dll")):
        raise ValueError("QtWebEngine Quick QML plugin missing from official extension")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--destination", type=Path, default=Path("Qt"))
    parser.add_argument("--architecture", choices=tuple(ARCHITECTURES), required=True)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    base, expected = extension_layout(args.architecture)
    try:
        archives = checked_archives(read_url(base).decode("utf-8"), expected)
    except (OSError, ValueError) as error:
        raise ValueError(f"Official WebEngine 6.140 extension for Qt {VERSION} "
                         f"not available or incomplete; refusing a mixed Qt SDK: {error}") from error
    if args.dry_run:
        for archive in archives:
            print(urljoin(base, archive))
        return 0

    executable = shutil.which("7z") or shutil.which("7za") or r"C:\Program Files\7-Zip\7z.exe"
    if not Path(executable).is_file():
        raise ValueError("7-Zip is required for official Qt WebEngine installation")
    destination = args.destination.resolve()
    # ci/install_qt_windows.py extracts official Qt base archives directly
    # into --destination, and exports QT_ROOT_DIR=that exact directory.
    # Do not invent a 6.12.1/msvc2022_* subdirectory: the hosted Windows
    # runner verified the actual Qt6Config.cmake resides at Qt/lib/cmake.
    root = destination
    if not (root / "lib/cmake/Qt6/Qt6Config.cmake").is_file():
        raise ValueError(f"Install the matching official Qt 6.12 SDK first: {root}")
    cache = destination / ".webengine-archive-cache"
    cache.mkdir(parents=True, exist_ok=True)
    for archive in archives:
        target = cache / archive
        try:
            download_archive(urljoin(base, archive), target)
            subprocess.run(
                [executable, "x", "-y", f"-o{destination}", str(target)],
                check=True, stdout=subprocess.DEVNULL,
            )
        finally:
            target.unlink(missing_ok=True)
    check_installation(root)
    print(f"Pinned official Qt WebEngine 6.140 extension installed: {root}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"::error::{error}", file=sys.stderr)
        raise SystemExit(1)
