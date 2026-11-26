#!/usr/bin/env python3
"""Check the actual official Qt desktop package indexes before allocating build runners.

Only metadata and checksum sidecars are fetched. Archive downloads and binary
verification still belong to the platform jobs.
"""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import re
import sys
from urllib.parse import urljoin
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from ci.install_qt_windows import VERSION, STAMP, archive_layout, archives_from_index, read_url
from ci.install_qt_webengine_windows import extension_layout, checked_archives

REPOSITORY = "https://download.qt.io/online/qtsdkrepository/"
LINUX_ARCHITECTURES = {
    "x86_64": ("linux_x64", "x86_64", "linux_gcc_64"),
    "arm64": ("linux_arm64", "arm64", "linux_gcc_arm64"),
}


def require_checksum(url: str) -> None:
    digest = read_url(url + ".sha1").decode("ascii").strip().split()
    if not digest or not re.fullmatch(r"[a-fA-F0-9]{40}", digest[0]):
        raise ValueError(f"Missing or invalid official archive checksum: {url}")


def windows_archives(arch: str) -> list[str]:
    base, groups = archive_layout(arch)
    urls = []
    for package, components in groups.items():
        index = urljoin(base, package + "/")
        names = archives_from_index(read_url(index).decode("utf-8"), components, stamp=STAMP)
        urls.extend(urljoin(index, name) for name in names)
    index, components = extension_layout(arch)
    names = checked_archives(read_url(index).decode("utf-8"), components)
    urls.extend(urljoin(index, name) for name in names)
    return urls


def linux_package_archives(index: str, xml: bytes, name: str,
                           required: tuple[str, ...]) -> list[str]:
    updates = ET.fromstring(xml)
    packages = [p for p in updates.findall("PackageUpdate")
                if p.findtext("Name") == name]
    if len(packages) != 1:
        raise ValueError(f"Expected exactly one Qt package {name} at {index}")
    package = packages[0]
    stamp = package.findtext("Version", "")
    if not re.fullmatch(re.escape(VERSION) + r"-0-[0-9]{12}", stamp):
        raise ValueError(f"Incompatible Qt package version {stamp!r}: {name}")
    names = [part.strip() for part in
             package.findtext("DownloadableArchives", "").split(",")]
    if not all(re.fullmatch(r"[A-Za-z0-9_.-]+\.7z", n) for n in names):
        raise ValueError(f"Invalid or missing archive list: {name}")
    chosen = []
    for component in required:
        matches = [n for n in names if n == component + ".7z"
                   or n.startswith(component + "-")]
        if len(matches) != 1:
            raise ValueError(f"Expected exactly one {component} archive: {name}")
        chosen.append(matches[0])
    return [urljoin(index, name + "/" + stamp + n) for n in chosen]


def linux_archives(arch: str) -> list[str]:
    platform, directory, suffix = LINUX_ARCHITECTURES[arch]
    version_id = VERSION.replace(".", "")
    base = urljoin(REPOSITORY, f"{platform}/desktop/qt6_{version_id}/qt6_{version_id}/")
    xml = read_url(urljoin(base, "Updates.xml"))
    prefix = f"qt.qt6.{version_id}"
    urls = linux_package_archives(base, xml, f"{prefix}.{suffix}",
                                  ("qtbase", "qtdeclarative", "qttools", "qtsvg"))
    for addon in ("qtmultimedia", "qtwebchannel", "qtpositioning", "qtserialport"):
        urls.extend(linux_package_archives(base, xml, f"{prefix}.addons.{addon}.{suffix}",
                                          (addon,)))
    engine = urljoin(REPOSITORY, f"{platform}/extensions/qtwebengine/{version_id}/61400/{directory}/")
    urls.extend(linux_package_archives(engine, read_url(urljoin(engine, "Updates.xml")),
                                      f"extensions.qtwebengine.{version_id}.61400.{suffix}",
                                      ("qtwebengine", "qtwebview_qtwebengine_plugin")))
    return urls


def check_target(target: tuple[str, str]) -> tuple[str, int]:
    platform, arch = target
    urls = windows_archives(arch) if platform == "windows" else linux_archives(arch)
    with ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(require_checksum, urls))
    return f"{platform}/{arch}", len(urls)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--platform", choices=("all", "linux", "windows"), default="all")
    args = parser.parse_args()
    if args.version != VERSION:
        parser.error(f"Desktop workflow pin must agree with SDK installer: {VERSION}")
    targets = [(p, a) for p in ("linux", "windows")
               if args.platform in ("all", p) for a in ("x86_64", "arm64")]
    with ThreadPoolExecutor(max_workers=4) as pool:
        for target, count in pool.map(check_target, targets):
            print(f"PASS: {target}: Qt {VERSION}, {count} official archives and SHA1 sidecars", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, ET.ParseError) as error:
        print(f"::error::Desktop Qt SDK preflight failed: {error}", file=sys.stderr)
        raise SystemExit(1)
