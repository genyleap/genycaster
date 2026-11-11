#!/usr/bin/env python3
"""Turn an already self-contained linuxdeploy AppDir into a native Debian installer.

Both the AppImage and .deb use the same verified Qt/QML/runtime directory.
glibc and system graphics libraries remain system-managed. The compiler's
versioned libstdc++ and Qt/QML runtime remain under /opt/genycaster,
never copied into the distribution's /usr/lib.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import shutil
import subprocess


def build(appdir: Path, output: Path, version: str, arch: str) -> Path:
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:-(?:alpha|beta|rc)[1-9][0-9]*)?", version):
        raise ValueError("Invalid GenyCaster package version")
    if arch not in {"x86_64", "arm64"}:
        raise ValueError("Unsupported Linux target architecture")
    for required in ("AppRun", "usr/bin/Genycaster",
                     "usr/share/applications/genycaster.desktop",
                     "usr/share/icons/hicolor/512x512/apps/genycaster.png"):
        if not (appdir / required).exists():
            raise ValueError(f"Missing deployed AppDir entry: {required}")
    deb_arch = {"x86_64": "amd64", "arm64": "arm64"}[arch]
    deb_version = version.replace("-", "~", 1)
    output.mkdir(parents=True, exist_ok=True)
    filename = output / f"GenyCaster-v{version}-linux-{arch}.deb"
    staging = output / f".genycaster-deb-{arch}"
    if staging.exists():
        shutil.rmtree(staging)
    try:
        binary_root = staging / "opt" / "genycaster"
        shutil.copytree(appdir, binary_root, symlinks=True)
        desktop = staging / "usr/share/applications/genycaster.desktop"
        desktop.parent.mkdir(parents=True)
        desktop.write_text(
            (appdir / "usr/share/applications/genycaster.desktop")
            .read_text(encoding="utf-8")
            .replace("Exec=Genycaster", "Exec=/opt/genycaster/AppRun"),
            encoding="utf-8",
        )
        icon = staging / "usr/share/icons/hicolor/512x512/apps/genycaster.png"
        icon.parent.mkdir(parents=True)
        shutil.copy2(appdir / "usr/share/icons/hicolor/512x512/apps/genycaster.png", icon)
        control = staging / "DEBIAN/control"
        control.parent.mkdir(parents=True)
        control.parent.chmod(0o755)
        control.write_text(
            f"Package: genycaster\n"
            f"Version: {deb_version}\n"
            f"Section: net\n"
            f"Priority: optional\n"
            f"Architecture: {deb_arch}\n"
            f"Maintainer: Genyleap <info@genyleap.com>\n"
            f"Depends: libc6, libgl1, libopengl0, libegl1, libxkbcommon0, "
            f"libxkbcommon-x11-0, libxcb1, libxcb-cursor0, "
            f"libxcb-icccm4, libxcb-image0, libxcb-keysyms1, "
            f"libxcb-render-util0, libxcb-xinerama0, libfontconfig1, "
            f"libfreetype6, libsqlite3-0, libdbus-1-3, libsecret-1-0, "
            f"ca-certificates, xdg-utils\n"
            f"Description: GenyCaster decentralized social client\n"
            f" Native local-first application with bundled Qt and QML runtime.\n",
            encoding="utf-8",
        )
        subprocess.run(
            ["dpkg-deb", "--build", "--root-owner-group", str(staging), str(filename)],
            check=True, timeout=180,
        )
        if filename.stat().st_size < 4096:
            raise ValueError("Empty Debian installer")
        return filename
    finally:
        if staging.exists():
            shutil.rmtree(staging)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--appdir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--arch", required=True)
    args = parser.parse_args()
    print(build(args.appdir, args.output, args.version, args.arch))


if __name__ == "__main__":
    main()
