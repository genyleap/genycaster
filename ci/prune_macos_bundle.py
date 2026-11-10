#!/usr/bin/env python3
"""Prune the *unused* Qt LocalStorage/Sql deployment chain from a macOS .app.

macdeployqt can copy QtQmlLocalStorage and the quick localstorage plugin,
both of which reference QtSql. Deleting QtSql alone creates a broken app.
Reject any change if first-party QML imports LocalStorage or a remaining
Mach-O image still needs any of these libraries.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import shutil
import subprocess


def _candidates(contents: Path, excluded: tuple[Path, ...]):
    for path in contents.rglob("*"):
        if not path.is_file() or path.is_symlink():
            continue
        if any(path == removed or removed in path.parents for removed in excluded):
            continue
        parts = path.relative_to(contents).parts
        if (path.suffix in {".dylib", ".so", ".bundle"}
            or (parts and parts[0] in {"MacOS", "Helpers"})
            or (any(part.endswith(".framework") for part in parts)
                and "Versions" in parts and path.name.startswith("Qt"))):
            yield path


def _check_sources(qml_source: Path) -> None:
    if not qml_source.is_dir():
        raise ValueError(f"QML source directory missing: {qml_source}")
    imports = re.compile(r"^\s*import\s+(?:QtQuick|QtQml)\.LocalStorage\b", re.M)
    for source in qml_source.rglob("*.qml"):
        if imports.search(source.read_text(encoding="utf-8")):
            raise ValueError(f"Cannot prune Qt LocalStorage: imported by {source}")


def prune(app: Path, qml_source: Path | None = None) -> int:
    contents = app / "Contents"
    if not (contents / "MacOS/Genycaster").is_file():
        raise ValueError("Not a GenyCaster macOS application bundle")
    if qml_source is not None:
        _check_sources(qml_source)
    # This exact set is an optional Qt deployment closure, not an arbitrary
    # recursive removal of frameworks that happen to look unrelated.
    obsolete = (
        contents / "PlugIns/sqldrivers",
        contents / "PlugIns/quick/libqmllocalstorageplugin.dylib",
        contents / "Frameworks/QtQmlLocalStorage.framework",
        contents / "Resources/qml/QtQuick/LocalStorage",
        contents / "Resources/qml/QtQml/LocalStorage",
        contents / "Frameworks/QtSql.framework",
    )
    if not any(path.exists() or path.is_symlink() for path in obsolete):
        return 0
    users = []
    for binary in _candidates(contents, obsolete):
        result = subprocess.run(
            ["/usr/bin/otool", "-L", str(binary)],
            capture_output=True, text=True, check=False, timeout=20,
        )
        if result.returncode:
            raise ValueError(f"Cannot inspect Mach-O {binary}: {result.stderr}")
        if any(symbol in result.stdout for symbol in (
            "QtSql.framework/", "libQt6Sql",
            "QtQmlLocalStorage.framework/", "libqmllocalstorageplugin",
        )):
            users.append(str(binary.relative_to(contents)))
    if users:
        raise ValueError("Pruned Qt SQL/LocalStorage would be required by: "
                         + ", ".join(users))
    removed = []
    for path in obsolete:
        if path.is_symlink() or (path.is_file() and not path.is_dir()):
            path.unlink()
            removed.append(str(path.relative_to(contents)))
        elif path.is_dir():
            shutil.rmtree(path)
            removed.append(str(path.relative_to(contents)))
    print("Pruned unused macOS Qt SQL/LocalStorage components: "
          + (", ".join(removed) if removed else "none"))
    return len(removed)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("app", type=Path)
    parser.add_argument("--qml-source", type=Path, required=True)
    args = parser.parse_args()
    prune(args.app, args.qml_source)


if __name__ == "__main__":
    main()
