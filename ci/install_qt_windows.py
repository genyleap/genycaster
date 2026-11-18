#!/usr/bin/env python3
"""Install official Qt 6.12 Windows MSVC archives when aqt lacks Updates.xml.

Qt 6.12.0's public Windows repository currently exposes version/arch-specific
component directories, but not the top-level Updates.xml expected by aqtinstall.
This installer downloads ONLY fixed official 6.12.0 MSVC 2022 components, checks
the upstream SHA1 sidecars, and extracts archives with 7-Zip. No Qt account,
unofficial mirror, or downgraded Qt version is required.
"""
from __future__ import annotations

import argparse
from html.parser import HTMLParser
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from urllib.request import urlopen
from urllib.parse import urljoin

VERSION = "6.12.0"
STAMP = "6.12.0-0-202609280346"
ARCHIVE_LAYOUTS = {
    "x86_64": ("windows_x86", "qt6_6120_msvc2022_64", "win64_msvc2022_64"),
    "arm64": ("windows_arm64", "qt6_6120", "win64_msvc2022_arm64"),
}

# Qt WebEngine 6.140 is distributed separately from the Qt 6.12 desktop
# component index. These are the official standalone extension directories,
# verified against download.qt.io for BOTH Windows architectures.
WEBENGINE_LAYOUTS = {
    "x86_64": ("windows_x86", "msvc2022_64",
               "win64_msvc2022_64", "6.12.0-0-202609281005"),
    "arm64": ("windows_arm64", "msvc2022_arm64",
              "win64_msvc2022_arm64", "6.12.0-0-202611110526"),
}


def webengine_archive_layout(
    architecture: str,
) -> tuple[str, dict[str, tuple[str, ...]], str]:
    platform, folder, suffix, stamp = WEBENGINE_LAYOUTS[architecture]
    base = ("https://download.qt.io/online/qtsdkrepository/"
            f"{platform}/extensions/qtwebengine/6120/61400/{folder}/")
    groups = {
        f"extensions.qtwebengine.6120.61400.{suffix}":
            ("qtwebengine", "qtwebview_qtwebengine_plugin"),
    }
    return base, groups, stamp


def archive_layout(architecture: str) -> tuple[str, dict[str, tuple[str, ...]]]:
    platform, leaf, suffix = ARCHIVE_LAYOUTS[architecture]
    base = ("https://download.qt.io/online/qtsdkrepository/"
            f"{platform}/desktop/qt6_6120/{leaf}/")
    packages = {
        f"qt.qt6.6120.{suffix}": (
            "qtbase", "qtdeclarative", "qttools", "qtsvg"
        ),
        f"qt.qt6.6120.addons.qtshadertools.{suffix}": ("qtshadertools",),
        f"qt.qt6.6120.addons.qtmultimedia.{suffix}": ("qtmultimedia",),
        f"qt.qt6.6120.addons.qtlottie.{suffix}": ("qtlottie",),
        # Origin-bound Mini App SDK host: both the QWidget and Quick
        # WebEngine runtimes and their isolated WebChannel transport are
        # mandatory. A partial install must fail before compilation.
        f"qt.qt6.6120.addons.qtwebchannel.{suffix}": ("qtwebchannel",),
    }
    return base, packages


