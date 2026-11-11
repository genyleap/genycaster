#!/usr/bin/env python3
"""Regression fixtures: a successful linker is not sufficient for installation."""
from __future__ import annotations

import tempfile
import subprocess
import sys
import unittest
from unittest.mock import patch
from pathlib import Path

from ci.audit_linux_appdir import validate as validate_linux
from ci.prune_linux_qt_plugins import prune
from ci.preflight_linux_qt_runtime import (preflight as preflight_qt_runtime,
                                           REQUIRED_PLUGINS, MEDIA_PLUGINS,
                                           unresolved_sonames)
from ci.audit_macos_bundle import audit as validate_macos
from ci.prune_macos_bundle import prune as prune_macos


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
                    "libQt6Qml.so.6", "libQt6Quick.so.6", "libQt6Multimedia.so.6",
                    "libssl.so.3", "libcrypto.so.3"):
        lib = usr / "lib" / library
        lib.parent.mkdir(parents=True, exist_ok=True)
        lib.write_bytes(b"fixture")
    openssl_backend = usr / "plugins/tls/libqopensslbackend.so"
    openssl_backend.parent.mkdir(parents=True, exist_ok=True)
    openssl_backend.write_bytes(b"fixture")
    qt_config = usr / "bin/qt.conf"
    qt_config.parent.mkdir(parents=True, exist_ok=True)
    qt_config.write_text("[Paths]\nPrefix = ..\nPlugins = plugins\n")
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
    # Explicit runtime and launcher; system libstdc++ must not satisfy a
    # GCC 16 executable when installed on an older clean distribution.
    (usr / "lib/libstdc++.so.6").write_bytes(b"ELF fixture")
    launcher = app / "AppRun"
    launcher.write_text('#!/bin/sh\nLD_LIBRARY_PATH="$(dirname "$0")/usr/lib"\n')
    launcher.chmod(0o755)
    multimedia = usr / "plugins/multimedia/libffmpegmediaplugin.so"
    multimedia.parent.mkdir(parents=True)
    multimedia.write_bytes(b"fixture")


