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
from ci.install_qt_windows import archive_layout, archives_from_index, STAMP


class WindowsWebEngineExtensionTest(unittest.TestCase):
    def test_base_sdk_includes_official_qt_positioning_for_both_architectures(self):
        for architecture in ARCHITECTURES:
            with self.subTest(architecture=architecture):
                origin, packages = archive_layout(architecture)
                self.assertTrue(origin.startswith("https://download.qt.io/"))
                package = next(key for key in packages if ".qtpositioning." in key)
                self.assertEqual(packages[package], ("qtpositioning",))
                self.assertEqual(
                    archives_from_index(
                        f'<a href="{STAMP}qtpositioning-Windows-ARCH.7z">signed archive</a>',
                        ("qtpositioning",)
                    ),
                    [f"{STAMP}qtpositioning-Windows-ARCH.7z"],
                )

    def test_archives_are_pinned_and_architecture_specific(self):
        for architecture in ARCHITECTURES:
            with self.subTest(architecture=architecture):
                base, components = extension_layout(architecture)
                self.assertIn("/extensions/qtwebengine/6121/61400/", base)
                stamp = WEBENGINE_STAMP + "202611260000"  # synthetic fixture
                archives = tuple(stamp + name + ".7z" for name in components)
                html = "".join(f'<a href="{name}">archive</a>' for name in archives)
                self.assertEqual(checked_archives(html, components), archives)
                with self.assertRaises(ValueError):
                    checked_archives(f'<a href="{archives[1]}">only plugin</a>', components)
                with self.assertRaises(ValueError):
                    checked_archives(html.replace(WEBENGINE_STAMP, "6.12.0-0-"), components)
                with self.assertRaises(ValueError):
                    checked_archives(html.replace(archives[1], archives[1].replace("202611260000", "202611260001")), components)

    def test_webengine_uses_qt_base_installation_root_not_derived_version_folder(self):
        from unittest.mock import patch
        from ci.install_qt_webengine_windows import main, extension_layout
        import sys
        with TemporaryDirectory() as directory:
            root = Path(directory)
            config = root / "lib/cmake/Qt6/Qt6Config.cmake"
            config.parent.mkdir(parents=True)
            config.write_text("# official base Qt marker")
            fake_7z = root / "7z.exe"
            fake_7z.write_bytes(b"fixture")
            _, components = extension_layout("x86_64")
            archives = [WEBENGINE_STAMP + "202611260000" + name + ".7z" for name in components]
            listing = "".join(f'<a href="{name}"></a>' for name in archives)
            with patch.object(sys, "argv", [
                    "install_qt_webengine_windows.py",
                    "--destination", str(root),
                    "--architecture", "x86_64"]), \
                 patch("ci.install_qt_webengine_windows.read_url",
                       return_value=listing.encode()), \
                 patch("ci.install_qt_webengine_windows.shutil.which",
                       return_value=str(fake_7z)), \
                 patch("ci.install_qt_webengine_windows.download_archive"), \
                 patch("ci.install_qt_webengine_windows.subprocess.run"), \
                 patch("ci.install_qt_webengine_windows.check_installation") as verify:
                # Use the actual Qt base archive layout. Any made-up
                # 6.12.0/msvc2022_* subdirectory must fail this regression.
                self.assertEqual(main(), 0)
                verify.assert_called_once_with(root)

    def test_sdk_host_fails_closed_on_partial_extension(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaisesRegex(ValueError, "SDK component missing"):
                check_installation(root)
            for component in ("Qt6WebEngineCore", "Qt6WebEngineWidgets", "Qt6WebEngineQuick"):
                directory_path = root / f"lib/cmake/{component}"
                directory_path.mkdir(parents=True, exist_ok=True)
                (directory_path / f"{component}Config.cmake").write_text("# fixture")
            with self.assertRaisesRegex(ValueError, "Qt6Positioning"):
                check_installation(root)
            positioning = root / "lib/cmake/Qt6Positioning/Qt6PositioningConfig.cmake"
            positioning.parent.mkdir(parents=True, exist_ok=True)
            positioning.write_text("# official base addon")
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
