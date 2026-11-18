#!/usr/bin/env python3
"""Offline regression checks for pinned official Qt WebEngine Extensions."""
from __future__ import annotations

from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from ci.install_qt_webengine_windows import (
    ARCHITECTURES, WEBENGINE_STAMP, checked_archives,
    check_installation, extension_layout,
)


class WindowsWebEngineExtensionTest(unittest.TestCase):
    def test_archives_are_pinned_and_architecture_specific(self):
        for architecture in ARCHITECTURES:
            with self.subTest(architecture=architecture):
                base, archives = extension_layout(architecture)
                self.assertEqual(len(archives), 2)
                self.assertTrue(all(name.startswith(WEBENGINE_STAMP) for name in archives))
                self.assertTrue(all(name.endswith(".7z") for name in archives))
                self.assertIn("/extensions/qtwebengine/6120/61400/", base)
                self.assertIn(ARCHITECTURES[architecture][3], archives[0])
                self.assertEqual(checked_archives(
                    "".join(f'<a href="{name}">archive</a>' for name in archives), archives),
                    archives)
                with self.assertRaisesRegex(ValueError, "Missing pinned"):
                    checked_archives(f'<a href="{archives[1]}">only plugin</a>', archives)
                with self.assertRaisesRegex(ValueError, "Missing pinned"):
                    checked_archives(
                        "".join(f'<a href="{name.replace(WEBENGINE_STAMP, "unreviewed-")}">'
                                'archive</a>' for name in archives), archives)

    def test_sdk_host_fails_closed_on_partial_extension(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaisesRegex(ValueError, "SDK component missing"):
                check_installation(root)
            for component in ("Qt6WebEngineCore", "Qt6WebEngineWidgets", "Qt6WebEngineQuick"):
                directory_path = root / f"lib/cmake/{component}"
                directory_path.mkdir(parents=True, exist_ok=True)
                (directory_path / f"{component}Config.cmake").write_text("# fixture")
            with self.assertRaisesRegex(ValueError, "QtWebEngineProcess"):
                check_installation(root)
            (root / "bin").mkdir()
            (root / "bin/QtWebEngineProcess.exe").write_bytes(b"MZ")
            with self.assertRaisesRegex(ValueError, "Quick QML plugin"):
                check_installation(root)
            plugin = root / "qml/QtWebEngine/qtwebenginequickplugin.dll"
            plugin.parent.mkdir(parents=True)
            plugin.write_bytes(b"MZ")
            check_installation(root)


if __name__ == "__main__":
    unittest.main()