class RuntimePackagingTests(unittest.TestCase):
    def test_windows_signer_uses_native_wincred_without_a_nested_qt_loop(self):
        source = Path("src/platform/signer_vault_qtkeychain.cpp").read_text(
            encoding="utf-8")
        cmake = Path("src/platform/CMakeLists.txt").read_text(encoding="utf-8")
        for snippet in (
            "CredReadW(", "CredWriteW(", "CredDeleteW(",
            "CRED_TYPE_GENERIC", "CRED_PERSIST_ENTERPRISE",
            "windowsLoadSeed(service, fid)", "windowsSaveSeed(service, fid, seed)",
            "windowsDeleteSeed(service, fid)", "qtkeychainCompatNamingScheme",
            "qtkeychainFallbackToCompatNamingScheme",
        ):
            self.assertIn(snippet, source)
        self.assertIn("#if !defined(Q_OS_WIN)\nvoid runJob", source)
        self.assertIn('key + QLatin1Char(\'@\')', source)
        self.assertIn("target_link_libraries(farcaster_platform_signer PRIVATE advapi32)",
                      cmake)

    def test_linux_qt_preflight_collects_all_missing_xcb_sonames(self):
        missing = unresolved_sonames(
            "  libxcb-shape.so.0 => not found\n"
            "  libxcb-randr.so.0 => not found\n"
            "  libQt6Core.so.6 => /opt/Qt/libQt6Core.so.6 (0x0)\n"
        )
        self.assertEqual(missing, {"libxcb-shape.so.0", "libxcb-randr.so.0"})
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            (root / "lib").mkdir()
            for _, relative in (*REQUIRED_PLUGINS, MEDIA_PLUGINS[0]):
                plugin = root / relative
                plugin.parent.mkdir(parents=True, exist_ok=True)
                plugin.write_bytes(b"fixture")

            def fake_inspect(plugin, _env):
                if plugin.name == "libqxcb.so":
                    return ({"libxcb-randr.so.0", "libxcb-shape.so.0"}, "")
                return (set(), "")

            with patch("ci.preflight_linux_qt_runtime.inspect",
                       side_effect=fake_inspect):
                with self.assertRaisesRegex(ValueError,
                                            r"libxcb-randr\.so\.0, libxcb-shape\.so\.0"):
                    preflight_qt_runtime(root)

    def test_linux_qt_preflight_allows_absent_optional_multimedia_backend(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            (root / "lib").mkdir()
            for _, relative in (*REQUIRED_PLUGINS, MEDIA_PLUGINS[0]):
                plugin = root / relative
                plugin.parent.mkdir(parents=True, exist_ok=True)
                plugin.write_bytes(b"fixture")
            with patch("ci.preflight_linux_qt_runtime.inspect",
                       return_value=(set(), "")):
                preflight_qt_runtime(root)

    def test_linux_launcher_handles_unbundled_wayland_without_overriding_user(self):
        from ci.bundle_linux_cpp_runtime import LAUNCHER
        self.assertIn('WAYLAND_DISPLAY', LAUNCHER)
        self.assertIn('QT_QPA_PLATFORM:-', LAUNCHER)
        self.assertIn('export QT_QPA_PLATFORM=xcb', LAUNCHER)
        self.assertIn('usr/plugins/platforms/libqwayland-generic.so', LAUNCHER)

    def test_macos_mounted_dmg_otool_architecture_header(self):
        """otool -L can repeat a mounted DMG path with an architecture label."""
        with tempfile.TemporaryDirectory() as d:
            app = Path(d) / "Genycaster.app"
            fixture_macos(app)
            ffmpeg = app / "Contents/Frameworks/libswresample.7.dylib"
            ffmpeg.write_bytes(b"Mach-O test fixture")

            def fake_macho_tool(command, **_kwargs):
                if command[0] == "/usr/bin/lipo":
                    return subprocess.CompletedProcess(command, 0, "", "")
                self.assertEqual(command[0], "/usr/bin/otool")
                binary = command[-1]
                return subprocess.CompletedProcess(
                    command, 0,
                    f"{binary}:\n"
                    f"{binary} (architecture arm64):\n"
                    "    @rpath/libswresample.7.dylib "
                    "(compatibility version 7.0.0, current version 7.0.0)\n"
                    "    /usr/lib/libSystem.B.dylib "
                    "(compatibility version 1.0.0, current version 1.0.0)\n",
                    "",
                )

            with patch("ci.audit_macos_bundle.subprocess.run",
                       side_effect=fake_macho_tool):
                validate_macos(app, "arm64")

    def test_macos_otool_still_rejects_real_host_dependencies(self):
        """Filtering otool's header must not permit a real SDK-path leak."""
        with tempfile.TemporaryDirectory() as d:
            app = Path(d) / "Genycaster.app"
            fixture_macos(app)

            def fake_macho_tool(command, **_kwargs):
                if command[0] == "/usr/bin/lipo":
                    return subprocess.CompletedProcess(command, 0, "", "")
                self.assertEqual(command[0], "/usr/bin/otool")
                binary = command[-1]
                return subprocess.CompletedProcess(
                    command, 0,
                    f"{binary}:\n"
                    f"{binary} (architecture arm64):\n"
                    "    /Users/runner/work/Qt/libUnbundled.dylib "
                    "(compatibility version 1.0.0, current version 1.0.0)\n",
                    "",
                )

            with patch("ci.audit_macos_bundle.subprocess.run",
                       side_effect=fake_macho_tool):
                with self.assertRaisesRegex(ValueError, "Build-host library leaked"):
                    validate_macos(app, "arm64")

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

    def test_macos_prunes_unreferenced_sql_framework_before_dmg(self):
        with tempfile.TemporaryDirectory() as d:
            app = Path(d) / "Genycaster.app"
            fixture_macos(app)
            sql = app / "Contents/Frameworks/QtSql.framework"
            (sql / "Versions/A").mkdir(parents=True)
            (sql / "Versions/A/QtSql").write_bytes(b"unused Mach-O")
            plugin = app / "Contents/PlugIns/sqldrivers/libqsqlite.dylib"
            plugin.parent.mkdir(parents=True)
            plugin.write_bytes(b"unused plugin")
            storage = app / "Contents/Frameworks/QtQmlLocalStorage.framework"
            storage.mkdir(parents=True)
            local_plugin = app / "Contents/PlugIns/quick/libqmllocalstorageplugin.dylib"
            local_plugin.parent.mkdir(parents=True)
            local_plugin.write_bytes(b"unused localstorage plugin")
            with patch("ci.prune_macos_bundle.subprocess.run",
                       return_value=subprocess.CompletedProcess(
                           ["/usr/bin/otool"], 0,
                           "binary:\\n    @rpath/QtCore.framework/Versions/A/QtCore", "")):
                self.assertEqual(prune_macos(app), 4)
            self.assertFalse(sql.exists())
            self.assertFalse(plugin.exists())
            self.assertFalse(storage.exists())
            self.assertFalse(local_plugin.exists())
            validate_macos(app, "arm64", inspect_macho=False)

    def test_macos_does_not_delete_sql_needed_by_another_plugin(self):
        with tempfile.TemporaryDirectory() as d:
            app = Path(d) / "Genycaster.app"
            fixture_macos(app)
            sql = app / "Contents/Frameworks/QtSql.framework"
            sql.mkdir(parents=True)
            with patch("ci.prune_macos_bundle.subprocess.run",
                       return_value=subprocess.CompletedProcess(
                           ["/usr/bin/otool"], 0,
                           "binary:\\n    @rpath/QtSql.framework/Versions/A/QtSql", "")):
                with self.assertRaisesRegex(ValueError, "would be required"):
                    prune_macos(app)
            self.assertTrue(sql.exists())

    def test_macos_preserves_localstorage_when_imported_by_product(self):
        with tempfile.TemporaryDirectory() as d:
            app = Path(d) / "Genycaster.app"
            fixture_macos(app)
            (app / "Contents/Frameworks/QtSql.framework").mkdir(parents=True)
            qml = Path(d) / "qml"
            qml.mkdir()
            (qml / "Main.qml").write_text("import QtQuick.LocalStorage\\n")
            with self.assertRaisesRegex(ValueError, "imported by"):
                prune_macos(app, qml)
            self.assertTrue((app / "Contents/Frameworks/QtSql.framework").exists())

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

    def test_linux_rejects_unbundled_qt_tls_plugin(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d) / "AppDir"
            fixture_linux(root)
            (root / "usr/plugins/tls/libqopensslbackend.so").unlink()
            with self.assertRaisesRegex(ValueError, "Qt OpenSSL TLS"):
                validate_linux(root, inspect_elf=False)

    def test_linux_rejects_missing_openssl_pair_member(self):
        for soname in ("libssl.so.3", "libcrypto.so.3"):
            with self.subTest(soname=soname), tempfile.TemporaryDirectory() as d:
                root = Path(d) / "AppDir"
                fixture_linux(root)
                (root / "usr/lib" / soname).unlink()
                with self.assertRaisesRegex(ValueError, soname.replace(".", r"\.")):
                    validate_linux(root, inspect_elf=False)

    def test_linux_rejects_missing_or_invalid_qt_conf(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d) / "AppDir"
            fixture_linux(root)
            (root / "usr/bin/qt.conf").write_text("[Paths]\nPlugins = /opt/Qt\n")
            with self.assertRaisesRegex(ValueError, "Qt plugin path"):
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

    def test_linux_audit_cli_imports_sibling_runtime_module(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d) / "AppDir"
            fixture_linux(root)
            result = subprocess.run(
                [sys.executable, "ci/audit_linux_appdir.py", str(root),
                 "--skip-elf"],
                capture_output=True, text=True, timeout=15,
            )
            self.assertEqual(result.returncode, 0,
                             result.stdout + result.stderr)

    def test_linux_rejects_missing_bundled_cpp_runtime(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d) / "AppDir"
            fixture_linux(root)
            (root / "usr/lib/libstdc++.so.6").unlink()
            with self.assertRaisesRegex(ValueError, "target compiler libstdc"):
                validate_linux(root, inspect_elf=False)

    def test_linux_rejects_missing_runtime_launcher(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d) / "AppDir"
            fixture_linux(root)
            (root / "AppRun").unlink()
            with self.assertRaisesRegex(ValueError, "runtime launcher"):
                validate_linux(root, inspect_elf=False)

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
