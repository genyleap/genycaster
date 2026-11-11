"""Tests for the optional Linux WebEngine SDK preflight."""
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from ci.preflight_linux_miniapp_host import missing_host_files


class LinuxMiniAppSdkPreflightTest(unittest.TestCase):
    def test_empty_sdk_rejects_missing_components_and_runtime(self):
        with tempfile.TemporaryDirectory() as tmp:
            blockers = missing_host_files(Path(tmp))
            self.assertIn("Qt CMake module: Qt6WebChannel", blockers)
            self.assertIn("Qt CMake module: Qt6WebEngineWidgets", blockers)
            self.assertTrue(any("QtWebEngineProcess" in b for b in blockers))
            self.assertTrue(any("ICU" in b for b in blockers))

    def test_complete_fixture_has_no_missing_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for module in ("Qt6Widgets", "Qt6WebChannel", "Qt6WebEngineCore",
                           "Qt6WebEngineWidgets"):
                path = root / "lib" / "cmake" / module / f"{module}Config.cmake"
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
            process = root / "libexec" / "QtWebEngineProcess"
            process.parent.mkdir(parents=True)
            process.touch(mode=0o755)
            process.chmod(0o755)
            resources = root / "resources"
            resources.mkdir()
            (resources / "qtwebengine_resources.pak").touch()
            (resources / "icudtl.dat").touch()
            locales = root / "translations" / "qtwebengine_locales"
            locales.mkdir(parents=True)
            (locales / "en-US.pak").touch()
            self.assertEqual(missing_host_files(root), [])

    def test_missing_locales_remains_fail_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            blockers = missing_host_files(Path(tmp))
            self.assertIn("Qt WebEngine locales (.pak)", blockers)


if __name__ == "__main__":
    unittest.main()
