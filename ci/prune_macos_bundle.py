#!/usr/bin/env python3
"""Remove only unused Qt SQL deployment extras from a macdeployqt .app.

The project uses SQLite directly, not QtSql. macdeployqt may still copy the
framework via optional Qt plugins; before removing it, inspect every shipped
Mach-O executable, plugin and framework for a genuine dynamic dependency.
Fail closed when a dependency exists or Mach-O inspection fails.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess


def _candidates(contents: Path, sql_framework: Path, sql_plugins: Path):
    for path in contents.rglob("*"):
        if not path.is_file() or path.is_symlink():
            continue
        if sql_framework in path.parents or sql_plugins in path.parents:
            continue
        parts = path.relative_to(contents).parts
        if (path.suffix in {".dylib", ".so", ".bundle"}
            or (parts and parts[0] in {"MacOS", "Helpers"})
            or (".framework" in "".join(parts)
                and "Versions" in parts and path.name.startswith("Qt"))):
            yield path


def prune(app: Path) -> int:
    contents = app / "Contents"
    if not (contents / "MacOS/Genycaster").is_file():
        raise ValueError("Not a GenyCaster macOS application bundle")
    frameworks = contents / "Frameworks"
    sql_framework = frameworks / "QtSql.framework"
    sql_plugins = contents / "PlugIns/sqldrivers"
    if not sql_framework.exists() and not sql_plugins.exists():
        return 0
    users: list[str] = []
    for binary in _candidates(contents, sql_framework, sql_plugins):
        result = subprocess.run(
            ["/usr/bin/otool", "-L", str(binary)],
            capture_output=True, text=True, check=False, timeout=20,
        )
        if result.returncode:
            raise ValueError(f"Cannot inspect Mach-O {binary}: {result.stderr}")
        if "QtSql.framework/" in result.stdout or "libQt6Sql" in result.stdout:
            users.append(str(binary.relative_to(contents)))
    if users:
        raise ValueError("QtSql is dynamically required by: " + ", ".join(users))
    count = 0
    if sql_plugins.exists():
        shutil.rmtree(sql_plugins)
        count += 1
    if sql_framework.exists():
        shutil.rmtree(sql_framework)
        count += 1
    print(f"Pruned {count} unused Qt SQL deployment directories from {app}")
    return count


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("app", type=Path)
    args = parser.parse_args()
    prune(args.app)


if __name__ == "__main__":
    main()
