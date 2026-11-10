#!/usr/bin/env python3
"""Regression tests for release staging, including Qt Creator-only DLLs."""
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from ci.audit_windows_stage import audit, PE_MACHINES


def fake_pe(path: Path, arch: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    header = bytearray(0x90)
    header[:2] = b"MZ"
    header[0x3c:0x40] = (0x80).to_bytes(4, "little")
    header[0x80:0x84] = b"PE\x00\x00"
    header[0x84:0x86] = PE_MACHINES[arch].to_bytes(2, "little")
    path.write_bytes(header)


def stage_dir(root: Path, arch: str) -> None:
    for rel in ("Genycaster.exe", "Qt6Core.dll", "Qt6Gui.dll",
                "Qt6Qml.dll", "Qt6Quick.dll", "Qt6Multimedia.dll",
                "multimedia/ffmpegmediaplugin.dll", "Qt6Network.dll",
                "qt6keychain.dll", "platforms/qwindows.dll"):
        fake_pe(root / rel, arch)
    for module, plugin in (
        ("QtQuick/Controls", "qtquickcontrols2plugin.dll"),
        ("QtQuick/Controls/Basic", "qtquickcontrols2basicstyleplugin.dll"),
    ):
        (root / "qml" / module).mkdir(parents=True)
        (root / "qml" / module / "qmldir").write_text("module QtQuick.Controls")
        fake_pe(root / "qml" / module / plugin, arch)
    (root / ("vc_redist.arm64.exe" if arch == "arm64"
             else "vc_redist.x64.exe")).write_bytes(b"fixture")


class AuditWindowsStageTests(unittest.TestCase):
    def test_removes_only_qt_creator_debugger_tools(self):
        for arch in ("x86_64", "arm64"):
            with self.subTest(arch=arch), tempfile.TemporaryDirectory() as d:
                root = Path(d)
                stage_dir(root, arch)
                fake_pe(root / "qmltooling/qmldbg_debugger.dll", arch)
                fake_pe(root / "qmltooling/qmldbg_messages.dll", arch)
                self.assertGreater(audit(root, arch), 0)
                self.assertFalse((root / "qmltooling").exists())
                self.assertTrue((root / "platforms/qwindows.dll").is_file())

    def test_refuses_unknown_dll_in_sdk_only_path(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            stage_dir(root, "x86_64")
            fake_pe(root / "qmltooling/other.dll", "x86_64")
            with self.assertRaisesRegex(ValueError, "Unstaged runtime"):
                audit(root, "x86_64")

    def test_refuses_unused_qt_sql_drivers(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            stage_dir(root, "x86_64")
            fake_pe(root / "sqldrivers/qsqlpsql.dll", "x86_64")
            with self.assertRaisesRegex(ValueError, "Unused Qt SQL"):
                audit(root, "x86_64")

    def test_refuses_missing_qml_controls_plugin(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            stage_dir(root, "x86_64")
            (root / "qml/QtQuick/Controls/qtquickcontrols2plugin.dll").unlink()
            with self.assertRaisesRegex(ValueError, "QML runtime plugin missing"):
                audit(root, "x86_64")

    def test_refuses_non_debugger_binary_in_other_sdk_directory(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            stage_dir(root, "x86_64")
            fake_pe(root / "bin/not_packaged.dll", "x86_64")
            with self.assertRaisesRegex(ValueError, "Unstaged runtime"):
                audit(root, "x86_64")

    def test_refuses_wrong_architecture_after_cleanup(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            stage_dir(root, "x86_64")
            fake_pe(root / "Qt6Network.dll", "arm64")
            with self.assertRaisesRegex(ValueError, "Wrong Windows PE architecture"):
                audit(root, "x86_64")


if __name__ == "__main__":
    unittest.main()
