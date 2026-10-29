#!/usr/bin/env python3
"""Validate a packaged GenyCaster app's structure before publishing artifacts.

These checks validate packaging, not code signing, TLS handshakes, startup or
runtime behavior. CI must additionally gate those operations independently.
"""
from __future__ import annotations

import argparse
from pathlib import Path, PurePosixPath
import tarfile
import zipfile


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def validate_linux(path: Path) -> None:
    require(path.suffix == ".AppImage", "Linux package must be an AppImage")
    with path.open("rb") as data:
        header = data.read(12)
    require(header[:4] == b"\x7fELF", "AppImage ELF header missing")
    require(header[8:11] == b"AI\x02", "AppImage type-2 marker missing")


def validate_macos(path: Path) -> None:
    require(path.suffix == ".dmg", "macOS package must be a DMG")
    require(path.stat().st_size >= 512, "DMG too small")
    with path.open("rb") as data:
        data.seek(-512, 2)
        header = data.read(4)
    require(header == b"koly", "Apple UDIF DMG trailer missing")


def validate_web(path: Path) -> None:
    require(path.name.endswith(".tar.gz"), "Web package must be a gzipped tarball")
    with tarfile.open(path, "r:gz") as archive:
        members = archive.getmembers()
        names = {member.name for member in members}
        require(all(member.isfile() and not member.issym() and not member.islnk()
                    and not PurePosixPath(member.name).is_absolute()
                    and ".." not in PurePosixPath(member.name).parts
                    for member in members), "Web archive contains unsafe entries")
        for required in (
            "farcaster_client.html", "farcaster_client.js",
            "farcaster_client.wasm", "genycaster-assets.json",
            "genycaster-loader.js", "genycaster-sw.js",
        ):
            require(required in names, f"Web archive missing {required}")
        require(any(name.endswith(".wasm") for name in names), "Missing WASM payload")


def validate_android(path: Path) -> None:
    require(path.suffix == ".apk", "Android package must be an APK")
    with zipfile.ZipFile(path) as apk:
        names = set(apk.namelist())
        required = (
            "AndroidManifest.xml",
            "lib/arm64-v8a/libfarcaster_client_arm64-v8a.so",
            "lib/arm64-v8a/libQt6Network_arm64-v8a.so",
            "lib/arm64-v8a/libqt6keychain.so",
            "lib/arm64-v8a/libssl_3.so",
            "lib/arm64-v8a/libcrypto_3.so",
            "lib/arm64-v8a/libplugins_tls_qopensslbackend_arm64-v8a.so",
        )
        for member in required:
            require(member in names, f"Android APK missing {member}")
        require(any(name.endswith(".dex") for name in names), "Android APK missing DEX")
        require(not any(name.startswith("lib/x86") for name in names),
                "Unexpected x86 libraries in ARM64-only APK")


def validate_windows(path: Path) -> None:
    require(path.suffix == ".zip", "Windows package must be a ZIP")
    with zipfile.ZipFile(path) as archive:
        names = {name.replace("\\", "/").lower() for name in archive.namelist()}
    for required in (
        "farcaster_client.exe", "qt6core.dll", "qt6network.dll",
        "qt6keychain.dll", "platforms/qwindows.dll",
    ):
        require(required in names, f"Windows ZIP missing {required}")
    for needle in ("crypto", "ssl", "curl", "sqlite"):
        require(any(needle in PurePosixPath(name).name and name.endswith(".dll")
                    for name in names),
                f"Windows ZIP missing {needle} runtime DLL")


def verify(platform: str, path: Path) -> None:
    require(path.is_file() and path.stat().st_size > 0, "Package missing or empty")
    validators = {
        "linux": validate_linux,
        "macos": validate_macos,
        "windows": validate_windows,
        "android": validate_android,
        "web": validate_web,
    }
    validators[platform](path)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("platform", choices=("linux", "macos", "windows", "android", "web"))
    parser.add_argument("path", type=Path)
    args = parser.parse_args()
    verify(args.platform, args.path)
    print(f"Valid {args.platform} package: {args.path}")


if __name__ == "__main__":
    main()
