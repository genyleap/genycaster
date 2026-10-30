#!/usr/bin/env python3
"""Validate a packaged GenyCaster app's structure before publishing artifacts.

These checks validate packaging, not code signing, TLS handshakes, startup or
runtime behavior. CI must additionally gate those operations independently.
"""
from __future__ import annotations

import argparse
from pathlib import Path, PurePosixPath
import struct
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


def validate_android_elf_alignment(
    name: str, data: bytes, *, check_load_mapping: bool = True
) -> None:
    """Reject 4 KiB ELF load segments, which can fail on 16 KiB Android kernels.

    Parse ELF64 little-endian program headers directly so the artifact
    acceptance check works without any external objdump/NDK dependencies.
    """
    require(len(data) >= 64 and data[:4] == b"\x7fELF",
            f"{name}: not an ELF library")
    require(data[4] == 2 and data[5] == 1,
            f"{name}: expected 64-bit little-endian ELF")
    require(struct.unpack_from("<H", data, 18)[0] == 183,
            f"{name}: expected AArch64 machine type")
    phoff = struct.unpack_from("<Q", data, 32)[0]
    phentsize = struct.unpack_from("<H", data, 54)[0]
    phnum = struct.unpack_from("<H", data, 56)[0]
    require(phentsize >= 56 and phnum > 0
            and phoff + phentsize * phnum <= len(data),
            f"{name}: invalid ELF program headers")
    load_segments = 0
    for index in range(phnum):
        offset = phoff + index * phentsize
        if struct.unpack_from("<I", data, offset)[0] != 1:
            continue
        load_segments += 1
        alignment = struct.unpack_from("<Q", data, offset + 48)[0]
        require(alignment >= 16384 and alignment & (alignment - 1) == 0,
                f"{name}: 4 KiB ELF LOAD alignment ({alignment} bytes); "
                "Android 16 KiB pages require at least 16384")
        # p_align alone is insufficient: when PatchELF adds a PT_LOAD with
        # incongruent file offset / virtual address, Android's loader can
        # resolve DT_VERNEED through the wrong bytes (e.g. "sign_ctx" rather
        # than "libdl.so") and abort before the Qt entry point runs.
        file_offset = struct.unpack_from("<Q", data, offset + 8)[0]
        virtual_address = struct.unpack_from("<Q", data, offset + 16)[0]
        if check_load_mapping:
            require((virtual_address - file_offset) % 16384 == 0,
                    f"{name}: incongruent ELF LOAD offset/vaddr for 16 KiB pages "
                    f"(file=0x{file_offset:x}, vaddr=0x{virtual_address:x})")
    require(load_segments > 0, f"{name}: missing ELF LOAD segment")


def validate_android_elf32_arm(name: str, data: bytes) -> None:
    """Confirm a 32-bit ARM library; 64-bit ELF alignment rules do not apply."""
    require(len(data) >= 52 and data[:4] == b"\x7fELF",
            f"{name}: not an ELF library")
    require(data[4] == 1 and data[5] == 1,
            f"{name}: expected 32-bit little-endian ELF")
    require(struct.unpack_from("<H", data, 18)[0] == 40,
            f"{name}: expected ARM machine type")
    header_size = struct.unpack_from("<H", data, 40)[0]
    require(header_size >= 52, f"{name}: invalid ELF32 header")


def validate_android(path: Path) -> None:
    require(path.suffix == ".apk", "Android package must be an APK")
    abi = "armeabi-v7a" if "-android-armeabi-v7a" in path.name else "arm64-v8a"
    with zipfile.ZipFile(path) as apk:
        names = set(apk.namelist())
        required = (
            "AndroidManifest.xml",
            f"lib/{abi}/libfarcaster_client_{abi}.so",
            f"lib/{abi}/libQt6Network_{abi}.so",
            f"lib/{abi}/libqt6keychain.so",
            f"lib/{abi}/libssl_3.so",
            f"lib/{abi}/libcrypto_3.so",
            f"lib/{abi}/libplugins_tls_qopensslbackend_{abi}.so",
        )
        for member in required:
            require(member in names, f"Android APK missing {member}")

        # Verify every native library, including provider DLL equivalents and
        # bundled OpenSSL; checking only Qt6Core misses real startup failures.
        libraries = sorted(name for name in names
                           if name.startswith(f"lib/{abi}/") and name.endswith(".so"))
        require(bool(libraries), f"Android APK contains no {abi} native libraries")
        require(not any(name.startswith("lib/") and name.endswith(".so")
                        and not name.startswith(f"lib/{abi}/") for name in names),
                "Android APK mixes native ABIs")
        for library in libraries:
            # Qt 6.12's vendor-prebuilt libavformat.so has an invalid
            # post-link PT_LOAD mapping. It is not loaded by QtLoader at
            # startup, unlike OpenSSL. Permit only CI test artifacts while
            # investigating FFmpeg separately; never publish a release that
            # silently carries this unresolved 16 KiB mapping defect.
            vendor_ffmpeg_ci_exception = (
                path.name.endswith("-CI-TEST.apk")
                and library == "lib/arm64-v8a/libavformat.so"
            )
            if vendor_ffmpeg_ci_exception:
                print("WARNING: vendor Qt FFmpeg libavformat.so has an "
                      "unresolved 16 KiB LOAD mapping defect (CI test only)")
            if abi == "arm64-v8a":
                validate_android_elf_alignment(
                    library, apk.read(library),
                    check_load_mapping=not vendor_ffmpeg_ci_exception,
                )
            else:
                validate_android_elf32_arm(library, apk.read(library))

        manifest = apk.read("AndroidManifest.xml")
        # Android binary XML can use either UTF-8 or UTF-16LE string pools.
        # Missing INTERNET permission makes a working emulator look offline.
        permission = "android.permission.INTERNET"
        require(permission.encode("utf-8") in manifest
                or permission.encode("utf-16le") in manifest,
                "Android APK missing INTERNET permission")
        require(any(name.endswith(".dex") for name in names), "Android APK missing DEX")
        require(not any(name.startswith("lib/x86") for name in names),
                "Unexpected x86 libraries in ARM-only APK")


def validate_ios(path: Path) -> None:
    """Unsigned iOS CI bundle only. Real device distribution needs Apple signing."""
    require(path.name.endswith("-ios-arm64-unsigned-app.tar.gz"),
            "iOS CI package must be explicitly labeled unsigned")
    with tarfile.open(path, "r:gz") as archive:
        members = archive.getmembers()
        names = {member.name.rstrip("/") for member in members}
        root = "farcaster_client.app"
        require(root + "/Info.plist" in names, "iOS bundle missing Info.plist")
        require(root + "/farcaster_client" in names, "iOS bundle missing executable")
        require(all(not PurePosixPath(name).is_absolute()
                    and ".." not in PurePosixPath(name).parts
                    for name in names), "Unsafe iOS bundle path")


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
        "ios": validate_ios,
        "macos": validate_macos,
        "windows": validate_windows,
        "android": validate_android,
        "web": validate_web,
    }
    validators[platform](path)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("platform", choices=("linux", "macos", "windows", "android", "ios", "web"))
    parser.add_argument("path", type=Path)
    args = parser.parse_args()
    verify(args.platform, args.path)
    print(f"Valid {args.platform} package: {args.path}")


if __name__ == "__main__":
    main()
