#!/usr/bin/env python3
"""Fast guard for accidental per-commit CI/release triggers and release bypasses."""
from __future__ import annotations

from pathlib import Path
import re


def main() -> None:
    workflow = Path(".github/workflows/ci.yml").read_text(encoding="utf-8")
    assert re.search(r"^on:\n  workflow_dispatch:", workflow, re.M)
    assert "default: web" in workflow
    assert not re.search(r"^  core:\\n", workflow, re.M)
    assert "confirm_full_matrix:" in workflow
    assert "GENYCASTER_FULL_MATRIX_CONFIRMED" in workflow
    # Hosted Actions only build/package; development tests run on the server.
    for test_command in ("node --test", "ctest --test-dir",
                         "python3 -m unittest", "cmake -S tests/"):
        assert test_command not in workflow, test_command
    assert "cancel-in-progress: ${{ github.event_name == 'workflow_dispatch' }}" in workflow
    # Release-gate checks prevent accidental tag or main-branch bypass.
    gate = workflow.split("  gate:", 1)[1].split("  linux:", 1)[0]
    assert "ci/release_gate.py" in gate
    assert "git merge-base --is-ancestor" in gate
    assert "GENYCASTER_FULL_MATRIX_CONFIRMED" in gate
    # Every ordinary main commit remains exempt from the expensive matrix.
    assert re.search(r"^  push:\n    tags:\n      - 'v\*'", workflow, re.M)
    assert not re.search(r"^  (pull_request|schedule|release):", workflow, re.M)
    assert not re.search(r"^    (branches|paths):", workflow, re.M)
    assert re.search(r"^  publish:\n", workflow, re.M)
    assert "github.event_name == 'push' && needs.gate.outputs.release == 'true'" in workflow
    assert "needs: [gate, linux, macos, windows, android, web]" in workflow
    assert "FARCASTER_REQUIRE_SECURE_SIGNER_VAULT=ON" in workflow
    assert "linux_gcc_arm64" in workflow
    assert "macos-26-intel" in workflow
    assert "runner: macos-26" in workflow
    # A job-level if is evaluated *before* matrix expansion. When the
    # platform is skipped, matrix expressions in its job name are displayed
    # literally in GitHub's UI. Leave matrix values to the job instances and
    # artifact names, and keep job-level display names stable.
    for name in ("Linux AppImage and DEB", "macOS DMG",
                 "Windows NSIS installer", "Android APK",
                 "NixOS native package"):
        assert f"    name: {name}" in workflow
    assert not re.search(r"^    name: .*\\$\\{\\{.*matrix\\.", workflow, re.M)
    # A completed macOS ARM64 link was previously rejected by a malformed
    # lipo invocation. Keep the executable before -verify_arch.
    assert '/usr/bin/lipo "$APP/Contents/MacOS/Genycaster" -verify_arch "${{ matrix.arch }}"' in workflow
    assert '/usr/bin/lipo -verify_arch' not in workflow
    assert '/usr/bin/lipo "$APP/Genycaster" -verify_arch arm64' in workflow
    # Homebrew macOS-26 OpenSSL static objects cannot safely target macOS 15.
    # Match both release architectures to source-built, SHA-verified static
    # libraries with the same minimum deployment target as the application.
    assert "Build OpenSSL 3.5.9 for macOS 15 deployment" in workflow
    assert "603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a" in workflow
    assert "MACOSX_DEPLOYMENT_TARGET=15.0" in workflow
    assert 'export CFLAGS="-O2 -mmacosx-version-min=15.0"' in workflow
    assert 'darwin64-${{ matrix.arch }}-cc' in workflow
    assert '-DOPENSSL_ROOT_DIR="$GITHUB_WORKSPACE/build/ci-openssl/install"' in workflow
    assert '-DOPENSSL_INCLUDE_DIR="$GITHUB_WORKSPACE/build/ci-openssl/install/include"' in workflow
    assert '-DOPENSSL_CRYPTO_LIBRARY="$GITHUB_WORKSPACE/build/ci-openssl/install/lib/libcrypto.a"' in workflow
    assert '-DOPENSSL_SSL_LIBRARY="$GITHUB_WORKSPACE/build/ci-openssl/install/lib/libssl.a"' in workflow
    assert "OPENSSL_CRYPTO_LIBRARY:[^=]*=" in workflow
    assert "OPENSSL_SSL_LIBRARY:[^=]*=" in workflow
    # Qt Multimedia's GStreamer backend must resolve native runtime
    # dependencies before compiling and packaging costly Linux artifacts.
    assert "libgstreamer-plugins-bad1.0-0" in workflow
    linux = workflow.split("  linux:", 1)[1].split("  macos:", 1)[0]
    assert linux.index("Check Qt Multimedia deployment dependencies") < linux.index("Build Qt client")
    assert 'ldd "$GST_QT_PLUGIN"' in linux
    assert "libgstplay-1.0.so.0" in linux
    # A desktop install must not be a portable bundle masquerading as setup.
    # Verify per-user activation, desktop shortcuts, publisher metadata, and
    # architecture hygiene independently from the cross-platform build.
    instance = Path("ui/app/desktop/instance.cpp").read_text(encoding="utf-8")
    desktop_module = Path("ui/app/desktop/instance.cppm").read_text(encoding="utf-8")
    assert "export module genycaster.ui.desktop.instance;" in desktop_module
    assert "import genycaster.ui.desktop.instance;" in Path("ui/app/main.cpp").read_text(encoding="utf-8")
    entry = Path("ui/app/main.cpp").read_text(encoding="utf-8")
    windows = Path("properties/Windows/Configure.cmake").read_text(encoding="utf-8")
    rc = Path("properties/Windows/Genycaster.rc.in").read_text(encoding="utf-8")
    for token in ("QLockFile", "QLocalServer", "QLocalSocket",
                  "UserAccessOption", "requestActivate()", "showNormal()"):
        assert token in instance, token
    # Non-QObject helpers are named modules with consistent lowercase names.
    for relative, name in (
        ("bootstrap/options", "genycaster.ui.bootstrap"),
        ("bootstrap/fonts", "genycaster.ui.bootstrap.fonts"),
        ("bootstrap/providers", "genycaster.ui.bootstrap.providers"),
        ("bootstrap/profiles", "genycaster.ui.bootstrap.profiles"),
        ("bootstrap/routes", "genycaster.ui.bootstrap.routes"),
        ("bootstrap/auth", "genycaster.ui.bootstrap.auth"),
        ("bootstrap/worker", "genycaster.ui.bootstrap.worker"),
        ("bootstrap/teardown", "genycaster.ui.bootstrap.teardown"),
        ("web/wheel", "genycaster.ui.web.wheel"),
        ("update/install", "genycaster.ui.update.install"),
        ("desktop/instance", "genycaster.ui.desktop.instance"),
        ("diagnostics/snapshots", "genycaster.ui.diagnostics.snapshots"),
        ("diagnostics/smoke", "genycaster.ui.diagnostics.smoke"),
    ):
        interface = Path("ui/app") / (relative + ".cppm")
        implementation = Path("ui/app") / (relative + ".cpp")
        assert interface.is_file() and implementation.is_file(), relative
        assert f"export module {name};" in interface.read_text(encoding="utf-8")
        assert f"module {name};" in implementation.read_text(encoding="utf-8")
        assert not (Path("ui/app") / (relative + ".hpp")).exists()
        assert interface.name == interface.name.lower()
        assert implementation.name == implementation.name.lower()
        # Every named-module interface must retain the canonical project
        # documentation contract, not an ad-hoc one-line comment.
        source = interface.read_text(encoding="utf-8")
        assert source.startswith("/*!\n"), relative
        for tag in ("@file", "@brief", "@details", "@author", "@since",
                    "@copyright", "@license"):
            assert tag in source.split("*/", 1)[0], (relative, tag)
        assert f"@file        ui/app/{relative}.cppm" in source, relative
        assert "Copyright (c) 2026 Genyleap." in source, relative
        assert "GPL-3.0-only" in source, relative
        header = source.split("*/", 1)[0]
        assert "@author      Kambiz Asadzadeh" in header, relative
        assert re.search(r"@since\s+[0-9]{2} [A-Za-z]{3} [0-9]{4}", header), relative
        assert not re.search(r'^\\s*#include\\s+"\\.\\./', implementation.read_text(encoding="utf-8"), re.M), relative
        if relative != "bootstrap/worker":  # pure C++ worker has no Qt ABI
            assert '#include "qt_fwd.hpp"' in interface.read_text(encoding="utf-8")
        assert '#include <QString>' not in interface.read_text(encoding="utf-8")
    assert 'import genycaster.ui.bootstrap.fonts;' in entry
    assert 'import genycaster.ui.bootstrap.providers;' in entry
    assert 'import genycaster.ui.bootstrap.profiles;' in entry
    assert 'import genycaster.ui.bootstrap.routes;' in entry
    assert 'import genycaster.ui.bootstrap.auth;' in entry
    assert 'bootstrap::loadProviderRoutes(' in entry
    assert 'bootstrap::loadAuthenticationOptions(' in entry
    routes_source = Path("ui/app/bootstrap/routes.cpp").read_text(encoding="utf-8")
    auth_source = Path("ui/app/bootstrap/auth.cpp").read_text(encoding="utf-8")
    assert "Selected protocol has no compatible canonical message writer" in routes_source
    assert "signerAttestationUrlText" not in entry
    assert "Signer attestation URL must be a credential-free HTTP(S) service" in auth_source
    for test_source in ("tests/ui/provider_routes_tests.cpp",
                        "tests/ui/bootstrap_auth_tests.cpp"):
        assert Path(test_source).is_file(), test_source
    # Production secure signer support is not optional in release packaging.
    signer_cmake = Path("src/platform/CMakeLists.txt").read_text(encoding="utf-8")
    assert "libsecret-1-dev" in signer_cmake
    assert "pkg_check_modules(GENYCASTER_QTKEYCHAIN_LIBSECRET QUIET libsecret-1)" in signer_cmake
    for guarded_header in (
        "ui/app/browser_signer_bridge.hpp",
        "ui/app/browser_cast_payload.hpp",
        "ui/app/browser_emoji_recovery.hpp",
    ):
        header_text = Path(guarded_header).read_text(encoding="utf-8")
        assert header_text.index("#include <QtGlobal>") < header_text.index(
            "#if defined(Q_OS_WASM)"), guarded_header
    assert 'bootstrap::loadProviderProfiles(' in entry
    assert Path("tests/ui/provider_profiles_tests.cpp").is_file()
    assert 'GENYCASTER_READ_PROVIDER_API_KEYS_JSON' in Path(
        "ui/app/bootstrap/profiles.cpp").read_text(encoding="utf-8")
    assert 'GENYCASTER_READ_PROVIDER_API_KEYS_JSON' not in entry
    # Qt's platform abstraction is the source of truth for Qt-facing
    # translation units. Do not use a compiler vendor macro for UI policy.
    for directory in (Path("ui/app"), Path("src/presentation/qt")):
        for qt_source in directory.rglob("*"):
            if qt_source.suffix not in (".cpp", ".hpp", ".cppm"):
                continue
            assert "__EMSCRIPTEN__" not in qt_source.read_text(
                encoding="utf-8"), qt_source

    assert 'import genycaster.ui.bootstrap.worker;' in entry
    assert "bootstrap.start(std::move(bootstrapRuntime));" in entry
    assert "bootstrap.join();" in entry
    assert 'import genycaster.ui.bootstrap.teardown;' in entry
    assert "bootstrap::releaseQmlRoots(engine, mainWindow);" in entry
    assert entry.index("shutdownScope.stop();") < entry.index("bootstrap.join();") < entry.index(
        "bootstrap::releaseQmlRoots(engine, mainWindow);")
    assert "QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete)" in Path(
        "ui/app/bootstrap/teardown.cpp").read_text(encoding="utf-8")
    assert 'std::thread worker' not in entry
    assert 'RenderingController::canonicalProfile' not in Path(
        "ui/app/bootstrap/options.cpp").read_text(encoding="utf-8")
    assert "RenderingController::canonicalProfile" in Path(
        "ui/app/bootstrap/rendering.cpp").read_text(encoding="utf-8")
    assert Path("tests/ui/provider_endpoints_tests.cpp").is_file()
    assert 'import genycaster.ui.diagnostics.snapshots;' in entry
    assert 'import genycaster.ui.diagnostics.smoke;' in entry
    assert "fontFamilies.forLanguage(" in entry
    assert "diagnostics::scheduleSnapshots(" in entry
    assert "diagnostics::configureSmokeTests(" in entry
    assert 'import genycaster.ui.bootstrap;' in entry
    assert 'import genycaster.ui.web.wheel;' in entry
    assert 'import genycaster.ui.update.install;' in Path(
        "ui/app/update_controller.cpp").read_text(encoding="utf-8")
    assert Path("ui/app/modules/qt_fwd.hpp").is_file()
    assert not Path("ui/app/web/Wheel.hpp").exists()
    assert not Path("ui/app/bootstrap/Options.hpp").exists()
    assert not Path("ui/app/browser_wheel_fallback.hpp").exists()
    assert "desktopInstance.acquire(" in entry
    assert "desktopInstance.setWindow(mainWindow)" in entry
    assert "desktopInstance.setOpenUrlHandler(" in entry
    assert 'optionValue(arguments, QStringLiteral("--open-url="))' in entry
    assert "QJsonDocument" in instance
    assert "setOpenUrlHandler(" in instance
    assert 'QStringLiteral("https")' in instance
    assert 'CPACK_NSIS_EXECUTABLES_DIRECTORY "."' in windows
    assert 'CPACK_NSIS_INSTALLED_ICON_NAME "Genycaster.exe"' in windows
    assert "string(CONCAT CPACK_NSIS_EXTRA_INSTALL_COMMANDS" in windows
    assert "vc_redist.${_genycasterVCRedistSuffix}.exe" in windows
    assert "ExecWait" in windows and "SetRebootFlag true" in windows
    assert "Get-AuthenticodeSignature -FilePath" in workflow
    assert "CPACK_NSIS_CREATE_ICONS_EXTRA" in windows
    assert "CPACK_NSIS_DELETE_ICONS_EXTRA" in windows
    assert "GenyCaster.lnk" in windows
    assert "GenyCaster-Installer-License.txt" in windows
    assert "CPACK_RESOURCE_FILE_LICENSE" in windows
    assert 'RENAME "LICENSE.txt"' in windows
    assert "THIRD_PARTY_NOTICES.md" in windows
    assert 'VALUE "LegalCopyright"' in rc
    assert 'VALUE "LegalTrademarks"' in rc
    assert "python ci/audit_windows_stage.py dist/app" in workflow
    updates = Path("ui/app/update_controller.cpp").read_text(encoding="utf-8")
    assert "checksum.addData(&installer)" in updates
    assert "Cached update failed SHA-256 verification" in updates
    assert "QCoreApplication::quit();" in updates
    assert "token: ${{ github.token }}" in workflow
    assert "prerelease: ${{ contains(github.ref_name, '-') }}" in workflow
    assert "windows-11-arm" in workflow
    assert "android_armv7" in workflow
    assert "name: iOS arm64 unsigned device app" in workflow
    assert "name: NixOS" in workflow
    assert "GenyCaster-linux-arm64" in workflow
    assert "GenyCaster-windows-arm64" in workflow
    assert "GenyCaster-android-armeabi-v7a" in workflow
    assert "            vcpkg_triplet: x64-windows" in workflow
    assert "            vcpkg_triplet: arm64-windows" in workflow
    assert "            vcpkg_triplet: ${{ matrix.vcpkg_triplet }}" not in workflow
    assert "ANDROID_KEYSTORE_BASE64" in workflow
    assert "if-no-files-found: error" in workflow
    assert "Refuse overwriting an existing release" in workflow
    assert "ci/release_gate.py" in workflow
    assert "ci/release_artifacts.py" in workflow
    assert not Path("tools").exists(), "legacy tools/ directory must not return"
    assert not Path("docs-site").exists(), "obsolete docs-site/ directory must not return"
    assert not Path("prototypes").exists(), "experimental prototypes must not return"
    assert not Path("ui/mobile").exists(), "cross-platform Qt application sources must live in ui/app"
    assert not Path("ui/demo").exists(), "unused sample QML must not re-enter the shipped module"
    assert Path("ui/app/Main.qml").is_file(), "unified Qt application entrypoint is required"
    assert Path("ui/app/main.cpp").is_file(), "unified native application entrypoint is required"
    public_root_docs = {
        "README.md", "AGENTS.md", "CHANGELOG.md", "CODE_OF_CONDUCT.md",
        "CONTRIBUTING.md", "SECURITY.md", "STATUS.md", "THIRD_PARTY_NOTICES.md",
    }
    assert {p.name for p in Path(".").glob("*.md")} == public_root_docs, (
        "Keep root Markdown limited to canonical public and engineering entry points"
    )
    assert Path("docs/12-roadmap.md").is_file(), "development stage plan is required"
    assert "tools/linuxdeploy" not in workflow
    assert "build/ci-linuxdeploy/" in workflow
    assert "version=3.5.9" in workflow
    assert "tools/build_android_openssl.sh" not in workflow
    assert "Prepare Android OpenSSL 3.5.9 libraries" in workflow
    assert "$ANDROID_HOME/cmdline-tools/latest/bin" in workflow
    assert "$ANDROID_HOME/cmdline-ci" not in workflow
    assert "sdkmanager_bin/sdkmanager" in workflow
    assert 'export LDFLAGS="-Wl,-z,max-page-size=16384' in workflow
    # OpenSSL must emit the final Android SONAMEs itself; post-link
    # PatchELF corrupted DT_VERNEED/PT_LOAD and caused QtLoader startup crashes.
    assert '"android-genycaster-arm64" =>' in workflow
    assert 'inherit_from => [ "android-arm64" ]' in workflow
    assert 'shlib_variant => "_3"' in workflow
    assert './Configure "android-genycaster-${{ matrix.openssl_arch }}" shared no-tests' in workflow
    assert '"android-genycaster-arm" =>' in workflow
    assert "arm64-v8a" in workflow and "armeabi-v7a" in workflow
    assert "patchelf --set-soname" not in workflow
    assert "patchelf --page-size" not in workflow
    assert 'llvm-readelf" -d "$libdir/$lib" > "$libdir/$lib.dynamic.txt"' in workflow
    assert 'grep -Fq "Library soname: [$lib]" "$libdir/$lib.dynamic.txt"' in workflow
    assert "grep -Fq 'Shared library: [libcrypto_3.so]' " in workflow
    assert '"$ANDROID_HOME/build-tools/35.0.0/zipalign" -c -P 16' in workflow
    assert "validate_android_elf_alignment" in Path(
        "ci/verify_platform_package.py"
    ).read_text(encoding="utf-8")
    android_options = Path("cmake/platform/Android/configure.cmake").read_text(encoding="utf-8")
    assert "add_link_options(" in android_options
    assert "max-page-size=16384" in android_options
    assert "genycaster_android_apply_toolchain_options()" in Path("CMakeLists.txt").read_text(encoding="utf-8")
    assert "$ANDROID_HOME/build-tools/35.0.0/apksigner" in workflow
    assert "genycaster-ci-test-signed.apk" in workflow
    assert '"$signer" verify --verbose --print-certs "$selected"' in workflow
    assert "sha256sum --check" in workflow
    for module_path, target in (
        ("src/presentation/media/CMakeLists.txt", "farcaster_presentation_media"),
        ("src/providers/hypersnap/CMakeLists.txt", "farcaster_provider_hypersnap"),
        ("src/providers/public_data/CMakeLists.txt", "farcaster_provider_public_data"),
    ):
        module_cmake = Path(module_path).read_text(encoding="utf-8")
        assert f"add_library({target} STATIC)" in module_cmake, (
            f"{target} must retain its implementation symbols on Windows"
        )
    print("CI trigger, source layout and release gate invariants passed")


if __name__ == "__main__":
    main()
