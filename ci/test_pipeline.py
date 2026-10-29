"""Portable, offline tests for release gates and artifact packaging."""
from __future__ import annotations

from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch
import sys
import unittest
import zipfile
import struct

from ci.verify_platform_package import validate_android_elf_alignment, verify
from ci.install_qt_windows import STAMP, archives_from_index
from ci.package_web import select_files
from ci.release_artifacts import EXPECTED, main as assemble_release
from ci.release_gate import project_version, release_version


class PipelineTests(unittest.TestCase):
    def test_version_tag_gate(self) -> None:
        version = project_version(Path.cwd())
        self.assertEqual(release_version("refs/heads/main", version), (version, False))
        self.assertEqual(release_version(f"refs/tags/v{version}", version), (version, True))
        for bad in ("refs/tags/v1.2", "refs/tags/latest", "refs/tags/v01.2.3",
                    "refs/tags/v1.2.3-rc1", "refs/tags/v999.0.0"):
            with self.assertRaises(ValueError):
                release_version(bad, version)

    def test_windows_qt_archive_manifest(self) -> None:
        html = f'<a href="{STAMP}qtbase-Windows-MSVC2022.7z">Qt Base</a>'
        self.assertEqual(
            archives_from_index(html, ("qtbase",)),
            [f"{STAMP}qtbase-Windows-MSVC2022.7z"],
        )
        with self.assertRaises(ValueError):
            archives_from_index(html, ("qtbase", "qtdeclarative"))

    def test_web_archive_selection_excludes_build_artifacts(self) -> None:
        with TemporaryDirectory() as path:
            root = Path(path)
            (root / "farcaster_client.html").write_text("<html></html>")
            (root / "farcaster_client.wasm").write_bytes(b"\x00asm")
            (root / "farcaster_client_autogen").mkdir()
            (root / "farcaster_client_autogen" / "secret.js").write_text("ignored")
            (root / "farcaster_client.cpp.o").write_bytes(b"obj")
            selected = {p.name for p in select_files(root)}
            self.assertEqual(selected, {"farcaster_client.html", "farcaster_client.wasm"})

    def test_android_16k_elf_alignment(self) -> None:
        # Synthetic ELF64 AArch64 DSO with a single PT_LOAD program header.
        data = bytearray(128)
        data[:6] = b"\x7fELF\x02\x01"
        struct.pack_into("<H", data, 18, 183)  # EM_AARCH64
        struct.pack_into("<Q", data, 32, 64)   # e_phoff
        struct.pack_into("<H", data, 54, 56)   # e_phentsize
        struct.pack_into("<H", data, 56, 1)    # e_phnum
        struct.pack_into("<I", data, 64, 1)    # PT_LOAD
        struct.pack_into("<Q", data, 112, 16384)  # p_align
        validate_android_elf_alignment("valid.so", bytes(data))

        struct.pack_into("<Q", data, 112, 4096)
        with self.assertRaisesRegex(ValueError, "4 KiB ELF LOAD alignment"):
            validate_android_elf_alignment("invalid.so", bytes(data))

        struct.pack_into("<Q", data, 112, 16384)
        struct.pack_into("<H", data, 18, 62)  # Wrong architecture (x86_64)
        with self.assertRaisesRegex(ValueError, "AArch64"):
            validate_android_elf_alignment("wrong-arch.so", bytes(data))

    def test_windows_portable_zip_structure(self) -> None:
        with TemporaryDirectory() as folder:
            archive = Path(folder) / "genycaster-windows.zip"
            required = (
                "farcaster_client.exe", "Qt6Core.dll", "Qt6Network.dll",
                "qt6keychain.dll", "platforms/qwindows.dll", "libcrypto-3-x64.dll",
                "libssl-3-x64.dll", "libcurl.dll", "sqlite3.dll",
            )
            with zipfile.ZipFile(archive, "w") as package:
                for name in required:
                    package.writestr(name, b"test")
            verify("windows", archive)
            with zipfile.ZipFile(archive, "w") as broken:
                for name in required:
                    if name != "Qt6Network.dll":
                        broken.writestr(name, b"test")
            with self.assertRaisesRegex(ValueError, "qt6network"):
                verify("windows", archive)

    def test_release_package_contract(self) -> None:
        self.assertEqual(len(EXPECTED), 5)
        self.assertEqual(EXPECTED["GenyCaster-android-arm64-v8a"], ".apk")
        self.assertEqual(EXPECTED["GenyCaster-web-wasm"], ".tar.gz")
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            artifacts = root / "artifacts"
            release = root / "release"
            for platform, extension in EXPECTED.items():
                directory = artifacts / platform
                directory.mkdir(parents=True)
                (directory / f"{platform}{extension}").write_bytes(platform.encode())
            with patch.object(sys, "argv", ["release_artifacts.py", str(artifacts), str(release)]):
                assemble_release()
            self.assertEqual(len(list(release.iterdir())), len(EXPECTED) + 1)
            self.assertEqual(len((release / "SHA256SUMS").read_text().splitlines()), len(EXPECTED))


if __name__ == "__main__":
    unittest.main()
