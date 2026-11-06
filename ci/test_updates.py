"""Portable tests for supervised OS update scripts and Qt resource wiring."""
from __future__ import annotations

import hashlib
import os
from pathlib import Path
import shutil
import subprocess
from tempfile import TemporaryDirectory
import unittest

ROOT = Path(__file__).resolve().parent.parent
LINUX = ROOT / "ui/app/update/scripts/appimage.sh"
MACOS = ROOT / "ui/app/update/scripts/macos.sh"


class UpdateInstallTests(unittest.TestCase):
    def test_native_resources_and_ui_contract(self) -> None:
        source = (ROOT / "ui/app/update_controller.cpp").read_text(encoding="utf-8")
        qt = (ROOT / "ui/CMakeLists.txt").read_text(encoding="utf-8")
        qml = (ROOT / "ui/components/UpdateNoticeDrawer.qml").read_text(encoding="utf-8")
        self.assertIn("launchVerifiedUpdate(", source)
        self.assertIn("installDownloadedUpdate()", source)
        self.assertIn("checksum.addData(&installer)", source)
        self.assertIn("assetPreference(name)", source)
        self.assertIn('-android-") + arch + QStringLiteral(".apk")', source)
        self.assertIn("genycaster_update_scripts", qt)
        for name in ("appimage.sh", "macos.sh"):
            self.assertIn(name, qt)
            self.assertTrue((ROOT / "ui/app/update/scripts" / name).is_file())
        self.assertIn("updateController.installDownloadedUpdate()", qml)
        manifest = (ROOT / "properties/Android/AndroidManifest.xml").read_text(
            encoding="utf-8")
        provider = (ROOT / "properties/Android/res/xml/qtprovider_paths.xml").read_text(
            encoding="utf-8")
        self.assertIn("android.permission.REQUEST_INSTALL_PACKAGES", manifest)
        self.assertIn('android:authorities="${applicationId}.qtprovider"', manifest)
        self.assertIn('android:resource="@xml/qtprovider_paths"', manifest)
        self.assertIn('<files-path name="files" path="."', provider)


    @unittest.skipUnless(shutil.which("sh"), "Shell unavailable")
    def test_posix_install_scripts_parse(self) -> None:
        for script in (LINUX, MACOS):
            with self.subTest(script=script.name):
                subprocess.run(["sh", "-n", str(script)], check=True, timeout=4)

    @unittest.skipUnless(os.name == "posix" and Path("/usr/bin/sha256sum").exists(),
                         "Linux SHA-256 utilities unavailable")
    def test_appimage_supervised_success_and_digest_rejection(self) -> None:
        with TemporaryDirectory() as tmp:
            base = Path(tmp)
            target = base / "GenyCaster.AppImage"
            package = base / "NewRelease.AppImage"
            old = "#!/bin/sh\necho old\n"
            new = "#!/bin/sh\nsleep 5\n"
            target.write_text(old, encoding="utf-8")
            package.write_text(new, encoding="utf-8")
            target.chmod(0o755)
            package.chmod(0o755)
            digest = hashlib.sha256(package.read_bytes()).hexdigest()
            args = ["sh", str(LINUX), "99999999", str(package),
                    str(target), digest]
            wrong = subprocess.run(args[:-1] + ["0" * 64],
                                   capture_output=True, timeout=6, check=False)
            self.assertNotEqual(wrong.returncode, 0)
            self.assertEqual(target.read_text(), old)
            applied = subprocess.run(args, capture_output=True, timeout=12, check=False)
            self.assertEqual(applied.returncode, 0, applied.stderr.decode())
            self.assertEqual(target.read_text(), new)

    @unittest.skipUnless(os.name == "posix" and Path("/usr/bin/sha256sum").exists(),
                         "Linux SHA-256 utilities unavailable")
    def test_appimage_failed_launch_rolls_back(self) -> None:
        with TemporaryDirectory() as tmp:
            base = Path(tmp)
            target = base / "GenyCaster.AppImage"
            package = base / "Broken.AppImage"
            old = "#!/bin/sh\necho old\n"
            broken = "#!/bin/sh\nexit 17\n"
            target.write_text(old, encoding="utf-8")
            package.write_text(broken, encoding="utf-8")
            target.chmod(0o755)
            package.chmod(0o755)
            digest = hashlib.sha256(package.read_bytes()).hexdigest()
            result = subprocess.run(
                ["sh", str(LINUX), "99999999", str(package), str(target), digest],
                capture_output=True, timeout=12, check=False)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(target.read_text(), old)


if __name__ == "__main__":
    unittest.main()
