"""Portable tests for native installers and independent update snapshots."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shutil
import tarfile
from tempfile import TemporaryDirectory
import unittest
import zipfile

from ci.package_linux_deb import build
from ci.package_selfupdate import create_snapshot
from ci.verify_platform_package import verify


class PackagingTests(unittest.TestCase):
    def test_windows_snapshot_contains_verifiable_runtime_files(self) -> None:
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            stage = root / "app"
            stage.mkdir()
            (stage / "Genycaster.exe").write_bytes(b"MZ" + b"release")
            (stage / "Qt6Core.dll").write_bytes(b"qt")
            result = create_snapshot("windows", "arm64", "1.0.0-rc1",
                                     stage, root / "dist")
            self.assertEqual(result.suffix, ".zip")
            with zipfile.ZipFile(result) as archive:
                manifest = json.loads(archive.read("selfupdate.json"))
                self.assertEqual(manifest["installAction"], "unsupported")
                self.assertEqual(manifest["architecture"], "arm64")
                self.assertEqual(manifest["platform"], "windows")
                for entry in manifest["files"]:
                    self.assertEqual(hashlib.sha256(archive.read(entry["path"])).hexdigest(),
                                     entry["sha256"])
            checksum, filename = result.with_suffix(".zip.sha256").read_text().split()
            self.assertEqual(filename, result.name)
            self.assertEqual(checksum, hashlib.sha256(result.read_bytes()).hexdigest())

    def test_macos_snapshot_preserves_app_layout_and_hashes(self) -> None:
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            app = root / "Genycaster.app"
            binary = app / "Contents" / "MacOS" / "Genycaster"
            binary.parent.mkdir(parents=True)
            binary.write_bytes(b"Mach-O")
            result = create_snapshot("macos", "x86_64", "1.0.0-rc1",
                                     app, root / "dist")
            with tarfile.open(result, "r:gz") as archive:
                manifest = json.load(archive.extractfile("selfupdate.json"))
                self.assertEqual(manifest["platform"], "macos")
                self.assertIn("Genycaster.app/Contents/MacOS/Genycaster",
                              {entry["path"] for entry in manifest["files"]})

    @unittest.skipUnless(shutil.which("dpkg-deb"), "dpkg-deb unavailable")
    def test_deb_installer_contains_app_and_desktop_entry(self) -> None:
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            app = root / "AppDir"
            entries = {
                "AppRun": "#!/bin/sh\nexit 0\n",
                "usr/bin/Genycaster": "elf-placeholder",
                "usr/share/applications/genycaster.desktop":
                    "[Desktop Entry]\nType=Application\nName=GenyCaster\n"
                    "Exec=Genycaster\nIcon=genycaster\n",
                "usr/share/icons/hicolor/512x512/apps/genycaster.png": "icon",
            }
            for name, body in entries.items():
                file = app / name
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_text(body)
            (app / "AppRun").chmod(0o755)
            (app / "usr/bin/Genycaster").write_bytes(os.urandom(12288))
            (app / "usr/bin/Genycaster").chmod(0o755)
            result = build(app, root / "dist", "1.0.0-rc1", "arm64")
            self.assertTrue(result.name.endswith("-linux-arm64.deb"))
            verify("linux-deb", result)


if __name__ == "__main__":
    unittest.main()
