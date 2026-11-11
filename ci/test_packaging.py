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

from ci.audit_windows_stage import audit, pe_machine
from ci.package_linux_deb import build
from ci.package_selfupdate import create_snapshot
from ci.verify_platform_package import verify


class PackagingTests(unittest.TestCase):
    @staticmethod
    def fake_pe(machine: int) -> bytes:
        data = bytearray(128)
        data[0:2] = b"MZ"
        data[0x3C:0x40] = (64).to_bytes(4, "little")
        data[64:68] = bytes((80, 69, 0, 0))
        data[68:70] = machine.to_bytes(2, "little")
        return bytes(data)

    def test_windows_arm64_stage_strips_sdk_and_rejects_x64_dlls(self) -> None:
        with TemporaryDirectory() as tmp:
            stage = Path(tmp)
            for name in ("Genycaster.exe", "sqlite3.dll", "Qt6Core.dll", "Qt6Gui.dll",
                         "Qt6Qml.dll", "Qt6Quick.dll", "Qt6Multimedia.dll",
                         "Qt6Network.dll", "qt6keychain.dll",
                         "platforms/qwindows.dll", "multimedia/ffmpegmediaplugin.dll",
                         "qml/Farcaster/Client/plugin.dll",
                         "qml/QtQuick/Controls/qtquickcontrols2plugin.dll",
                         "qml/QtQuick/Controls/Basic/qtquickcontrols2basicstyleplugin.dll"):
                file = stage / name
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_bytes(self.fake_pe(0xAA64))
            for module in ("QtQuick/Controls", "QtQuick/Controls/Basic"):
                (stage / "qml" / module / "qmldir").write_text("module " + module)
            (stage / "vc_redist.arm64.exe").write_bytes(self.fake_pe(0xAA64))
            for directory in ("bin", "include", "lib", "mkspecs", "qmltooling"):
                (stage / directory).mkdir()
                (stage / directory / "unused.txt").write_text("sdk")
            # An unexpected nested runtime DLL is a packaging error, not
            # permission to delete a dependency and ship a broken installer.
            stray = stage / "bin/unresolved-runtime.dll"
            stray.write_bytes(self.fake_pe(0xAA64))
            with self.assertRaisesRegex(ValueError, "Unstaged runtime executable or DLL"):
                audit(stage, "arm64")
            stray.unlink()
            helper = stage / "bin/QtWebEngineProcess.exe"
            helper.write_bytes(self.fake_pe(0xAA64))
            with self.assertRaisesRegex(ValueError, "Unstaged runtime executable or DLL"):
                audit(stage, "arm64")
            helper.unlink()
            self.assertEqual(audit(stage, "arm64"), 14)
            self.assertEqual(pe_machine(stage / "Genycaster.exe"), 0xAA64)
            self.assertTrue((stage / "qml/Farcaster/Client/plugin.dll").exists())
            self.assertFalse((stage / "include").exists())
            self.assertFalse((stage / "bin").exists())
            self.assertEqual(audit(stage, "arm64", clean=False), 14)
            (stage / "Qt6Network.dll").write_bytes(self.fake_pe(0x8664))
            with self.assertRaisesRegex(ValueError, "Wrong Windows PE architecture"):
                audit(stage, "arm64", clean=False)

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

    def test_linux_desktop_entry_has_registered_categories(self) -> None:
        source = Path("properties/Linux/genycaster.desktop").read_text()
        self.assertIn("Categories=Network;InstantMessaging;", source)
        self.assertNotIn("SocialNetwork", source)
        validator = shutil.which("desktop-file-validate")
        if validator:
            import subprocess
            subprocess.run([validator, "properties/Linux/genycaster.desktop"],
                           check=True, capture_output=True, text=True)

    @unittest.skipUnless(shutil.which("dpkg-deb"), "dpkg-deb unavailable")
    def test_deb_installer_contains_app_and_desktop_entry(self) -> None:
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            app = root / "AppDir"
            entries = {
                "AppRun": "#!/bin/sh\nexit 0\n",
                "usr/bin/Genycaster": "elf-placeholder",
                "usr/lib/libstdc++.so.6": "shipped target GCC C++ runtime fixture",
                "usr/share/applications/genycaster.desktop":
                    "[Desktop Entry]\nType=Application\nName=GenyCaster\n"
                    "Exec=Genycaster\nIcon=genycaster\n",
                "usr/share/icons/hicolor/512x512/apps/genycaster.png": "icon",
                "usr/qml/QtQuick/Controls/qmldir": "module QtQuick.Controls",
                "usr/qml/QtQuick/Controls/libqtquickcontrols2plugin.so": "plugin",
                "usr/qml/QtQuick/Controls/Basic/qmldir": "module QtQuick.Controls.Basic",
                "usr/qml/QtQuick/Controls/Basic/libqtquickcontrols2basicstyleplugin.so": "plugin",
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
            import subprocess
            depends = subprocess.run(
                ["dpkg-deb", "-f", str(result), "Depends"],
                capture_output=True, text=True, check=True,
            ).stdout
            self.assertIn("libsqlite3-0", depends)
            # Qt 6's GLVND libOpenGL.so.0 belongs to libopengl0, not libgl1.
            # Without this dependency an otherwise valid DEB cannot launch
            # in a clean Ubuntu 24.04 install.
            self.assertIn("libopengl0", depends)
            self.assertIn("libasound2t64", depends)
            self.assertIn("libpulse0", depends)
            verify("linux-deb", result)
            # Exercise the exact command CI invokes. Calling verify() directly
            # previously hid the missing argparse "linux-deb" choice.
            cli = subprocess.run(
                ["python3", "ci/verify_platform_package.py", "linux-deb",
                 str(result)],
                capture_output=True, text=True, check=False,
            )
            self.assertEqual(cli.returncode, 0, cli.stdout + cli.stderr)
            self.assertIn("Valid linux-deb package:", cli.stdout)


if __name__ == "__main__":
    unittest.main()
