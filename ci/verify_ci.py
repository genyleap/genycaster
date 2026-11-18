#!/usr/bin/env python3
"""Fast guard for accidental per-commit CI/release triggers and release bypasses."""
from __future__ import annotations

from pathlib import Path
import re


# Job-level `if` is evaluated before matrix expansion; these must fail closed.
FORBIDDEN_CORE_JOB = re.compile(r"^  core:\s*$", re.M)
MATRIX_JOB_DISPLAY_NAME = re.compile(r"^    name: .*\$\{\{\s*matrix\.", re.M)
PARENT_DIRECTORY_INCLUDE = re.compile(r'^\s*#include\s+"\.\./', re.M)


def main() -> None:
    workflow = Path(".github/workflows/ci.yml").read_text(encoding="utf-8")
    assert re.search(r"^on:\n  workflow_dispatch:", workflow, re.M)
    # Release-candidate validation runs one coordinated matrix, not one
    # GitHub Actions invocation per platform (GenyConnect-style orchestration).
    dispatch = workflow.split("  workflow_dispatch:\\n", 1)[1].split("  push:\\n", 1)[0]
    assert re.search(r"(?m)^        default: all$\\n        options:$\\n          - all$", dispatch)
    assert "confirm_full_matrix:" in dispatch
    assert not FORBIDDEN_CORE_JOB.search(workflow)
    assert "confirm_full_matrix:" in workflow
    assert "GENYCASTER_FULL_MATRIX_CONFIRMED" in workflow
    # A bounded security gate runs before expensive hosted builds; large
    # native and integration test suites continue to run separately.
    gate = workflow.split("  gate:", 1)[1].split("  linux:", 1)[0]
    for test_command in ("ctest --test-dir", "cmake -S tests/"):
        # Expensive native compilation belongs to platform jobs after the
        # reusable gate, not in the low-cost security gate itself.
        assert test_command not in gate, test_command
    assert "python3 -m unittest ci/test_deployment_config.py" in gate
    assert "node --test services/test/deployment_config.test.mjs" in gate
    assert "ci/test_audit_windows_stage.py" in gate
    assert "ci/test_native_runtime_audits.py" in gate
    assert "ci/test_nsis_cpack_config.py" in gate
    assert "ci/test_packaging.py" in gate
    assert "ci/test_linux_cpp_runtime.py" in gate
    assert "ci/test_linux_release_parity.py" in gate
    assert "ci/test_linux_distro_assets.py" in gate
    assert "ci/test_macos_otool_headers.py" in gate
    assert "ci/test_release_feature_contract.py" in gate
    assert "ci/test_macos_signing_policy.py" in gate
    assert "ci/test_platform_arm64_dispatch.py" in gate
    assert "ci/verify_release_feature_contract.py --strict" in gate
    assert "ci/bundle_linux_cpp_runtime.py AppDir --compiler g++-16" in workflow
    assert "ci/preflight_linux_qt_runtime.py --qt-root" in workflow
    linux_job = workflow.split("  linux:", 1)[1].split("  macos:", 1)[0]
    assert linux_job.index("ci/bundle_linux_cpp_runtime.py AppDir") < linux_job.index("--output appimage")
    assert linux_job.count("ci/audit_linux_appdir.py AppDir") == 2
    assert "ci/preflight_linux_miniapp_host.py --qt-root" in linux_job
    assert "ci/stage_linux_webengine.py AppDir" in linux_job
    assert "ci/bundle_linux_webengine_runtime.py AppDir" in linux_job
    assert linux_job.count("--require-miniapps") >= 2
    assert "-DFARCASTER_ENABLE_MINIAPPS=ON" in linux_job
    assert "-DGENYCASTER_REQUIRE_LINUX_MINIAPP_HOST=ON" in linux_job
    assert '"dist/$OUTPUT"' in linux_job
    assert "GENYCASTER_APPIMAGE" in Path("ci/smoke_linux_install.sh").read_text()
    assert "desktop-file-validate AppDir/usr/share/applications/genycaster.desktop" in workflow
    assert '-qmldir="$QML_SCAN"' in workflow
    assert 'rm -f "$QML_SCAN/app/miniapp/EmbeddedBrowser.qml"' in workflow
    assert '--qmldir $scan --dir dist/app' in workflow
    # Platform-specific browser QML is explicitly excluded from macdeployqt's
    # cross-platform scan; the signed-off path is asserted above.
    assert "app/miniapp/EmbeddedBrowser.qml" in workflow
    assert 'mv "$SQL_DRIVERS" "$RUNNER_TEMP/genycaster-unused-qt-sqldrivers"' in workflow
    assert '--skip-plugin-types sqldrivers,qmltooling' in workflow
    assert 'ci/prune_linux_qt_plugins.py AppDir' in workflow
    assert workflow.index("ci/prune_linux_qt_plugins.py AppDir") < workflow.index("--output appimage")
    assert "ci/audit_macos_bundle.py" in workflow
    assert 'ci/audit_macos_bundle.py "$MOUNT/Genycaster.app"' in workflow
    assert "EXCLUDE_BY_TYPE sqldrivers" in Path("ui/CMakeLists.txt").read_text()
    assert "--skip-plugin-types sqldrivers,qmltooling" in workflow
    assert 'ci/audit_linux_appdir.py AppDir' in workflow
    assert 'bash ci/smoke_linux_install.sh' in workflow
    assert 'ci/smoke_windows_stage.ps1 -Stage dist/app' in workflow
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
    macos_job = workflow.split("  macos:", 1)[1].split("  windows:", 1)[0]
    assert "-DFARCASTER_ENABLE_MINIAPPS=ON" in macos_job
    assert "--target farcaster_miniapp_browser" in macos_job
    # macOS 15/Xcode 16.4 Apple SDK headers contain legacy enum arithmetic
    # rejected in C++26 mode. Preserve the C++26 native WebKit boundary on
    # the macOS 26/Xcode 26 SDK/compiler pair until Apple fixes older SDKs.
    assert '"runner":"macos-26"' in workflow
    assert "inputs.target_arch == 'x86_64'" in workflow
    # A job-level if is evaluated *before* matrix expansion. When the
    # platform is skipped, matrix expressions in its job name are displayed
    # literally in GitHub's UI. Leave matrix values to the job instances and
    # artifact names, and keep job-level display names stable.
    for name in ("Linux AppImage and DEB", "macOS DMG",
                 "Windows NSIS installer", "Android APK",
                 "NixOS native package"):
        assert f"    name: {name}" in workflow
    assert not MATRIX_JOB_DISPLAY_NAME.search(workflow)
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
    assert 'export CFLAGS="-O2 -mmacosx-version-min=15.0 -isysroot $SDKROOT"' in workflow
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
    assert 'ldd "$plugin"' in linux
    assert "for backend in ffmpeg gstreamer" in linux
    assert "inputs.target_arch == 'x86_64'" in linux
    assert "QtQuick.Controls" in Path("ui/components/shell/Main.qml").read_text()
    # The actual Qt plugin's ELF dependencies are authoritative; a separate
    # legacy libgstplay SONAME is not present on every Qt 6.12 target.
    assert "libgstplay-1.0.so.0" not in linux
    # A desktop install must not be a portable bundle masquerading as setup.
    # Verify per-user activation, desktop shortcuts, publisher metadata, and
    # architecture hygiene independently from the cross-platform build.
    instance = Path("ui/app/platform/instance.cpp").read_text(encoding="utf-8")
    desktop_module = Path("ui/app/platform/instance.cppm").read_text(encoding="utf-8")
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
        ("bootstrap/context", "genycaster.ui.bootstrap.context"),
        ("bootstrap/monitoring", "genycaster.ui.bootstrap.monitoring"),
        ("bootstrap/wallet", "genycaster.ui.bootstrap.wallet"),
        ("bootstrap/providers", "genycaster.ui.bootstrap.providers"),
        ("bootstrap/profiles", "genycaster.ui.bootstrap.profiles"),
        ("bootstrap/routes", "genycaster.ui.bootstrap.routes"),
        ("bootstrap/auth", "genycaster.ui.bootstrap.auth"),
        ("bootstrap/worker", "genycaster.ui.bootstrap.worker"),
        ("bootstrap/teardown", "genycaster.ui.bootstrap.teardown"),
        ("platform/wheel", "genycaster.ui.web.wheel"),
        ("update/install", "genycaster.ui.update.install"),
        ("platform/instance", "genycaster.ui.desktop.instance"),
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
        assert not PARENT_DIRECTORY_INCLUDE.search(implementation.read_text(encoding="utf-8")), relative
        if relative not in ("bootstrap/worker", "bootstrap/wallet"):
            # The worker and URL policy interfaces export no Qt types.
            assert '#include "qt_fwd.hpp"' in interface.read_text(encoding="utf-8")
        assert '#include <QString>' not in interface.read_text(encoding="utf-8")
    assert 'import genycaster.ui.bootstrap.fonts;' in entry
    assert 'import genycaster.ui.bootstrap.context;' in entry
    assert 'import genycaster.ui.bootstrap.wallet;' in entry
    assert 'bootstrap::prepareWalletApproval(' in entry
    assert 'bootstrap::openWalletApproval(' in entry
    assert 'bootstrap::miniAppEthereumChains()' in entry
    assert 'browser_wheel_fallback.hpp' not in Path(
        "ui/app/runtime/bindings.cpp").read_text(encoding="utf-8")
    assert Path("tests/ui/wallet_launch_tests.cpp").is_file()
    assert 'import genycaster.ui.bootstrap.monitoring;' in entry
    assert 'bootstrap::exposeControllers(engine, {' in entry
    assert 'bootstrap::exposeFontFamilies(engine, fontFamilies);' in entry
    assert 'bootstrap::connectNotificationBadge(' in entry
    assert 'bootstrap::connectNetworkRecovery(' in entry
    # Context aliases are a public QML contract; none may be renamed,
    # omitted, duplicated, or injected by partial registration.
    context_objects = re.findall(
        r'\{"([A-Za-z][A-Za-z0-9]*)",\s*&[A-Za-z][A-Za-z0-9]*\}', entry)
    expected_context_objects = {
        "genyPulseActivity", "openProofConnections", "proCheckout",
        "proWalletHost", "presentationBridge",
        "channelDirectory", "channelBrowser", "channelAdmin",
        "nativeRegistrationStatus", "nativeCustodyWallet",
        "nativeRegistrationPreparation", "nativeRegistrationPublisher",
        "neynarRegistration", "dataStorage", "richComposer", "emojiController",
        "mediaUpload", "mediaClassifier", "mediaPlayback",
        "localizationController", "geoLocationResolver", "linkPreviewResolver",
        "browserLinkMetadataController", "browserTokenPreviewController",
        "castViewController", "statsController", "marketController",
        "browserReactionController", "miniAppHost",
        "miniAppCatalog", "miniAppTaskManager", "liveInsights",
        "nodeSelectionController", "providerProfiles", "updateController",
        "appearanceController", "renderingController", "textSelectionSettings",
    }
    assert len(context_objects) == len(expected_context_objects), context_objects
    assert set(context_objects) == expected_context_objects, context_objects
    context_source = Path("ui/app/bootstrap/context.cpp").read_text(encoding="utf-8")
    assert "names.contains(name)" in context_source
    assert "entry.object == nullptr" in context_source
    assert Path("tests/ui/bootstrap_context_tests.cpp").is_file()
    assert 'authenticationAvailable, openExploreForTest]' in entry
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
        "ui/app/browser/signer_bridge.hpp",
        "ui/app/browser/cast_payload.hpp",
        "ui/app/browser/emoji_recovery.hpp",
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
        "ui/app/update/controller.cpp").read_text(encoding="utf-8")
    assert Path("ui/app/runtime/qt_fwd.hpp").is_file()
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
    nsis_options = Path("properties/Windows/NSISOptions.cmake.in").read_text(encoding="utf-8")
    assert "set(CPACK_PROJECT_CONFIG_FILE" in windows
    assert "GenyCasterNSISOptions.cmake" in windows
    assert "vc_redist.@GENYCASTER_VC_REDIST_SUFFIX@.exe" in nsis_options
    assert "ExecWait" in nsis_options and "SetRebootFlag true" not in nsis_options
    assert "CPACK_NSIS_WELCOME_TITLE" in windows
    assert 'file(WRITE "${_genycasterLicensePage}" "${_genycasterGplText}")' in windows
    assert "Get-AuthenticodeSignature -FilePath" in workflow
    assert "CPACK_NSIS_CREATE_ICONS_EXTRA" in nsis_options
    assert "CPACK_NSIS_DELETE_ICONS_EXTRA" in nsis_options
    assert "GenyCaster.lnk" in nsis_options
    assert "GenyCaster-Installer-License.txt" in windows
    assert "CPACK_RESOURCE_FILE_LICENSE" in windows
    assert 'RENAME "LICENSE.txt"' in windows
    assert "THIRD_PARTY_NOTICES.md" in windows
    assert 'VALUE "LegalCopyright"' in rc
    assert 'VALUE "LegalTrademarks"' in rc
    assert "python ci/audit_windows_stage.py dist/app" in workflow
    updates = Path("ui/app/update/controller.cpp").read_text(encoding="utf-8")
    assert "checksum.addData(&installer)" in updates
    assert "Cached update failed SHA-256 verification" in updates
    assert "QCoreApplication::quit();" in updates
    assert "token: ${{ github.token }}" in workflow
    assert "prerelease: ${{ contains(github.ref_name, '-') }}" in workflow
    assert "windows-11-arm" in workflow
    assert "android_arm64_v8a" in workflow and "android_armv7" not in workflow
    assert "name: iOS arm64 unsigned device app" in workflow
    assert "name: NixOS" in workflow
    assert "GenyCaster-linux-arm64" in workflow
    assert "GenyCaster-windows-arm64" in workflow
    assert "GenyCaster-android-arm64-v8a" in workflow
    assert "GenyCaster-android-armeabi-v7a" not in workflow
    assert '"vcpkg_triplet":"x64-windows"' in workflow
    assert '"vcpkg_triplet":"arm64-windows"' in workflow
    assert "inputs.target_arch == 'arm64'" in workflow
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
    assert Path("ui/components/shell/Main.qml").is_file(), "unified Qt application entrypoint is required"
    assert Path("ui/app/main.cpp").is_file(), "unified native application entrypoint is required"
    public_root_docs = {
        "README.md", "AGENTS.md", "CHANGELOG.md", "CODE_OF_CONDUCT.md",
        "CONTRIBUTING.md", "SECURITY.md", "STATUS.md", "THIRD_PARTY_NOTICES.md",
        # Verified release specification, ordered backlog and execution checkpoint.
        "SPEC.md", "PLAN.md", "PROJECT_STATE.md",
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
    assert "arm64-v8a" in workflow and "armeabi-v7a" not in workflow
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
