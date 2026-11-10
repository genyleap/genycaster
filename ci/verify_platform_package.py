#!/usr/bin/env python3
"""Validate a packaged GenyCaster app's structure before publishing artifacts.

These checks validate packaging, not code signing, TLS handshakes, startup or
runtime behavior. CI must additionally gate those operations independently.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path, PurePosixPath
import struct
import subprocess
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


def validate_linux_deb(path: Path) -> None:
    require(path.suffix == ".deb", "Linux installer must be a .deb package")
    try:
        result = subprocess.run(
            ["dpkg-deb", "--contents", str(path)],
            capture_output=True, text=True, timeout=30, check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise ValueError("dpkg-deb is required to validate Linux installers") from exc
    require(result.returncode == 0, "Invalid Linux Debian installer")
    listing = result.stdout
    for required in (
        "./opt/genycaster/AppRun",
        "./opt/genycaster/usr/bin/Genycaster",
        "./usr/share/applications/genycaster.desktop",
        "./usr/share/icons/hicolor/512x512/apps/genycaster.png",
        "./opt/genycaster/usr/qml/QtQuick/Controls/qmldir",
        "./opt/genycaster/usr/qml/QtQuick/Controls/libqtquickcontrols2plugin.so",
        "./opt/genycaster/usr/qml/QtQuick/Controls/Basic/qmldir",
        "./opt/genycaster/usr/qml/QtQuick/Controls/Basic/libqtquickcontrols2basicstyleplugin.so",
    ):
        require(required in listing, f"Linux DEB missing {required}")


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
        require(len(members) == len(names) and 0 < len(members) <= 512,
                "Web archive has duplicate names or too many entries")
        require(all(member.isfile() and not member.issym() and not member.islnk()
                    and not PurePosixPath(member.name).is_absolute()
                    and ".." not in PurePosixPath(member.name).parts
                    and not any(part.startswith(".") for part in
                                PurePosixPath(member.name).parts)
                    for member in members), "Web archive contains unsafe entries")
        required = {
            "genycaster.html", "genycaster.js",
            "genycaster.wasm", "genycaster-assets.json",
            "genycaster-assets.js", "genycaster-loader.js",
            "genycaster-sw.js",
        }
        require(required.issubset(names), "Web archive missing required runtime files")

        # Hash every member in bounded chunks, then compare against the public
        # release manifest. Structural tar checks alone would accept a corrupt
        # engine, a stale HTML entry, or an unrelated historical WASM payload.
        digests: dict[str, str] = {}
        sizes: dict[str, int] = {}
        captured: dict[str, bytes] = {}
        for member in members:
            stream = archive.extractfile(member)
            require(stream is not None, f"Unable to read {member.name}")
            sha = hashlib.sha256()
            size = 0
            body = bytearray() if member.name in {
                "genycaster-assets.json", "genycaster-assets.js"} else None
            with stream:
                while data := stream.read(1024 * 1024):
                    size += len(data)
                    sha.update(data)
                    if body is not None:
                        require(size <= 1024 * 1024,
                                "Web release manifest/config too large")
                        body.extend(data)
            digests[member.name] = sha.hexdigest()
            sizes[member.name] = size
            if body is not None:
                captured[member.name] = bytes(body)

        try:
            manifest = json.loads(captured["genycaster-assets.json"])
            files = manifest["files"]
            checksums = manifest["integrity"]
            engine = manifest["wasm"]
            compressed = manifest["wasmGzip"]
        except (KeyError, TypeError, ValueError) as error:
            raise ValueError("Invalid WebAssembly release manifest") from error
        require(isinstance(files, list) and all(isinstance(x, str) for x in files)
                and len(files) == len(set(files)),
                "Web release contains an invalid asset inventory")
        require(names == set(files) | {"genycaster.wasm", "genycaster-assets.json"},
                "Web archive disagrees with its current release inventory")
        require(isinstance(engine, str)
                and re.fullmatch(r"genycaster\.[0-9a-f]{20}\.wasm", engine) is not None
                and isinstance(compressed, str) and compressed == engine + ".gz"
                and {engine, compressed}.issubset(names),
                "Web manifest has an invalid immutable engine pair")
        require(engine.split(".")[1] == digests[engine][:20],
                "Content-addressed WASM filename does not match SHA-256")
        require(digests["genycaster.wasm"] == digests[engine],
                "Stable and immutable WASM engines disagree")
        require(sizes[engine] == manifest.get("wasmBytes")
                and sizes[compressed] == manifest.get("wasmGzipBytes"),
                "WASM engine or gzip sidecar size does not match the manifest")
        require(digests[compressed] == manifest.get("wasmGzipSha256"),
                "WASM gzip sidecar SHA-256 mismatch")
        require(isinstance(checksums, dict)
                and set(checksums) == set(files) - {"genycaster-assets.js", engine}
                and all(isinstance(value, str) and
                        re.fullmatch(r"[0-9a-f]{64}", value)
                        for value in checksums.values()),
                "Web manifest lacks a complete integrity map")
        for name, expected in checksums.items():
            require(digests[name] == expected,
                    f"Web release asset digest mismatch: {name}")

        config = captured["genycaster-assets.js"].decode("utf-8").strip()
        prefix = "window.__genycasterAssets="
        require(config.startswith(prefix) and config.endswith(";"),
                "Browser asset bootstrap is invalid")
        try:
            browser = json.loads(config[len(prefix):-1])
        except ValueError as error:
            raise ValueError("Invalid browser release bootstrap") from error
        for field in ("wasm", "wasmBytes", "wasmGzip", "wasmGzipBytes", "version"):
            require(browser.get(field) == manifest.get(field),
                    f"Browser and release manifest disagree on {field}")


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
            f"lib/{abi}/libgenycaster_{abi}.so",
            f"lib/{abi}/libQt6Core_{abi}.so",
            f"lib/{abi}/libQt6Gui_{abi}.so",
            f"lib/{abi}/libQt6Qml_{abi}.so",
            f"lib/{abi}/libQt6Quick_{abi}.so",
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
        root = "Genycaster.app"
        require(root + "/Info.plist" in names, "iOS bundle missing Info.plist")
        require(root + "/Genycaster" in names, "iOS bundle missing executable")
        require(all(not PurePosixPath(name).is_absolute()
                    and ".." not in PurePosixPath(name).parts
                    for name in names), "Unsafe iOS bundle path")


def windows_installer_entries(path: Path) -> set[str]:
    """Inspect actual NSIS contents with 7-Zip; MZ alone is not an installer."""
    try:
        listing = subprocess.run(
            ["7z", "l", "-slt", str(path)],
            capture_output=True, text=True, timeout=30, check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise ValueError("7-Zip is required to inspect the NSIS installer") from exc
    require(listing.returncode == 0, "Invalid or unreadable Windows NSIS installer")
    require("Type = Nsis" in listing.stdout or "Type = NSIS" in listing.stdout,
            "Windows EXE is not an NSIS installer")
    entries = set()
    for line in listing.stdout.splitlines():
        if line.startswith("Path = "):
            item = line.removeprefix("Path = ").replace("\\", "/")
            entries.add(item)
    return entries


def validate_windows(path: Path) -> None:
    require(path.suffix.lower() == ".exe", "Windows package must be an installable EXE")
    require(path.stat().st_size >= 4096, "Windows installer too small")
    with path.open("rb") as installer:
        require(installer.read(2) == b"MZ", "Windows installer lacks PE header")
    exact_names = windows_installer_entries(path)
    names = {name.lower() for name in exact_names}
    require(any(PurePosixPath(name).name == "Genycaster.exe" for name in exact_names),
            "Windows installer missing correctly cased executable")
    for required in ("qt6core.dll", "qt6gui.dll", "qt6qml.dll",
                     "qt6quick.dll", "qt6multimedia.dll", "qt6network.dll",
                     "qt6keychain.dll"):
        require(any(PurePosixPath(name).name == required for name in names),
                f"Windows installer missing {required}")
    require(any(name.endswith("platforms/qwindows.dll") for name in names),
            "Windows installer missing Qt platform plugin")
    for module, filename in (
        ("qml/qtquick/controls/", "qtquickcontrols2plugin.dll"),
        ("qml/qtquick/controls/basic/", "qtquickcontrols2basicstyleplugin.dll"),
    ):
        require(any(module in name and name.endswith(filename) for name in names),
                f"Windows installer missing packaged QML plugin {filename}")
    require(any(name.endswith("multimedia/ffmpegmediaplugin.dll") for name in names),
            "Windows installer missing Qt Multimedia backend")
    redist = "vc_redist.arm64.exe" if "windows-arm64" in path.name.lower() else "vc_redist.x64.exe"
    require(any(PurePosixPath(name).name == redist for name in names),
            f"Windows installer missing Microsoft compiler runtime: {redist}")
    # CPack must install both publisher/license notices, and SDK-only Qt
    # directories must not inflate the user's application installation.
    for required in ("license.txt", "third_party_notices.md"):
        require(any(PurePosixPath(name).name == required for name in names),
                f"Windows installer missing {required}")
    for sdk in ("include", "lib", "mkspecs", "qmltooling", "bin"):
        require(not any(sdk in PurePosixPath(name).parts for name in names),
                f"Windows installer includes SDK-only directory: {sdk}")
    for needle in ("crypto", "ssl", "curl", "sqlite"):
        require(any(needle in PurePosixPath(name).name and name.endswith(".dll")
                    for name in names),
                f"Windows installer missing {needle} runtime DLL")


PACKAGE_VALIDATORS = {
    "linux": validate_linux,
    "linux-deb": validate_linux_deb,
    "ios": validate_ios,
    "macos": validate_macos,
    "windows": validate_windows,
    "android": validate_android,
    "web": validate_web,
}


def verify(platform: str, path: Path) -> None:
    require(path.is_file() and path.stat().st_size > 0, "Package missing or empty")
    PACKAGE_VALIDATORS[platform](path)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("platform", choices=tuple(PACKAGE_VALIDATORS))
    parser.add_argument("path", type=Path)
    args = parser.parse_args()
    verify(args.platform, args.path)
    print(f"Valid {args.platform} package: {args.path}")


if __name__ == "__main__":
    main()
