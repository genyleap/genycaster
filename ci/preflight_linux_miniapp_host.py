#!/usr/bin/env python3
"""Fail-closed preflight for the optional native Linux Qt WebEngine Mini App host.

This checks the *installed SDK*, not merely CMake target declarations.
It intentionally does not enable Mini Apps in the normal Linux release path.
"""
from __future__ import annotations

import argparse
from pathlib import Path


def missing_host_files(root: Path) -> list[str]:
    """Return human-readable blockers, without mutating the Qt installation."""
    root = root.resolve()
    missing: list[str] = []
    cmake = root / "lib" / "cmake"
    for module in ("Qt6Widgets", "Qt6WebChannel", "Qt6WebEngineCore",
                   "Qt6WebEngineWidgets"):
        if not (cmake / module / f"{module}Config.cmake").is_file():
            missing.append(f"Qt CMake module: {module}")

    subprocess_paths = (
        root / "libexec" / "QtWebEngineProcess",
        root / "bin" / "QtWebEngineProcess",
    )
    if not any(path.is_file() and path.stat().st_mode & 0o111 for path in subprocess_paths):
        missing.append("executable QtWebEngineProcess (libexec/ or bin/)")

    resource_roots = (root / "resources", root / "lib" / "resources")
    if not any(any(folder.glob("qtwebengine_resources*.pak")) for folder in resource_roots):
        missing.append("Qt WebEngine resource .pak files")
    if not any((folder / "icudtl.dat").is_file() for folder in resource_roots):
        missing.append("Qt WebEngine ICU data (icudtl.dat)")

    locale_roots = (root / "translations" / "qtwebengine_locales",
                    root / "resources" / "qtwebengine_locales")
    if not any(folder.is_dir() and any(folder.glob("*.pak")) for folder in locale_roots):
        missing.append("Qt WebEngine locales (.pak)")
    return missing


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qt-root", required=True, type=Path,
                        help="Exact Qt 6.12 Linux architecture SDK prefix")
    args = parser.parse_args()
    if not args.qt_root.is_dir():
        parser.error(f"Qt SDK directory does not exist: {args.qt_root}")
    missing = missing_host_files(args.qt_root)
    if missing:
        print("Linux Mini App WebEngine preflight FAILED:")
        for item in missing:
            print(f"  - Missing {item}")
        print("Do not enable or ship the native Mini App host until the matching "
              "Qt WebEngine extension, WebChannel, resources and subprocess "
              "are installed and runtime-tested.")
        return 2
    print("Linux Mini App WebEngine SDK files found. "
          "Native compilation, packaging and runtime security tests are still required.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
