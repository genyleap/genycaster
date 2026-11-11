#!/usr/bin/env python3
"""Bundle the compiler's C++ runtime in a linuxdeploy AppDir.

linuxdeploy blacklists libstdc++.so.6. This conflicts with GenyCaster's
GCC 16/C++26 executable: Ubuntu 24.04's libstdc++ lacks GLIBCXX_3.4.35.
The copied library belongs to the target compiler, never the Qt SDK.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import shutil
import subprocess


LAUNCHER = """#!/bin/sh
set -eu
appdir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
export LD_LIBRARY_PATH="$appdir/usr/lib{{EXISTING_LIBRARY_PATH}}"
# Qt WebEngine uses an out-of-process Chromium renderer with its own resources.
# Resolve every path relative to the installed AppDir, never the CI Qt SDK.
if [ -x "$appdir/usr/libexec/QtWebEngineProcess" ]; then
    export QTWEBENGINEPROCESS_PATH="$appdir/usr/libexec/QtWebEngineProcess"
    export QTWEBENGINE_RESOURCES_PATH="$appdir/usr/resources"
    export QTWEBENGINE_LOCALES_PATH="$appdir/usr/translations/qtwebengine_locales"
fi
# Qt may prefer native Wayland on the host, although the portable package
# only includes XCB. Use XWayland when available instead of failing QPA init.
# Respect explicit user/application platform overrides.
if [ -z "${QT_QPA_PLATFORM:-}" ] && [ -n "${WAYLAND_DISPLAY:-}" ] &&
   [ -n "${DISPLAY:-}" ] &&
   [ ! -f "$appdir/usr/plugins/platforms/libqwayland-generic.so" ] &&
   [ ! -f "$appdir/usr/plugins/platforms/libqwayland-egl.so" ]; then
    export QT_QPA_PLATFORM=xcb
fi
exec "$appdir/usr/bin/Genycaster" "$@"
""".replace("{{EXISTING_LIBRARY_PATH}}", "$" + "{LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}")


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def compiler_runtime(compiler: str) -> Path:
    result = subprocess.run(
        [compiler, "-print-file-name=libstdc++.so.6"],
        capture_output=True, text=True, check=True, timeout=15,
    )
    value = result.stdout.strip()
    path = Path(value)
    require(path.is_absolute() and path.is_file(),
            f"{compiler} did not resolve target libstdc++.so.6: {value!r}")
    return path.resolve(strict=True)


def elf_machine(path: Path) -> str:
    result = subprocess.run(
        ["readelf", "-h", str(path)], capture_output=True,
        text=True, check=True, timeout=15,
    )
    match = re.search(r"^\s*Machine:\s*(.+)$", result.stdout, re.M)
    require(match is not None, f"Cannot read ELF machine: {path}")
    return match.group(1).strip()


def symbol_versions(path: Path) -> set[tuple[int, int, int]]:
    result = subprocess.run(
        ["readelf", "-W", "--version-info", str(path)], capture_output=True,
        text=True, check=True, timeout=15,
    )
    return {
        (int(a), int(b), int(c))
        for a, b, c in re.findall(r"GLIBCXX_(\d+)\.(\d+)\.(\d+)", result.stdout)
    }


def required_glibc_versions(path: Path) -> set[tuple[int, int]]:
    result = subprocess.run(
        ["readelf", "-W", "--version-info", str(path)],
        capture_output=True, text=True, check=True, timeout=15,
    )
    return {
        (int(a), int(b))
        for a, b in re.findall(r"Name: GLIBC_(\d+)\.(\d+)\b", result.stdout)
    }


def install(appdir: Path, compiler: str = "g++-16") -> Path:
    root = appdir.resolve()
    executable = root / "usr/bin/Genycaster"
    require(executable.is_file() and not executable.is_symlink(),
            "AppDir missing compiled Genycaster executable")
    origin = root / "AppRun"
    require(origin.is_symlink() and origin.resolve() == executable.resolve(),
            "Expected linuxdeploy AppRun symlink to usr/bin/Genycaster")
    source = compiler_runtime(compiler)
    target = root / "usr/lib/libstdc++.so.6"
    require(source != target.resolve(),
            "Source C++ runtime unexpectedly resolves inside AppDir")
    require(elf_machine(source) == elf_machine(executable),
            "Compiler C++ runtime and executable have different architectures")
    required = symbol_versions(executable)
    available = symbol_versions(source)
    require(required.issubset(available),
            "Compiler libstdc++ lacks executable's GLIBCXX symbols")
    # Ubuntu 24.04 is the published minimum Linux baseline (glibc 2.39).
    # Bundling an ABI-compatible libstdc++ must not silently raise glibc's
    # minimum version to that of a newer developer machine.
    needed_glibc = required_glibc_versions(source)
    require(all(version <= (2, 39) for version in needed_glibc),
            "Target C++ runtime requires glibc newer than Ubuntu 24.04")
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.is_symlink():
        target.unlink()
    shutil.copy2(source, target)
    require(target.is_file() and target.stat().st_size > 100000,
            "Bundled C++ runtime missing or implausibly small")
    require(elf_machine(target) == elf_machine(executable),
            "Bundled C++ runtime architecture mismatch")
    origin.unlink()
    origin.write_text(LAUNCHER, encoding="utf-8")
    origin.chmod(0o755)

    notice = Path("/usr/share/doc/libstdc++6/copyright")
    if notice.is_file():
        doc = root / "usr/share/doc/genycaster"
        doc.mkdir(parents=True, exist_ok=True)
        shutil.copy2(notice, doc / "libstdc++-copyright")
    print(f"Bundled target C++ runtime: {source} -> {target}")
    print(f"Executable max GLIBCXX requirement: {max(required) if required else 'none'}")
    return target


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("appdir", type=Path)
    parser.add_argument("--compiler", default="g++-16")
    args = parser.parse_args()
    install(args.appdir, args.compiler)


if __name__ == "__main__":
    main()