class Hrefs(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.hrefs: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if tag != "a":
            return
        for attr, value in attrs:
            if attr == "href" and value:
                self.hrefs.append(value)


def archives_from_index(
    html: str, required: tuple[str, ...], *, stamp: str = STAMP
) -> list[str]:
    links = Hrefs()
    links.feed(html)
    names = [link for link in links.hrefs if link.startswith(stamp)
             and link.endswith(".7z") and "/" not in link]
    selected: list[str] = []
    for component in required:
        matches = [name for name in names
                   if name[len(stamp):].startswith(component + "-")
                   or name[len(stamp):] == component + ".7z"]
        if len(matches) != 1:
            raise ValueError(
                f"Expected one pinned 6.12 MSVC archive for {component}, found {matches}"
            )
        selected.append(matches[0])
    return selected


def read_url(url: str) -> bytes:
    with urlopen(url, timeout=90) as response:
        return response.read()


def download_archive(url: str, destination: Path) -> None:
    checksum = read_url(url + ".sha1").decode("ascii").strip().split()[0]
    if not re.fullmatch(r"[0-9a-fA-F]{40}", checksum):
        raise ValueError(f"Invalid upstream checksum for {url}")
    digest = hashlib.sha1(usedforsecurity=False)
    with urlopen(url, timeout=180) as stream, destination.open("wb") as output:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
            output.write(chunk)
    if digest.hexdigest().lower() != checksum.lower():
        destination.unlink(missing_ok=True)
        raise ValueError(f"Qt archive checksum mismatch: {url}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--destination", type=Path, default=Path("Qt"))
    parser.add_argument("--architecture", choices=tuple(ARCHIVE_LAYOUTS),
                        default="x86_64")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--webengine-only", action="store_true",
                        help="Install official Qt WebEngine 6.140 MSVC extension into existing Qt")
    args = parser.parse_args()
    if args.webengine_only:
        base, groups, stamp = webengine_archive_layout(args.architecture)
    else:
        base, groups = archive_layout(args.architecture)
        stamp = STAMP
    packages: list[tuple[str, str]] = []
    for package, components in groups.items():
        index = urljoin(base, package + "/")
        html = read_url(index).decode("utf-8")
        for archive in archives_from_index(html, components, stamp=stamp):
            packages.append((package, archive))
    if args.dry_run:
        for package, archive in packages:
            print(urljoin(base, package + "/" + archive))
        return 0

    executable = (shutil.which("7z") or shutil.which("7za") or
                  r"C:\Program Files\7-Zip\7z.exe")
    if not Path(executable).is_file():
        raise RuntimeError("7-Zip is required to install Qt Windows archives")

    destination = args.destination.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    archive_dir = destination / ".archive-cache"
    archive_dir.mkdir(exist_ok=True)
    for package, archive in packages:
        url = urljoin(base, package + "/" + archive)
        target = archive_dir / archive
        print(f"Downloading {archive}", flush=True)
        download_archive(url, target)
        subprocess.run(
            [executable, "x", "-y", f"-o{destination}", str(target)],
            check=True, stdout=subprocess.DEVNULL
        )
        target.unlink()

    qt_configs = list(destination.rglob("lib/cmake/Qt6/Qt6Config.cmake"))
    if len(qt_configs) != 1:
        raise ValueError(f"Expected exactly one Qt6Config.cmake, got {qt_configs}")
    root = qt_configs[0].parents[3]
    for relative in ("lib/cmake/Qt6Multimedia/Qt6MultimediaConfig.cmake",
                     "lib/cmake/Qt6Qml/Qt6QmlConfig.cmake",
                     "lib/cmake/Qt6WebChannel/Qt6WebChannelConfig.cmake",
                     "bin/qmake.exe"):
        if not (root / relative).is_file():
            raise ValueError(f"Installed Qt is incomplete: {root / relative}")
    if args.webengine_only:
        required = (
            "lib/cmake/Qt6WebEngineCore/Qt6WebEngineCoreConfig.cmake",
            "lib/cmake/Qt6WebEngineQuick/Qt6WebEngineQuickConfig.cmake",
            "lib/cmake/Qt6WebEngineWidgets/Qt6WebEngineWidgetsConfig.cmake",
        )
        for relative in required:
            if not (root / relative).is_file():
                raise ValueError(f"Official WebEngine extension incomplete: {root / relative}")
        if not any((root / relative).is_file() for relative in (
            "bin/QtWebEngineProcess.exe", "libexec/QtWebEngineProcess.exe"
        )):
            raise ValueError("Official WebEngine extension missing QtWebEngineProcess.exe")
        configured_root = os.environ.get("QT_ROOT_DIR")
        if configured_root and Path(configured_root).resolve() != root.resolve():
            raise ValueError("WebEngine extension installed into a different Qt prefix")
    print(f"Qt {VERSION} installed: {root}")
    github_env = os.environ.get("GITHUB_ENV")
    if github_env:
        with Path(github_env).open("a", encoding="utf-8") as output:
            output.write(f"QT_ROOT_DIR={root}\n")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"::error::{error}", file=sys.stderr)
        raise SystemExit(1)
