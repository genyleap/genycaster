#!/usr/bin/env python3
"""Preflight packaged Qt 6.12 shared-library dependencies on Linux.

linuxdeploy's Qt plugin recursively resolves QPA and QML dependencies. Catch
all missing SONAMEs before the expensive GenyCaster C++26 build, rather than
discovering them one at a time during AppDir deployment.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess

REQUIRED_PLUGINS = (
    ("XCB platform", "plugins/platforms/libqxcb.so"),
    ("OpenSSL TLS", "plugins/tls/libqopensslbackend.so"),
    ("Qt Quick Controls", "qml/QtQuick/Controls/libqtquickcontrols2plugin.so"),
    ("Qt Quick Basic style",
     "qml/QtQuick/Controls/Basic/libqtquickcontrols2basicstyleplugin.so"),
)
MEDIA_PLUGINS = (
    ("FFmpeg", "plugins/multimedia/libffmpegmediaplugin.so"),
    ("GStreamer", "plugins/multimedia/libgstreamermediaplugin.so"),
)


def unresolved_sonames(output: str) -> set[str]:
    """Extract all unresolvable linked SONAMEs from ldd, including transitive ones."""
    return set(re.findall(r"^\s*(\S+)\s+=>\s+not found\s*$", output, re.M))


def inspect(binary: Path, env: dict[str, str]) -> tuple[set[str], str]:
    completed = subprocess.run(
        ["ldd", str(binary)],
        env=env,
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )
    missing = unresolved_sonames(completed.stdout + "\n" + completed.stderr)
    if completed.returncode != 0 and not missing:
        return set(), completed.stderr.strip() or f"ldd exit {completed.returncode}"
    return missing, ""


def preflight(qt_root: Path) -> None:
    if not (qt_root / "lib").is_dir():
        raise ValueError(f"Qt 6.12 library directory missing: {qt_root / 'lib'}")
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = str(qt_root / "lib") + (
        ":" + env["LD_LIBRARY_PATH"] if env.get("LD_LIBRARY_PATH") else ""
    )
    failures: list[str] = []
    for name, relative in REQUIRED_PLUGINS:
        plugin = qt_root / relative
        if not plugin.is_file():
            failures.append(f"{name}: missing Qt plugin {plugin}")
            continue
        missing, error = inspect(plugin, env)
        if missing or error:
            failures.append(
                f"{name}: {', '.join(sorted(missing)) if missing else error}"
            )
        else:
            print(f"Qt {name} dependencies: OK")

    usable_media = 0
    media_failures: list[str] = []
    for name, relative in MEDIA_PLUGINS:
        plugin = qt_root / relative
        if not plugin.is_file():
            continue
        missing, error = inspect(plugin, env)
        if missing or error:
            media_failures.append(
                f"{name}: {', '.join(sorted(missing)) if missing else error}"
            )
        else:
            usable_media += 1
            print(f"Qt {name} multimedia dependencies: OK")
    if usable_media < 1:
        failures.append(
            "No usable Qt Multimedia backend (FFmpeg/GStreamer): "
            + ("; ".join(media_failures) or "both plugins absent")
        )
    elif media_failures:
        print("Optional Qt Multimedia backends unavailable: "
              + "; ".join(media_failures))

    if failures:
        raise ValueError(
            "Qt runtime preflight FAILED before product compilation:\n  "
            + "\n  ".join(failures)
            + "\nInstall the missing runtime SONAMEs into the Ubuntu 24.04 "
            "build environment; do not install host Ubuntu 25.10 libraries."
        )
    print("Qt 6.12 XCB/QML/TLS/Multimedia runtime preflight: PASS")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--qt-root", type=Path, required=True)
    args = parser.parse_args()
    try:
        preflight(args.qt_root)
    except ValueError as exc:
        parser.exit(1, f"{exc}\n")


if __name__ == "__main__":
    main()
