#!/usr/bin/env python3
"""Audit linuxdeploy's actual AppDir before AppImage and DEB packaging.

A valid AppImage ELF header does not imply QtQuick.Controls or Qt platform
plugins are bundled. Require Qt/QML runtime modules and resolve ELF deps
using the packaged libraries, not the installer's Qt SDK paths.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise ValueError(reason)


def validate(appdir: Path, *, inspect_elf: bool = True) -> None:
    root = appdir / "usr"
    executable = root / "bin/Genycaster"
    require(executable.is_file(), "Linux AppDir missing Genycaster executable")
    libraries = root / "lib"
    for name in ("libQt6Core.so.6", "libQt6Gui.so.6",
                 "libQt6Qml.so.6", "libQt6Quick.so.6", "libQt6Multimedia.so.6"):
        require((libraries / name).is_file(), f"Linux AppDir missing {name}")
    qml = root / "qml"
    require(qml.is_dir(), "Linux AppDir missing QML imports")
    required_plugins = (
        ("QtQuick/Controls", "libqtquickcontrols2plugin.so"),
        ("QtQuick/Controls/Basic", "libqtquickcontrols2basicstyleplugin.so"),
    )
    for module, plugin in required_plugins:
        directory = qml / module
        require((directory / "qmldir").is_file(),
                f"Linux AppDir missing QML module {module}")
        require((directory / plugin).is_file(),
                f"Linux AppDir missing QML plugin {plugin}")
    platform_plugins = [
        root / "plugins/platforms/libqxcb.so",
        root / "lib/qt6/plugins/platforms/libqxcb.so",
    ]
    require(any(path.is_file() for path in platform_plugins),
            "Linux AppDir missing Qt XCB platform plugin")
    multimedia = [
        root / "plugins/multimedia/libffmpegmediaplugin.so",
        root / "plugins/multimedia/libgstreamermediaplugin.so",
        root / "lib/qt6/plugins/multimedia/libffmpegmediaplugin.so",
        root / "lib/qt6/plugins/multimedia/libgstreamermediaplugin.so",
    ]
    require(any(path.is_file() for path in multimedia),
            "Linux AppDir missing Qt Multimedia runtime backend")
    if not inspect_elf:
        return
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = str(libraries)
    env.pop("QT_PLUGIN_PATH", None)
    env.pop("QML_IMPORT_PATH", None)
    env.pop("QML2_IMPORT_PATH", None)
    for binary in [executable,
                   qml / "QtQuick/Controls/libqtquickcontrols2plugin.so",
                   qml / "QtQuick/Controls/Basic/libqtquickcontrols2basicstyleplugin.so",
                   next(p for p in platform_plugins if p.is_file()),
                   next(p for p in multimedia if p.is_file())]:
        result = subprocess.run(["ldd", str(binary)], env=env,
                                capture_output=True, text=True, check=False)
        require(result.returncode == 0,
                f"Cannot inspect Linux runtime dependencies: {binary}: {result.stderr}")
        require("not found" not in result.stdout
                and "GLIBC_" not in result.stderr,
                f"Unresolved runtime dependency in {binary}:\n{result.stdout}\n{result.stderr}")
    print(f"Linux Qt/QML AppDir dependencies resolved: {appdir}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("appdir", type=Path)
    parser.add_argument("--skip-elf", action="store_true",
                        help="Synthetic tests only, not release CI")
    args = parser.parse_args()
    validate(args.appdir, inspect_elf=not args.skip_elf)


if __name__ == "__main__":
    main()
