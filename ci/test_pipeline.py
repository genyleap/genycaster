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
    def test_native_recovery_session_is_pinned_and_cancelable(self) -> None:
        bridge = Path("src/presentation/qt/presentation_bridge.cpp").read_text(encoding="utf-8")
        wiring = Path("ui/mobile/runtime_ui_bindings.cpp").read_text(encoding="utf-8")
        setup = Path("ui/components/InitialSetup.qml").read_text(encoding="utf-8")
        self.assertIn("void PresentationBridge::beginNativeRecoverySetup(QString recoveryWords)", bridge)
        self.assertIn("emit nativeRecoveryRequested(std::move(recoveryWords));", bridge)
        self.assertIn("Qt.ImhSensitiveData", setup)
        self.assertIn("nativeRecoveryField.clear()", setup)
        self.assertIn("root.nativeRecoverAccount(words)", setup)
        self.assertIn("QFutureWatcher<RecoveryOutcome>", wiring)
        self.assertIn("resolveRecoveryCustodyFid(*runtime->transport, *address)", wiring)
        self.assertIn("startRecoveryChallenge(*runtime->transport, gateway, *fid, *address)", wiring)
        self.assertIn("signOneTimeIdentityProof(mnemonic, fid->value, challenge->message)", wiring)
        self.assertIn("completeRecoveryChallenge(", wiring)
        self.assertIn("nativeRecoveryGeneration->fetch_add", wiring)
        self.assertIn('bridge.authenticationRoute() == QStringLiteral("recovery")', wiring)
        self.assertIn("connections.acceptRecoveryGrant(", wiring)

    def test_native_custody_wordlist_and_module_boundary(self) -> None:
        # The native BIP39 table is generated from the reviewed browser
        # dictionary; never allow a drifted or hand-edited second wordlist.
        import subprocess
        validation = subprocess.run(
            [sys.executable, "ci/generate_bip39_custody_header.py", "--check"],
            capture_output=True, text=True, check=False,
        )
        self.assertEqual(validation.returncode, 0, validation.stderr)
        platform = Path("src/platform/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("farcaster_platform_custody_recovery", platform)
        self.assertIn("custody_recovery.cppm", platform)
        module = Path("src/platform/custody_recovery.cppm").read_text(encoding="utf-8")
        self.assertIn("export module farcaster.platform.custody_recovery;", module)
        self.assertIn("derivePublicCustodyAddress", module)
        self.assertFalse(Path("src/platform/custody_recovery.hpp").exists())

    def test_builtin_provider_linkage_is_explicitly_static(self) -> None:
        # C++26 modules do not implicitly export their C++ definitions from
        # Windows DLLs. Provider implementations linked into the executable
        # must remain static even when BUILD_SHARED_LIBS=ON.
        provider_specs = (
            ("src/providers/compatibility/browser_wallet/CMakeLists.txt",
             "farcaster_provider_compatibility_browser_wallet"),
            ("src/providers/compatibility/farcaster_auth/CMakeLists.txt",
             "farcaster_provider_compatibility_auth"),
            ("src/providers/compatibility/signer_attestation/CMakeLists.txt",
             "farcaster_provider_compatibility_signer_attestation"),
            ("src/providers/openproof_auth/CMakeLists.txt",
             "farcaster_provider_openproof_auth"),
            ("src/providers/emoji/CMakeLists.txt", "farcaster_provider_emoji"),
            ("src/providers/snapchain/CMakeLists.txt", "farcaster_provider_snapchain_read"),
            ("src/providers/snapchain/CMakeLists.txt", "farcaster_provider_snapchain_signer"),
            ("src/providers/snapchain/CMakeLists.txt", "farcaster_provider_snapchain_write"),
        )
        for relative, target in provider_specs:
            source = Path(relative).read_text(encoding="utf-8")
            with self.subTest(target=target):
                self.assertIn(f"add_library({target} STATIC)", source)

    def test_miniapp_url_validation_is_shared_before_platform_dispatch(self) -> None:
        source = Path("ui/mobile/miniapp_host_controller.cpp").read_text(encoding="utf-8")
        start = source.index("void MiniAppHostController::openWithMetadata(")
        end = source.index("void MiniAppHostController::close()", start)
        procedure = source[start:end]
        validation = procedure.index("const QUrl destination{url.trimmed(), QUrl::StrictMode};")
        wasm_dispatch = procedure.index("#if defined(__EMSCRIPTEN__)")
        self.assertLess(validation, wasm_dispatch)
        self.assertEqual(procedure.count("const QUrl destination{"), 1)
        self.assertEqual(procedure.count("QHostAddress numericHost{};"), 1)
        self.assertIn("destination.scheme() != QStringLiteral(\"https\")", procedure)
        # Unverified URL input must never enter the externally visible state.
        self.assertIn("m_launchUrl.clear();", procedure)
        self.assertIn("m_launchUrl = destination.toString(QUrl::FullyEncoded);", procedure)
        self.assertLess(procedure.index("if (!destination.isValid()"),
                        procedure.index("m_launchUrl = destination.toString("))

    def test_external_miniapp_fallback_is_unprivileged_and_explicit(self) -> None:
        controller = Path("ui/mobile/miniapp_host_controller.cpp").read_text(encoding="utf-8")
        header = Path("ui/mobile/miniapp_host_controller.hpp").read_text(encoding="utf-8")
        surface = Path("ui/components/AppsSurface.qml").read_text(encoding="utf-8")
        self.assertIn("if (QDesktopServices::openUrl(destination))", controller)
        self.assertIn("emit externalLaunchSucceeded();", controller)
        self.assertIn("void externalLaunchSucceeded();", header)
        self.assertIn("function onExternalLaunchSucceeded()", surface)
        self.assertIn("root.pendingLaunchApp = null", surface)
        self.assertNotIn('if (!miniAppHost.available && Qt.platform.os !== "wasm")', surface)

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
        # This caught the actual Oct 29 Android startup crash: PatchELF
        # advertised 16 KiB alignment but appended a misaligned PT_LOAD.
        struct.pack_into("<Q", data, 72, 4096)  # p_offset
        with self.assertRaisesRegex(ValueError, "incongruent ELF LOAD"):
            validate_android_elf_alignment("broken-openssl.so", bytes(data))
        struct.pack_into("<Q", data, 72, 0)
        struct.pack_into("<H", data, 18, 62)  # Wrong architecture (x86_64)
        with self.assertRaisesRegex(ValueError, "AArch64"):
            validate_android_elf_alignment("wrong-arch.so", bytes(data))

    def test_windows_deploy_disables_conflicting_vcpkg_applocal(self) -> None:
        workflow = Path(".github/workflows/ci.yml").read_text(encoding="utf-8")
        self.assertIn('"-DVCPKG_APPLOCAL_DEPS=OFF"', workflow)
        self.assertIn("Copy-Item $keychain.FullName dist/app/", workflow)
        self.assertIn("cmake --build build/native --target qt6keychain", workflow)
        self.assertIn("Copy-Item $keychainBuilt.FullName", workflow)
        self.assertIn("Join-Path $qtBin 'qt6keychain.dll'", workflow)
        self.assertIn("Copy-Item $dll.FullName dist/app/", workflow)

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
