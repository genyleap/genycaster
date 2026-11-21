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


def otool_dependencies(output: str, binary: Path) -> list[str]:
    """Return Mach-O install names, excluding otool's file/architecture headers.

    On some macOS versions, `otool -L` emits both `path:` and
    `path (architecture arm64):`. Neither is a linked library.
    """
    dependencies = []
    for raw_line in output.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        # On arm64 otool can print each Mach-O slice's filename as a
        # separate header, sometimes using a fully resolved path different
        # from the argument (especially when the .app is mounted from DMG).
        # Headers terminate in ':'; actual install-name lines include
        # ' (compatibility version ...)' or ' (current version ...)'.
        # Do not interpret the executable/header path as a dependency.
        if line.endswith(":") and not line.startswith("@"):
            continue
        dependencies.append(line.split(" (compatibility version", 1)[0])
    return dependencies


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
    require(not any(in_module(path, "sqldrivers") for path in files),
            "Unused Qt SQL drivers must not be shipped in a macOS client")
    require(not (frameworks / "QtSql.framework").exists(),
            "Unused QtSql framework must not be shipped in a macOS client")
    require(not (frameworks / "QtQmlLocalStorage.framework").exists(),
            "Unused QtQmlLocalStorage framework must not be shipped in a macOS client")
    require(not any(path.name == "libqmllocalstorageplugin.dylib" for path in files),
            "Unused QML LocalStorage plugin must not be shipped in a macOS client")
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
        for dependency in otool_dependencies(result.stdout, binary):
            require(not dependency.startswith(("/opt/", "/usr/local/",
                    "/Users/", "/home/", "/Applications/Qt/")),
                    f"Build-host library leaked into {binary}: {dependency}")
            if dependency.startswith("@rpath/Qt") and ".framework/" in dependency:
                name = dependency.removeprefix("@rpath/").split(".framework/", 1)[0]
                require((frameworks / (name + ".framework")).is_dir(),
                        f"Unbundled Qt framework {name} referenced by {binary}")


def smoke(app: Path, architecture: str) -> None:
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
        # QSG_RHI_BACKEND=software is NOT a Qt 6 RHI selection. On the
        # headless Intel macOS 26 GitHub runner it fell back to Metal and
        # wedged the QML startup probe for 120 seconds even though the
        # signed x86_64 executable was valid. Request Qt Quick's documented
        # SOFTWARE SCENE GRAPH, not an invalid RHI name. The real release
        # application keeps its normal GPU backend; this is strictly an
        # isolated, no-SDK dependency smoke-test process.
        env.pop("QSG_RHI_BACKEND", None)
        if architecture == "x86_64":
            env["QT_QUICK_BACKEND"] = "software"
            # The macOS Intel CI host runs Qt Quick without a physical
            # display refresh. Force the single-threaded render loop: Qt's
            # Metal threaded loop can wait indefinitely on vsync there.
            # This applies ONLY to the isolated smoke-test subprocess.
            env["QSG_RENDER_LOOP"] = "basic"
        else:
            env.pop("QT_QUICK_BACKEND", None)
            env.pop("QSG_RENDER_LOOP", None)
        env["QML_DISABLE_DISK_CACHE"] = "1"
        # Expose scenegraph selection if this smoke still stalls. Never
        # accept a missing QML shell as a passing release check.
        env["QSG_INFO"] = "1"
        settings = str(Path(home) / "settings.ini")
        try:
            completed = subprocess.run(
                [str(executable), "--smoke-test", f"--settings-path={settings}"],
                env=env, capture_output=True, text=True,
                # Intel CI runners can take longer to initialize the
                # full Qt Quick scene with software RHI on a cold bundle.
                # Bound the timeout rather than silently skipping smoke.
                timeout=120 if architecture == "x86_64" else 45,
                check=False,
            )
        except subprocess.TimeoutExpired as exc:
            def decode_partial(value: bytes | str | None) -> str:
                return value.decode("utf-8", errors="replace") if isinstance(value, bytes) else (value or "")
            diagnostic = (decode_partial(exc.stdout) + "\n" +
                          decode_partial(exc.stderr))[-6000:]
            raise ValueError(
                f"Packaged macOS {architecture} startup smoke timed out; "
                f"last output:\n{diagnostic}"
            ) from exc
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
        smoke(args.app, args.arch)
    print(f"Verified standalone macOS {args.arch} bundle: {args.app}")


if __name__ == "__main__":
    main()
