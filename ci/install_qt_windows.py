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
BASE = ("https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/"
        "qt6_6120/qt6_6120_msvc2022_64/")
STAMP = "6.12.0-0-202609280346"
PACKAGE_GROUPS = {
    "qt.qt6.6120.win64_msvc2022_64": (
        "qtbase", "qtdeclarative", "qttools", "qtsvg"
    ),
    "qt.qt6.6120.addons.qtshadertools.win64_msvc2022_64": ("qtshadertools",),
    "qt.qt6.6120.addons.qtmultimedia.win64_msvc2022_64": ("qtmultimedia",),
    "qt.qt6.6120.addons.qtlottie.win64_msvc2022_64": ("qtlottie",),
}


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


def archives_from_index(html: str, required: tuple[str, ...]) -> list[str]:
    links = Hrefs()
    links.feed(html)
    names = [link for link in links.hrefs if link.startswith(STAMP)
             and link.endswith(".7z") and "/" not in link]
    selected: list[str] = []
    for component in required:
        matches = [name for name in names if name[len(STAMP):].startswith(component + "-")]
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
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    packages: list[tuple[str, str]] = []
    for package, components in PACKAGE_GROUPS.items():
        index = urljoin(BASE, package + "/")
        html = read_url(index).decode("utf-8")
        for archive in archives_from_index(html, components):
            packages.append((package, archive))
    if args.dry_run:
        for package, archive in packages:
            print(urljoin(BASE, package + "/" + archive))
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
        url = urljoin(BASE, package + "/" + archive)
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
                     "bin/qmake.exe"):
        if not (root / relative).is_file():
            raise ValueError(f"Installed Qt is incomplete: {root / relative}")
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
