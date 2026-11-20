"""Portable, offline tests for release gates and artifact packaging."""
from __future__ import annotations

from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch
import sys
import unittest
import zipfile
import struct

from ci.verify_platform_package import (
    validate_android_elf_alignment, validate_android_elf32_arm, verify,
)
from ci.install_qt_windows import STAMP, archives_from_index
from ci.package_web import select_files
from ci.release_artifacts import EXPECTED, EXTRA_REQUIRED, main as assemble_release
from ci.release_gate import project_version, release_designation, release_version
from ci.verify_ci import (FORBIDDEN_CORE_JOB, MATRIX_JOB_DISPLAY_NAME,
                          PARENT_DIRECTORY_INCLUDE)


class PipelineTests(unittest.TestCase):
    def test_ui_feature_source_ownership(self) -> None:
        """Keep shared feature folders, short render files and stable QML aliases."""
        import json
        app = Path("ui/app")
        self.assertEqual(sorted(p.name for p in app.iterdir() if p.is_file()),
                         ["main.cpp"])
        for group in ("bootstrap", "runtime", "social", "identity",
                      "network", "browser", "miniapp", "media", "data",
                      "settings", "update", "platform", "diagnostics"):
            sources = [p for p in (app / group).rglob("*")
                       if p.is_file() and p.suffix in (".cpp", ".cppm", ".hpp", ".mm")]
            self.assertGreaterEqual(len(sources), 4, group)
        render = Path("ui/render")
        self.assertEqual({p.name for p in render.iterdir() if p.is_file()},
                         {"stroke.cpp", "stroke.hpp", "superellipse.cpp",
                          "superellipse.hpp", "sparkline.cpp", "sparkline.hpp",
                          "geometry_safety.hpp"})
        source_root = Path("ui/components")
        mapping = json.loads((source_root / "source_paths.json").read_text())
        # This count grows when new public QML components are registered.
        # The exact manifest-to-source set equality below is the real gate.
        self.assertGreaterEqual(len(mapping), 156)
        self.assertEqual(set(mapping.values()),
                         {p.relative_to(source_root).as_posix()
                          for p in source_root.rglob("*.qml")
                          if p.name != "Main.qml"})
        self.assertFalse(list(source_root.glob("*.qml")))
        self.assertEqual(len(mapping.values()), len(set(mapping.values())))
        cmake = Path("ui/CMakeLists.txt").read_text()
        self.assertIn('set(qmlResourceAlias "Main.qml")', cmake)
        self.assertIn('set(qmlResourceAlias "components/${CMAKE_MATCH_1}.qml")', cmake)
        # Qt 6.12 derives qmldir type entries from the source basename unless
        # they are explicitly overridden. Qt's extra qmldir generation also
        # ignores QT_RESOURCE_ALIAS when grouping physical source directories.
        self.assertIn('QT_QML_SOURCE_TYPENAME "${qmlPublicName}"', cmake)
        self.assertIn('get_source_file_property(qmlPublicAlias "${qmlFile}" QT_RESOURCE_ALIAS)', cmake)
        self.assertIn('get_filename_component(qmlPublicName "${qmlPublicAlias}" NAME_WE)', cmake)
        self.assertIn("NO_GENERATE_EXTRA_QMLDIRS", cmake)
        for public, physical in mapping.items():
            self.assertTrue((source_root / physical).is_file(), public)
            self.assertIn("components/" + physical, cmake, public)
            if physical.split("/")[-1] != public:
                self.assertIn('QT_RESOURCE_ALIAS "components/' + public + '"',
                              cmake, public)

    def test_openproof_connections_test_links_native_concurrent(self) -> None:
        """Standalone C++ test must carry every library needed by its sources."""
        import re

        source = Path("ui/app/identity/connections.cpp").read_text()
        cmake = Path("tests/ui/CMakeLists.txt").read_text()
        self.assertIn("#include <QtConcurrent>", source)
        self.assertIn("QtConcurrent::run(", source)
        self.assertIn('"${genycasterUiSourceDir}/app/identity/connections.cpp"', cmake)
        links = re.search(
            r"target_link_libraries\(farcaster_openproof_connections_tests"
            r"\s+PRIVATE\s+([^)]*)\)", cmake, re.S)
        self.assertIsNotNone(links)
        self.assertIn("Qt6::Concurrent", links.group(1))

    def test_explorer_masonry_uses_supported_qt_keyboard_signals(self) -> None:
        """Unsupported Keys signals must not break Main.qml at application startup."""
        import re

        component = Path("ui/components/feed/FeedMasonryView.qml")
        source = component.read_text(encoding="utf-8")
        self.assertIn("Keys.onPressed: function(event)", source)
        self.assertIn("event.accepted = true", source)
        for key in ("PageDown", "PageUp", "Home", "End", "Down", "Up"):
            self.assertIn(f"case Qt.Key_{key}:", source)

        unsupported = re.compile(r"\\bKeys\\.on(?:PageDown|PageUp|Home|End)Pressed\\s*:")
        for qml in Path("ui").rglob("*.qml"):
            with self.subTest(qml=qml.as_posix()):
                self.assertNotRegex(qml.read_text(encoding="utf-8"), unsupported)

    def test_explorer_feed_row_does_not_assign_loader_readonly_implicit_height(self) -> None:
        """Qt Loader derives implicitHeight from the item; only height is writable."""
        source = Path("ui/components/feed/FeedSurface.qml").read_text(encoding="utf-8")
        marker = "component FeedRow: Loader {"
        self.assertIn(marker, source)
        row = source.split(marker, 1)[1].split("        CastSkeleton {", 1)[0]
        self.assertNotRegex(row, r"(?m)^\s*implicitHeight\s*:")
        self.assertRegex(row, r"(?m)^\s*height: Math\.max\(1, committedHeight\)")
        self.assertIn("onResolvedHeightChanged: rowDelegate.synchronizeHeight()", source)
        self.assertIn("onReservedHeightChanged: rowDelegate.synchronizeHeight()", source)
        # The dedicated native Qt test must load the real type chain, not
        # merely scan QML as C++-generated source.
        smoke = Path("tests/ui/media_scroll_tests.cpp").read_text(encoding="utf-8")
        for type_name in ("FeedMasonryView", "ExplorerLayoutControls", "FeedSurface", "AppShell"):
            self.assertIn(f'QStringLiteral("{type_name}")', smoke)

    def test_native_qml_smoke_uses_public_alias_fixture(self) -> None:
        """Native QML smoke must load public component names after file moves."""
        cmake = Path("tests/ui/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("components/source_paths.json", cmake)
        self.assertIn("string(JSON genycasterQmlPublicName MEMBER", cmake)
        self.assertIn("string(JSON genycasterQmlPhysicalName GET", cmake)
        self.assertIn('MEDIA_COMPONENT_DIRECTORY="${genycasterPublicQmlFixtureDir}"', cmake)
        self.assertIn('"${genycasterPublicQmlFixtureDir}/ProCastPolicy.js"', cmake)

    def test_all_qml_module_units_import_the_public_namespace(self) -> None:
        """Feature folders must not rely on incidental implicit QML imports."""
        import json
        import re
        root = Path("ui/components")
        mapping = json.loads((root / "source_paths.json").read_text(encoding="utf-8"))
        sources = [root / "shell/Main.qml", *(root / p for p in mapping.values())]
        for path in sources:
            with self.subTest(path=str(path)):
                text = path.read_text(encoding="utf-8")
                self.assertRegex(text, r"(?m)^import Farcaster\.Client\s*$")

    def test_native_qml_types_import_their_own_module(self) -> None:
        """Nested QML sources must explicitly import C++-registered module types."""
        import re

        native_sources = {
            "RichInlineTextItem": "src/presentation/qt/rich_inline_text_item.hpp",
            "FeedRowsModel": "ui/app/social/feed_rows.hpp",
            "SceneGraphSuperellipse": "ui/render/superellipse.hpp",
            "SceneGraphStroke": "ui/render/stroke.hpp",
            "TokenSparklineGeometry": "ui/render/sparkline.hpp",
        }
        main = Path("ui/app/main.cpp").read_text(encoding="utf-8")
        for native_type, header in native_sources.items():
            native_source = Path(header).read_text(encoding="utf-8")
            if native_type == "RichInlineTextItem":
                self.assertIn("qmlRegisterType<RichInlineTextItem>", main)
            else:
                self.assertIn(f"QML_NAMED_ELEMENT({native_type})", native_source)
        for qml in Path("ui/components").rglob("*.qml"):
            source = qml.read_text(encoding="utf-8")
            used_types = [
                name for name in native_sources
                if re.search(r"(?m)^\s*" + re.escape(name) + r"\s*(?:\{|[.])", source)
            ]
            if used_types:
                self.assertRegex(
                    source, r"(?m)^import Farcaster\.Client(?:\s|$)",
                    f"{qml} must import Farcaster.Client for {used_types}",
                )

    def test_responsive_interaction_contracts(self) -> None:
        """Guard the shared motion, mobile sheets and composer chrome."""
        qml = Path("ui/components")
        setup = (qml / "setup/InitialSetup.qml").read_text(encoding="utf-8")
        checkbox = (qml / "controls/GenyCheckRow.qml").read_text(encoding="utf-8")
        home = (qml / "home/HomeFeedHeader.qml").read_text(encoding="utf-8")
        tabs = (qml / "navigation/ContentTabs.qml").read_text(encoding="utf-8")
        composer = (qml / "composer/ComposerDrawer.qml").read_text(encoding="utf-8")
        settings = (qml / "settings/SettingsPopup.qml").read_text(encoding="utf-8")
        shell = (qml / "shell/AppShell.qml").read_text(encoding="utf-8")
        self.assertIn("components/controls/GenyCheckRow.qml", Path("ui/CMakeLists.txt").read_text())
        # Backup verification, irreversible publish consent, and recovery\n        # reveal/signer consent must all use the shared accessible control.\n        self.assertEqual(setup.count("GenyCheckRow {"), 4)\n        self.assertIn('objectName: "setupNativeRegistrationConsent"', setup)
        self.assertIn("Accessible.role: Accessible.CheckBox", checkbox)
        self.assertIn("spacing: Theme.space2", checkbox)
        self.assertIn("checkable: true", checkbox)
        # The checked state and focus use the canonical accessible neon row;
        # the older detached checkbox tile is no longer the design contract.
        self.assertIn("background: NeonSurface {", checkbox)
        self.assertIn("interactive: control.enabled", checkbox)
        self.assertIn(": control.checked ? Theme.surfaceSelected", checkbox)
        self.assertIn(": control.checked ? Theme.accent : Theme.borderMuted", checkbox)
        self.assertIn("Accessible.role: Accessible.CheckBox", checkbox)
        self.assertIn("color: Theme.contrastInk(Theme.accent)", checkbox)
        self.assertIn("font.pixelSize: Theme.typeLabelSize", checkbox)
        self.assertNotIn("Rectangle {", checkbox)
        self.assertNotIn("ColorAnimation", checkbox)
        self.assertIn("id: audienceSelectionPill", home)
        self.assertIn("Behavior on x", home)
        self.assertIn("Accessible.role: Accessible.PageTab", tabs)
        self.assertNotIn("Accessible.role: Accessible.Tab", tabs)
        self.assertIn("Behavior on color", tabs)
        self.assertIn("ColorAnimation", tabs)
        # Hover is text-only: background plates fade into adjacent cells.
        self.assertNotIn("anchors.margins: Theme.space1", tabs)
        self.assertNotIn("color: hover.hovered && !tabButton.selected", tabs)
        self.assertIn("Behavior on color", home)
        self.assertIn("ColorAnimation", home)
        self.assertIn("background: Item {}", home)
        self.assertNotIn("color: scopeOption.selected ? \"transparent\"", home)
        shared_tabs = (qml / "controls/GenyTabButton.qml").read_text(encoding="utf-8")
        actions = (qml / "controls/ActionButton.qml").read_text(encoding="utf-8")
        self.assertIn("ColorAnimation", shared_tabs)
        self.assertIn("ColorAnimation", actions)
        # Selection material is opt-in for explicit filled tab groups, while
        # ordinary tabs remain typographic and retain curved hit testing.
        self.assertIn("background: NeonStatePlate", shared_tabs)
        self.assertIn("containmentMask: background", shared_tabs)
        self.assertIn("property bool filledTabs: false", shared_tabs)
        self.assertIn("selected: control.filledTabs && control.selected", shared_tabs)
        self.assertIn("hovered: control.hovered", shared_tabs)
        self.assertIn("selectedFill: Theme.primarySurface", shared_tabs)
        self.assertIn('hoverFill: control.filledTabs ? Theme.surfaceHover : "transparent"', shared_tabs)
        self.assertIn("neonOnHover: control.filledTabs", shared_tabs)
        avatar = (qml / "content/Avatar.qml").read_text(encoding="utf-8")
        story = (qml / "feed/StoryItem.qml").read_text(encoding="utf-8")
        self.assertIn("visible: !avatarImage.ready", avatar)
        self.assertIn("opacity: 1.0", story)
        self.assertNotIn("scale: presetHover.hovered", settings)
        # The feed tabs intentionally remain background-free; no extra
        # underline floats below the selected text.
        self.assertNotIn('objectName: "contentTabUnderline"', tabs)
        self.assertIn("font.weight: Theme.weightDemiBold", tabs)
        self.assertIn("font.weight: Theme.weightDemiBold", tabs)
        self.assertIn("id: audienceSelectionPill", home)
        feed = (qml / "feed/FeedSurface.qml").read_text(encoding="utf-8")
        self.assertIn("property bool selectionSettling: false", feed)
        self.assertIn("root.beginSelectionHandoff()", feed)
        self.assertIn("root.selectionSettling", feed)
        self.assertIn("visible: root.mobilePresentation", settings)
        self.assertIn("columns: appearanceScroll.availableWidth >= 520 ? 4", settings)
        self.assertIn("Accessible.role: Accessible.RadioButton", settings)
        self.assertIn("id: composerFixedChrome", composer)
        self.assertLess(composer.index("id: composerFixedChrome"),
                        composer.index("id: composerScroll"))
        self.assertLess(composer.index("id: composerScroll"),
                        composer.index("id: mobileComposerFooter"))
        self.assertIn("Layout.minimumHeight: 0", composer)
        self.assertIn("Math.min(720, Math.max(0, parent.width - Theme.space4 * 2))", composer)
        self.assertIn("Math.min(parent.height * 0.78, 660)", composer)
        self.assertIn("&& !channelPicker.opened && !topicPicker.opened", composer)
        self.assertIn("desktopFullScreen: true", settings)
        self.assertIn("width: parent ? parent.width : 1180", settings)
        self.assertIn("height: parent ? parent.height : 780", settings)
        self.assertIn("mobileFullScreen: true", settings)
        self.assertIn('id: mobileSetupNav', setup)
        self.assertIn('anchors.top: root.phone ? mobileSetupNav.bottom', setup)
        self.assertIn('desktopSetupNav.visible ? desktopSetupNav.bottom : parent.top', setup)
        self.assertIn('anchors.top: root.phone ? mobileSetupNav.top : parent.top', setup)
        self.assertIn("root.pendingSettingsOpen = true", shell)
        self.assertIn("onLoaded: root.showLoadedSettings()", shell)
        self.assertEqual(settings.count("id: navigationEntry1"), 1)
        self.assertEqual(settings.count("id: navigationEntry2"), 1)
        self.assertEqual(settings.count("id: navigationEntry3"), 1)

    def test_composer_taxonomy_uses_bounded_dropdowns_and_full_editor(self) -> None:
        composer = Path("ui/components/composer/ComposerDrawer.qml").read_text(encoding="utf-8")
        editor = Path("ui/components/composer/RichTextEditor.qml").read_text(encoding="utf-8")
        channel = composer.split('id: channelPicker', 1)[1].split('id: topicPicker', 1)[0]
        topics = composer.split('id: topicPicker', 1)[1].split('id: mediaFileDialog', 1)[0]
        for popup in (channel, topics):
            # The popup must share its coordinate space with its trigger
            # on desktop, but attach to the composer viewport on mobile.
            # Anchoring to Overlay.overlay previously broke positioning
            # as the enclosing Drawer moved.
            self.assertIn("parent: root.compact ? root.contentItem : triggerItem", popup)
            self.assertIn("popupType: Popup.Item", popup)
            self.assertIn("triggerItem.mapToItem(root.contentItem, 0, 0)", popup)
            self.assertIn("root.contentItem.height - boundaryInset - triggerOrigin.y", popup)
            self.assertIn("triggerItem.height + Theme.space2", popup)
            self.assertNotIn("triggerOrigin.y - y", popup)
            self.assertNotIn('property: "y"', popup)
            self.assertIn("closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside", popup)
            self.assertIn("boundsBehavior: Flickable.StopAtBounds", popup)
            self.assertNotIn("parent: Overlay.overlay", popup)
            self.assertNotIn("bottomSheet: true", popup)
            self.assertNotIn("dimBackground: true", popup)
        self.assertIn('objectName: "composerChannelDropdown"', channel)
        self.assertIn("model: channelDirectory.channelModel", channel)
        self.assertIn("channelDirectory.search(text)", channel)
        self.assertIn("channelDirectory.chooseChannel(", channel)
        self.assertIn('objectName: "composerTopicDropdown"', topics)
        self.assertIn("openProofConnections.fetchTopics()", topics)
        self.assertIn("root.toggleTopic(topicChip.slug)", topics)
        self.assertIn("readonly property int maxCastTopics: 3", composer)
        self.assertIn("root.selectedParentUrl = String(selection.parentUrl", composer)
        self.assertIn("id: composerFixedChrome", composer)
        self.assertIn("id: formattingToolbar", composer)
        self.assertLess(composer.index("id: formattingToolbar"),
                        composer.index("id: composerScroll"))
        self.assertIn("root.blockModel.length === 1", editor)
        self.assertIn("list.height - Theme.space2", editor)
        self.assertIn('fillColor: "transparent"', editor)
        self.assertIn("borderWidth: 0", editor)
        self.assertIn('visible: root.hasPublishableContent()', composer)

    def test_http_authentication_diagnostics_do_not_log_credentials(self) -> None:
        for path in (
            "src/transport/qt_network/async_http.cpp",
            "src/transport/qt_network/http.cpp",
        ):
            source = Path(path).read_text(encoding="utf-8")
            self.assertIn("QNetworkReply::AuthenticationRequiredError", source)
            self.assertIn('"[Genycaster] Authentication required by host:"', source)
            self.assertIn("reply->url().host()", source)
            self.assertIn("#include <QDebug>", source)
            self.assertNotIn("reply->url().toString()", source)

    def test_native_recovery_session_is_pinned_and_cancelable(self) -> None:
        bridge = Path("src/presentation/qt/presentation_bridge.cpp").read_text(encoding="utf-8")
        wiring = Path("ui/app/runtime/bindings.cpp").read_text(encoding="utf-8")
        setup = Path("ui/components/setup/InitialSetup.qml").read_text(encoding="utf-8")
        self.assertIn("void PresentationBridge::beginNativeRecoverySetup(QString recoveryWords)", bridge)
        self.assertIn("emit nativeRecoveryRequested(std::move(recoveryWords));", bridge)
        self.assertIn("Qt.ImhSensitiveData", setup)
        self.assertIn("nativeRecoveryGrid.clearAll()", setup)
        self.assertIn("root.nativeRecoverFarcasterAccount(words, deviceSignerConsent.checked)", setup)
        self.assertIn("root.nativeRecoverHyperSnapAccount(words, deviceSignerConsent.checked)", setup)
        self.assertIn("nativeCustodyWallet.restoreForRegistration(words)", setup)
        # The native recovery DTO owns an ICustodyAuthorizer. Imports of other
        # C++26 modules do not make that capability declaration transitive.
        self.assertIn("import farcaster.capability.signer;", wiring)
        self.assertIn("std::shared_ptr<capability::signer::ICustodyAuthorizer>", wiring)
        # Watch Qt's void completion channel: comparing QFuture of a
        # C++26 std::expected DTO can instantiate incompatible constraints.
        self.assertIn("QFutureWatcher<void>", wiring)
        self.assertIn("std::make_shared<QFuture<RecoveryOutcome>>()", wiring)
        self.assertIn("RecoveryOutcome verified = recoveryFuture->result();", wiring)
        self.assertIn(
            "[&bridge, &connections, runtime, sessionObserver,",
            wiring,
            "The recovery completion callback must capture the Runtime it commits into.",
        )
        self.assertIn("resolveRecoveryCustodyFid(*runtime->transport, *address)", wiring)
        self.assertIn("startRecoveryChallenge(*runtime->transport, gateway, *fid, *address)", wiring)
        self.assertIn("signOneTimeIdentityProof(mnemonic, fid->value, challenge->message)", wiring)
        self.assertIn("completeRecoveryChallenge(", wiring)
        self.assertIn("nativeRecoveryGeneration->fetch_add", wiring)
        self.assertIn('bridge.authenticationRoute() == QStringLiteral("recovery")', wiring)
        self.assertIn("connections.acceptRecoveryGrant(", wiring)
        self.assertIn("runtime->sessionService->acceptVerifiedRecovery(", wiring)
        self.assertIn("runtime->sessionService->signOut(sessionObserver)", wiring)
        service = Path("src/application/session.cpp").read_text(encoding="utf-8")
        self.assertIn("SessionService::acceptVerifiedRecovery(", service)
        self.assertIn("m_impl->store.save(session)", service)
        self.assertIn("m_impl->current = std::move(session)", service)

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
        source = Path("ui/app/miniapp/host.cpp").read_text(encoding="utf-8")
        start = source.index("void MiniAppHostController::openWithMetadata(")
        end = source.index("void MiniAppHostController::close()", start)
        procedure = source[start:end]
        validation = procedure.index("const QUrl destination{url.trimmed(), QUrl::StrictMode};")
        wasm_dispatch = procedure.index("#if defined(Q_OS_WASM)")
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
        controller = Path("ui/app/miniapp/host.cpp").read_text(encoding="utf-8")
        header = Path("ui/app/miniapp/host.hpp").read_text(encoding="utf-8")
        surface = Path("ui/components/apps/AppsSurface.qml").read_text(encoding="utf-8")
        self.assertIn("if (QDesktopServices::openUrl(destination))", controller)
        self.assertIn("emit externalLaunchSucceeded();", controller)
        self.assertIn("void externalLaunchSucceeded();", header)
        self.assertIn("function onExternalLaunchSucceeded()", surface)
        self.assertIn("root.pendingLaunchApp = null", surface)
        self.assertNotIn('if (!miniAppHost.available && Qt.platform.os !== "wasm")', surface)

    def test_version_tag_gate(self) -> None:
        numeric = project_version(Path.cwd())
        version = release_designation(Path.cwd())
        self.assertTrue(version.startswith(numeric + "-"))
        self.assertEqual(release_version("refs/heads/main", version), (version, False))
        self.assertEqual(release_version(f"refs/tags/v{version}", version), (version, True))
        for bad in ("refs/tags/v1.2", "refs/tags/latest", "refs/tags/v01.2.3",
                    f"refs/tags/v{numeric}", f"refs/tags/v{numeric}-rc999",
                    "refs/tags/v999.0.0"):
            with self.assertRaises(ValueError):
                release_version(bad, version)

    def test_native_executable_and_release_identity_contract(self) -> None:
        # Qt Creator's Build All compiles the production source in a separate test.
        # CMake must provide identical release metadata on every platform.
        cmake = Path("ui/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("qt_add_executable(genycaster", cmake)
        self.assertIn('set_target_properties(genycaster PROPERTIES OUTPUT_NAME "Genycaster")', cmake)
        self.assertIn("if(NOT ANDROID AND NOT EMSCRIPTEN)", cmake)
        self.assertIn("add_library(genycaster_release_identity INTERFACE)", cmake)
        self.assertIn("target_link_libraries(genycaster PRIVATE genycaster_release_identity)", cmake)
        test_cmake = Path("tests/ui/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("target_link_libraries(farcaster_update_controller_tests PRIVATE", test_cmake)
        self.assertIn("    genycaster_release_identity", test_cmake)
        for name in ("GENYCASTER_APP_VERSION", "GENYCASTER_PRODUCT_NAME",
                     "GENYCASTER_VERSION_NUMBER", "GENYCASTER_RELEASE_CHANNEL",
                     "GENYCASTER_RELEASE_CHANNEL_NUMBER"):
            with self.subTest(macro=name):
                self.assertEqual(cmake.count(name + "="), 1)
        for file in (".github/workflows/ci.yml", "ci/package_web.py",
                     "ci/package_wasm_assets.py", "ci/verify_platform_package.py",
                     "ui/wasm/genycaster-loader.js"):
            with self.subTest(path=file):
                content = Path(file).read_text(encoding="utf-8")
                if file == "ci/package_web.py":
                    # The old name is permitted only as an explicit rejection
                    # rule; a legacy engine must never enter a new archive.
                    self.assertIn('name.startswith("farcaster_client")', content)
                    self.assertNotIn('archive.add(source / "farcaster_client', content)
                else:
                    self.assertNotIn("farcaster_client", content)
        self.assertIn("window.genycaster_entry",
                      Path("ui/wasm/genycaster-loader.js").read_text(encoding="utf-8"))

    def test_ci_workflow_regex_guards(self) -> None:
        """Catch skipped-job template names and unsafe source includes."""
        workflow = Path(".github/workflows/ci.yml").read_text(encoding="utf-8")
        self.assertIsNone(FORBIDDEN_CORE_JOB.search(workflow))
        self.assertIsNone(MATRIX_JOB_DISPLAY_NAME.search(workflow))
        self.assertIsNotNone(FORBIDDEN_CORE_JOB.search("jobs:\n  core:\n    runs-on: ubuntu-latest\n"))
        self.assertIsNotNone(MATRIX_JOB_DISPLAY_NAME.search(
            "  android:\n    name: Android ${{ matrix.abi }} APK\n"))
        self.assertIsNotNone(MATRIX_JOB_DISPLAY_NAME.search(
            "  macos:\n    name: macOS ${{matrix.arch}} DMG\n"))
        self.assertIsNotNone(PARENT_DIRECTORY_INCLUDE.search('#include "../unsafe.hpp"\n'))
        self.assertIsNone(PARENT_DIRECTORY_INCLUDE.search('#include "qt_fwd.hpp"\n'))

    def test_windows_qt_archive_manifest(self) -> None:
        html = f'<a href="{STAMP}qtbase-Windows-MSVC2022.7z">Qt Base</a>'
        self.assertEqual(
            archives_from_index(html, ("qtbase",)),
            [f"{STAMP}qtbase-Windows-MSVC2022.7z"],
        )
        with self.assertRaises(ValueError):
            archives_from_index(html, ("qtbase", "qtdeclarative"))

    def test_web_archive_selection_excludes_build_artifacts(self) -> None:
        import json

        with TemporaryDirectory() as path:
            root = Path(path)
            engine = "genycaster.0123456789abcdef0123.wasm"
            files = {
                "genycaster.html": b"<html></html>",
                "genycaster.js": b"entry",
                "genycaster-loader.js": b"loader",
                "genycaster-sw.js": b"worker",
                "genycaster.wasm": b"\\x00asm",
                engine: b"\\x00asm",
                engine + ".gz": b"compressed",
            }
            for name, data in files.items():
                (root / name).write_bytes(data)
            manifest = {
                "wasm": engine, "wasmGzip": engine + ".gz",
                "files": sorted(set(files) - {"genycaster.wasm"}),
            }
            (root / "genycaster-assets.json").write_text(json.dumps(manifest))
            (root / "genycaster.aaaaaaaaaaaaaaaaaaaa.wasm").write_bytes(b"stale")
            (root / "genycaster.aaaaaaaaaaaaaaaaaaaa.wasm.gz").write_bytes(b"stale")
            (root / "unrelated.tar.gz").write_bytes(b"ignored")
            (root / "genycaster_autogen").mkdir()
            (root / "genycaster_autogen" / "secret.js").write_text("ignored")
            (root / "genycaster.cpp.o").write_bytes(b"obj")
            selected = {p.name for p in select_files(root)}
            self.assertEqual(selected, set(files) | {"genycaster-assets.json"})
            manifest["files"].append("../private.json")
            (root / "genycaster-assets.json").write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "Unsafe or unsupported"):
                select_files(root)
            manifest["files"][-1] = "genycaster.aaaaaaaaaaaaaaaaaaaa.wasm"
            (root / "genycaster-assets.json").write_text(json.dumps(manifest))
            # A manifest is not allowed to smuggle an obsolete engine into a
            # release, even if it exists in a previously used build directory.
            with self.assertRaisesRegex(ValueError, "Unexpected"):
                select_files(root)

    def test_web_deployment_updates_legacy_root_last(self) -> None:
        script = Path("ci/build_deploy_wasm_server.sh").read_text(encoding="utf-8")
        self.assertIn('if [[ -f "$web_root/farcaster_client.html" ]]', script)
        self.assertIn('cp -p -- "$stage/genycaster.html" "$stage/farcaster_client.html"', script)
        self.assertIn('if [[ "$rel" == genycaster.html || "$rel" == farcaster_client.html', script)
        self.assertLess(script.index("promote genycaster.html"),
                        script.index("promote farcaster_client.html"))
        self.assertLess(script.index("python3 ci/verify_platform_package.py"),
                        script.index('tar -xzf "$archive" -C "$stage"'))

    def test_web_archive_manifest_integrity_detects_corruption(self) -> None:
        import hashlib
        import json
        import tarfile
        from ci.verify_platform_package import validate_web

        with TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "public"
            source.mkdir()
            binary = b"\\x00asm\\x01\\x00\\x00\\x00"
            engine = "genycaster." + hashlib.sha256(binary).hexdigest()[:20] + ".wasm"
            gzip_name = engine + ".gz"
            assets = {
                "genycaster.html": b"<html>Genycaster</html>",
                "genycaster.js": b"entry",
                "genycaster-loader.js": b"loader",
                "genycaster-sw.js": b"worker",
                "genycaster.wasm": binary,
                engine: binary,
                gzip_name: b"compressed-fixture",
            }
            version = "a" * 20
            assets["genycaster-assets.js"] = (
                "window.__genycasterAssets=" + json.dumps({
                    "wasm": engine, "wasmBytes": len(binary),
                    "wasmGzip": gzip_name, "wasmGzipBytes": len(assets[gzip_name]),
                    "version": version,
                }) + ";\n").encode()
            files = sorted(set(assets) - {"genycaster.wasm"})
            manifest = {
                "wasm": engine, "wasmBytes": len(binary),
                "wasmGzip": gzip_name, "wasmGzipBytes": len(assets[gzip_name]),
                "wasmGzipSha256": hashlib.sha256(assets[gzip_name]).hexdigest(),
                "version": version, "files": files,
                "integrity": {
                    name: hashlib.sha256(assets[name]).hexdigest()
                    for name in files if name not in {engine, "genycaster-assets.js"}
                },
            }
            assets["genycaster-assets.json"] = json.dumps(manifest).encode()
            for name, content in assets.items():
                (source / name).write_bytes(content)

            def make_archive(target: Path) -> None:
                with tarfile.open(target, "w:gz") as archive:
                    for file in select_files(source):
                        archive.add(file, arcname=file.name)

            archive = root / "genycaster-good.tar.gz"
            make_archive(archive)
            validate_web(archive)
            (source / "genycaster.js").write_bytes(b"tampered")
            damaged = root / "genycaster-damaged.tar.gz"
            make_archive(damaged)
            with self.assertRaisesRegex(ValueError, "digest mismatch"):
                validate_web(damaged)

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

    def test_android_armv7_elf_type_and_machine(self) -> None:
        elf = bytearray(52)
        elf[:6] = b"\x7fELF\x01\x01"
        struct.pack_into("<H", elf, 18, 40)  # EM_ARM
        struct.pack_into("<H", elf, 40, 52)  # ELF32 header size
        validate_android_elf32_arm("armeabi-v7a.so", bytes(elf))

        elf[4] = 2
        with self.assertRaisesRegex(ValueError, "32-bit"):
            validate_android_elf32_arm("wrong-class.so", bytes(elf))
        elf[4] = 1
        struct.pack_into("<H", elf, 18, 183)
        with self.assertRaisesRegex(ValueError, "ARM machine"):
            validate_android_elf32_arm("wrong-machine.so", bytes(elf))
        with self.assertRaisesRegex(ValueError, "not an ELF"):
            validate_android_elf32_arm("truncated.so", b"hello")

    def test_windows_qt_architecture_manifest(self) -> None:
        from ci.install_qt_windows import archive_layout
        amd64, pkgs64 = archive_layout("x86_64")
        arm64, pkgsarm = archive_layout("arm64")
        self.assertIn("/windows_x86/", amd64)
        self.assertIn("/windows_arm64/", arm64)
        self.assertTrue(any("win64_msvc2022_64" in key for key in pkgs64))
        self.assertTrue(any("win64_msvc2022_arm64" in key for key in pkgsarm))

    def test_nixos_and_ios_build_contracts(self) -> None:
        workflow = Path(".github/workflows/ci.yml").read_text(encoding="utf-8")
        flake = Path("flake.nix").read_text(encoding="utf-8")
        self.assertTrue(flake.rstrip().endswith("}"))
        self.assertIn("gcc16Stdenv.mkDerivation", flake)
        self.assertIn("qt.qtkeychain", flake)
        self.assertIn("name: NixOS", workflow)
        self.assertIn("name: iOS arm64 unsigned", workflow)
        self.assertIn("ios-arm64-unsigned-app.tar.gz", workflow)
        self.assertNotIn("ios-arm64.ipa", workflow)

    def test_windows_deploy_disables_conflicting_vcpkg_applocal(self) -> None:
        workflow = Path(".github/workflows/ci.yml").read_text(encoding="utf-8")
        self.assertIn('"-DVCPKG_APPLOCAL_DEPS=OFF"', workflow)
        self.assertIn("Copy-Item $keychain.FullName dist/app/", workflow)
        self.assertIn("cmake --build build/native --target qt6keychain", workflow)
        self.assertIn("Copy-Item $keychainBuilt.FullName", workflow)
        self.assertIn("Join-Path $qtBin 'qt6keychain.dll'", workflow)
        self.assertIn("Copy-Item $dll.FullName dist/app/", workflow)

    def test_windows_nsis_installer_payload(self) -> None:
        with TemporaryDirectory() as folder:
            installer = Path(folder) / "GenyCaster-windows-x86_64.exe"
            installer.write_bytes(b"MZ" + bytes(8192))
            required = {
                "Genycaster.exe", "Qt6Core.dll", "Qt6Gui.dll",
                "Qt6Qml.dll", "Qt6Quick.dll", "Qt6Multimedia.dll",
                "Qt6Network.dll", "qt6keychain.dll", "platforms/qwindows.dll",
                "qml/QtQuick/Controls/qtquickcontrols2plugin.dll",
                "qml/QtQuick/Controls/Basic/qtquickcontrols2basicstyleplugin.dll",
                "multimedia/ffmpegmediaplugin.dll",
                "libcrypto-3-x64.dll", "libssl-3-x64.dll",
                "libcurl.dll", "sqlite3.dll",
                "LICENSE.txt", "THIRD_PARTY_NOTICES.md",
                "vc_redist.x64.exe",
            }
            with patch("ci.verify_platform_package.windows_installer_entries",
                       return_value=required):
                verify("windows", installer)
            with patch("ci.verify_platform_package.windows_installer_entries",
                       return_value=required - {"Qt6Network.dll"}):
                with self.assertRaisesRegex(ValueError, "qt6network"):
                    verify("windows", installer)
            with patch("ci.verify_platform_package.windows_installer_entries",
                       return_value=(required - {"Genycaster.exe"}) | {"genycaster.exe"}):
                with self.assertRaisesRegex(ValueError, "correctly cased executable"):
                    verify("windows", installer)
            with patch("ci.verify_platform_package.windows_installer_entries",
                       return_value=required):
                installer.with_suffix(".zip").write_bytes(b"MZ" + bytes(8192))
                with self.assertRaisesRegex(ValueError, "installable EXE"):
                    verify("windows", installer.with_suffix(".zip"))
            installer.write_bytes(b"PK" + bytes(8192))
            with self.assertRaisesRegex(ValueError, "PE header"):
                verify("windows", installer)

    def test_release_package_contract(self) -> None:
        self.assertEqual(len(EXPECTED), 7)
        self.assertEqual(EXPECTED["GenyCaster-linux-arm64"], ".AppImage")
        self.assertEqual(EXPECTED["GenyCaster-windows-arm64"], ".exe")
        self.assertNotIn("GenyCaster-android-armeabi-v7a", EXPECTED)
        self.assertEqual(EXPECTED["GenyCaster-macos-x86_64"], ".dmg")
        self.assertEqual(EXPECTED["GenyCaster-android-arm64-v8a"], ".apk")
        self.assertNotIn("GenyCaster-web-wasm", EXPECTED)
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            artifacts = root / "artifacts"
            release = root / "release"
            for platform, extension in EXPECTED.items():
                directory = artifacts / platform
                directory.mkdir(parents=True)
                (directory / f"{platform}{extension}").write_bytes(platform.encode())
                for index, extra_suffix in enumerate(EXTRA_REQUIRED.get(platform, ())):
                    extra_folder = directory / "selfupdate"
                    extra_folder.mkdir(exist_ok=True)
                    (extra_folder / f"GenyCaster-secondary-{platform}-{index}{extra_suffix}").write_bytes(
                        (platform + extra_suffix).encode()
                    )
            with patch.object(sys, "argv", ["release_artifacts.py", str(artifacts), str(release)]):
                assemble_release()
            expected_count = len(EXPECTED) + sum(map(len, EXTRA_REQUIRED.values()))
            self.assertEqual(len(list(release.iterdir())), expected_count + 1)
            self.assertEqual(len((release / "SHA256SUMS").read_text().splitlines()), expected_count)


if __name__ == "__main__":
    unittest.main()
