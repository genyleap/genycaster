#!/usr/bin/env python3
"""Remove only unused Qt SQL plugin drivers from a linuxdeploy AppDir.

GenyCaster uses the native SQLite API, not QtSql/QSqlDatabase. Linuxdeploy
may otherwise include an optional PostgreSQL/ODBC plugin whose transitive
system libraries are not available on clean desktop installations.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil


def prune(appdir: Path) -> int:
    root = appdir / "usr"
    if not root.is_dir():
        raise ValueError("Missing AppDir/usr; refusing to prune outside AppDir")
    removed = 0
    for directory in root.rglob("sqldrivers"):
        if directory.parent.name != "plugins":
            continue
        if directory.is_symlink():
            raise ValueError(f"Refusing symlinked Qt plugin directory: {directory}")
        if directory.is_dir():
            shutil.rmtree(directory)
            removed += 1
    return removed


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("appdir", type=Path)
    args = parser.parse_args()
    print(f"Removed {prune(args.appdir)} unused Qt SQL plugin groups")


if __name__ == "__main__":
    main()
