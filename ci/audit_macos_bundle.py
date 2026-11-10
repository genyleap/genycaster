#!/usr/bin/env python3
"""Validate the *shipped* macOS application, not just its Mach-O executable.

macdeployqt without -qmldir can produce a valid DMG which fails on machines
without Qt: "module QtQuick.Controls plugin qtquickcontrols2plugin not found".
Run this on the .app after deployment AND on the .app mounted from its DMG.
The subprocess smoke test strips development Qt paths so a runner's SDK cannot
hide a missing packaged plugin.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def require(condition: bool, description: str) -> None:
    if not condition:
        raise ValueError(description)


def in_module(path: Path, *parts: str) -> bool:
    segments = path.parts
    return any(tuple(segments[i:i + len(parts)]) == tuple(parts)
               for i in range(len(segments) - len(parts) + 1))


def audit(app: Path, architecture: str, *, inspect_macho: bool = True) -> None:
    require(architecture in {"arm64", "x86_64"}, "Invalid target architecture")
    contents = app / "Contents"
    executable = contents / "MacOS/Genycaster"
    require(executable.is_file() and executable.stat().st_size > 4096,
            "Missing bundled macOS executable")
    require((contents / "Info.plist").is_file(), "Missing Info.plist")
    frameworks = contents / "Frameworks"
    for framework in ("QtCore", "QtGui", "QtQml", "QtQuick", "QtQuickControls2", "QtMultimedia"):
        require((frameworks / f"{framework}.framework").is_dir(),
                f"Missing bundled {framework} framework")
    files = [path for path in contents.rglob("*") if path.is_file()]
    require(any(path.name == "libqcocoa.dylib"
                and in_module(path, "platforms")
                for path in files), "Missing bundled Cocoa platform plugin")
    for component, plugin in (
        (("QtQuick", "Controls"), "qtquickcontrols2plugin"),
        (("QtQuick", "Controls", "Basic"), "qtquickcontrols2basicstyleplugin"),
    ):
        require(any(path.name == "qmldir" and in_module(path, *component)
                    for path in files),
                f"Missing bundled QML module {'.'.join(component)}")
        require(any(plugin in path.name and path.suffix == ".dylib"
                    and in_module(path, *component) for path in files),
                f"Missing bundled QML plugin {plugin}")
    require(any(path.name in ("libffmpegmediaplugin.dylib",
                              "libdarwinmediaplugin.dylib")
                and in_module(path, "multimedia") for path in files),
            "Missing bundled macOS Qt Multimedia backend")
    if not inspect_macho:
        return
    # Never allow compiler/Qt SDK paths to leak into installed dependencies.
    # All Qt dependencies in the executable and bundle plugins must be loaded
    # via @rpath from Contents/Frameworks.
    framework_binaries = [
        framework / "Versions" / "A" / framework.stem
        for framework in frameworks.glob("Qt*.framework")
    ]
    dynamic = [executable] + [
        path for path in files if path.suffix == ".dylib" and path != executable
    ] + [binary for binary in framework_binaries if binary.is_file()]
    for binary in dynamic:
        arch = subprocess.run(["/usr/bin/lipo", str(binary),
                               "-verify_arch", architecture],
                              capture_output=True, text=True, check=False)
        require(arch.returncode == 0,
                f"Wrong/missing {architecture} architecture: {binary}: {arch.stderr}")
        result = subprocess.run(["/usr/bin/otool", "-L", str(binary)],
                                capture_output=True, text=True, check=False)
        require(result.returncode == 0,
                f"Cannot inspect macOS dependencies: {binary}: {result.stderr}")
        for line in result.stdout.splitlines()[1:]:
            dependency = line.strip().split(" (compatibility version", 1)[0]
            if not dependency:
                continue
            require(not dependency.startswith(("/opt/", "/usr/local/",
                    "/Users/", "/home/", "/Applications/Qt/")),
                    f"Build-host library leaked into {binary}: {dependency}")
            if dependency.startswith("@rpath/Qt") and ".framework/" in dependency:
                name = dependency.removeprefix("@rpath/").split(".framework/", 1)[0]
                require((frameworks / (name + ".framework")).is_dir(),
                        f"Unbundled Qt framework {name} referenced by {binary}")


def smoke(app: Path) -> None:
    executable = app / "Contents/MacOS/Genycaster"
    with tempfile.TemporaryDirectory(prefix="genycaster-clean-macos-") as home:
        env = dict(os.environ)
        for key in (
            "QML_IMPORT_PATH", "QML2_IMPORT_PATH", "QT_PLUGIN_PATH",
            "QT_QPA_PLATFORM_PLUGIN_PATH", "QT_QPA_FONTDIR",
            "DYLD_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH",
            "DYLD_FALLBACK_LIBRARY_PATH", "DYLD_FALLBACK_FRAMEWORK_PATH",
        ):
            env.pop(key, None)
        env["HOME"] = home
        env["QT_QPA_PLATFORM"] = "cocoa"
        env["QSG_RHI_BACKEND"] = "software"
        env["QML_DISABLE_DISK_CACHE"] = "1"
        settings = str(Path(home) / "settings.ini")
        try:
            completed = subprocess.run(
                [str(executable), "--smoke-test", f"--settings-path={settings}"],
                env=env, capture_output=True, text=True, timeout=45,
                check=False,
            )
        except subprocess.TimeoutExpired as exc:
            raise ValueError("Packaged macOS app did not finish startup smoke test") from exc
        combined = (completed.stdout + "\n" + completed.stderr)[-10000:]
        require(completed.returncode == 0,
                f"Packaged macOS app failed startup smoke test "
                f"({completed.returncode}): {combined}")
        require("QQmlApplicationEngine failed to load component" not in combined,
                "Packaged macOS app could not load its QML root")
        print("Packaged macOS QML startup smoke: PASS")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("app", type=Path)
    parser.add_argument("arch", choices=("arm64", "x86_64"))
    parser.add_argument("--smoke", action="store_true")
    parser.add_argument("--skip-macho", action="store_true",
                        help="Only for synthetic fixture tests, not release CI")
    args = parser.parse_args()
    audit(args.app, args.arch, inspect_macho=not args.skip_macho)
    if args.smoke:
        require(sys.platform == "darwin", "macOS bundle smoke must run on macOS")
        smoke(args.app)
    print(f"Verified standalone macOS {args.arch} bundle: {args.app}")


if __name__ == "__main__":
    main()
