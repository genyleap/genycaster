#!/usr/bin/env python3
"""Regression fixtures: a successful linker is not sufficient for installation."""
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from ci.audit_linux_appdir import validate as validate_linux
from ci.prune_linux_qt_plugins import prune
from ci.audit_macos_bundle import audit as validate_macos


def fixture_macos(app: Path) -> None:
    contents = app / "Contents"
    exe = contents / "MacOS/Genycaster"
    exe.parent.mkdir(parents=True)
    exe.write_bytes(b"\xcf\xfa\xed\xfe" + b"\x00" * 5000)
    (contents / "Info.plist").write_text("<plist/>")
    for framework in ("QtCore", "QtGui", "QtQml", "QtQuick", "QtQuickControls2", "QtMultimedia"):
        (contents / "Frameworks" / (framework + ".framework")).mkdir(parents=True)
    cocoa = contents / "PlugIns/platforms/libqcocoa.dylib"
    cocoa.parent.mkdir(parents=True)
    cocoa.write_bytes(b"fixture")
    backend = contents / "PlugIns/multimedia/libffmpegmediaplugin.dylib"
    backend.parent.mkdir(parents=True)
    backend.write_bytes(b"fixture")
    for module, plugin in (
        ("QtQuick/Controls", "libqtquickcontrols2plugin.dylib"),
        ("QtQuick/Controls/Basic", "libqtquickcontrols2basicstyleplugin.dylib"),
    ):
        directory = contents / "Resources/qml" / module
        directory.mkdir(parents=True)
        (directory / "qmldir").write_text("module " + module.replace("/", "."))
        (directory / plugin).write_bytes(b"fixture")


def fixture_linux(app: Path) -> None:
    usr = app / "usr"
    executable = usr / "bin/Genycaster"
    executable.parent.mkdir(parents=True)
    executable.write_bytes(b"\x7fELF" + b"\x00" * 200)
    for library in ("libQt6Core.so.6", "libQt6Gui.so.6",
                    "libQt6Qml.so.6", "libQt6Quick.so.6", "libQt6Multimedia.so.6"):
        lib = usr / "lib" / library
        lib.parent.mkdir(parents=True, exist_ok=True)
        lib.write_bytes(b"fixture")
    for module, plugin in (
        ("QtQuick/Controls", "libqtquickcontrols2plugin.so"),
        ("QtQuick/Controls/Basic", "libqtquickcontrols2basicstyleplugin.so"),
    ):
        directory = usr / "qml" / module
        directory.mkdir(parents=True, exist_ok=True)
        (directory / "qmldir").write_text("module " + module.replace("/", "."))
        (directory / plugin).write_bytes(b"fixture")
    platform = usr / "plugins/platforms/libqxcb.so"
    platform.parent.mkdir(parents=True)
    platform.write_bytes(b"fixture")
    multimedia = usr / "plugins/multimedia/libffmpegmediaplugin.so"
    multimedia.parent.mkdir(parents=True)
    multimedia.write_bytes(b"fixture")


class RuntimePackagingTests(unittest.TestCase):
    def test_macos_complete_bundle(self):
        with tempfile.TemporaryDirectory() as d:
            app = Path(d) / "Genycaster.app"
            fixture_macos(app)
            validate_macos(app, "arm64", inspect_macho=False)

    def test_macos_rejects_missing_controls_plugin(self):
        with tempfile.TemporaryDirectory() as d:
            app = Path(d) / "Genycaster.app"
            fixture_macos(app)
            (app / "Contents/Resources/qml/QtQuick/Controls/libqtquickcontrols2plugin.dylib").unlink()
            with self.assertRaisesRegex(ValueError, "qtquickcontrols2plugin"):
                validate_macos(app, "arm64", inspect_macho=False)

    def test_macos_rejects_unused_qt_sql_drivers(self):
        with tempfile.TemporaryDirectory() as d:
            app = Path(d) / "Genycaster.app"
            fixture_macos(app)
            sql = app / "Contents/PlugIns/sqldrivers/libqsqlpsql.dylib"
            sql.parent.mkdir(parents=True)
            sql.write_bytes(b"bad external libpq reference")
            with self.assertRaisesRegex(ValueError, "SQL drivers"):
                validate_macos(app, "arm64", inspect_macho=False)

    def test_macos_rejects_missing_cocoa_plugin(self):
        with tempfile.TemporaryDirectory() as d:
            app = Path(d) / "Genycaster.app"
            fixture_macos(app)
            (app / "Contents/PlugIns/platforms/libqcocoa.dylib").unlink()
            with self.assertRaisesRegex(ValueError, "Cocoa"):
                validate_macos(app, "arm64", inspect_macho=False)

    def test_linux_complete_appdir(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d) / "AppDir"
            fixture_linux(root)
            validate_linux(root, inspect_elf=False)

    def test_linux_removes_only_sql_plugins_before_sealing_appimage(self):
        with tempfile.TemporaryDirectory() as d:
            appdir = Path(d) / "AppDir"
            fixture_linux(appdir)
            sql = appdir / "usr/plugins/sqldrivers/libqsqlpsql.so"
            sql.parent.mkdir(parents=True)
            sql.write_bytes(b"bad dependency")
            with self.assertRaisesRegex(ValueError, "unused Qt SQL"):
                validate_linux(appdir, inspect_elf=False)
            self.assertEqual(prune(appdir), 1)
            validate_linux(appdir, inspect_elf=False)
            self.assertTrue((appdir / "usr/plugins/multimedia/libffmpegmediaplugin.so").exists())

    def test_linux_rejects_missing_controls_plugin(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d) / "AppDir"
            fixture_linux(root)
            (root / "usr/qml/QtQuick/Controls/libqtquickcontrols2plugin.so").unlink()
            with self.assertRaisesRegex(ValueError, "qtquickcontrols2plugin"):
                validate_linux(root, inspect_elf=False)

    def test_linux_rejects_missing_platform_plugin(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d) / "AppDir"
            fixture_linux(root)
            (root / "usr/plugins/platforms/libqxcb.so").unlink()
            with self.assertRaisesRegex(ValueError, "XCB"):
                validate_linux(root, inspect_elf=False)


if __name__ == "__main__":
    unittest.main()
