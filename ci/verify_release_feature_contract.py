#!/usr/bin/env python3
"""Fail-closed version-tag policy for advertised GenyCaster feature parity.

A successful compiler link cannot prove a missing platform adapter exists.
This deliberately checks concrete implementation wiring, not optimistic flags.
Manual single-platform CI remains available for incremental validation.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def blockers() -> list[str]:
    ui = read("ui/CMakeLists.txt")
    workflow = read(".github/workflows/ci.yml")
    wasm = read("cmake/platform/WebAssembly/configure.cmake")
    errors: list[str] = []

    linux = workflow.split("  linux:\n", 1)[1].split("  macos:\n", 1)[0]
    macos = workflow.split("  macos:\n", 1)[1].split("  windows:\n", 1)[0]
    windows = workflow.split("  windows:\n", 1)[1].split("  android:\n", 1)[0]
    android = workflow.split("  android:\n", 1)[1].split("  ios:\n", 1)[0]
    web = workflow.split("  web:\n", 1)[1].split("  publish:\n", 1)[0]

    # Version-tag builds must explicitly opt in on every advertised target.
    # Never derive Release feature admission from a mutable CMake default:
    # flipping an OFF default to ON must not bypass this fail-closed gate.
    # The initial public build advertises Channels and Search. Chat and the
    # standalone Wallet product are explicitly deferred; do not confuse them
    # with the custody/signing/recovery capabilities required by onboarding.
    for feature in ("CHANNELS", "SEARCH"):
        option = f"-DFARCASTER_ENABLE_{feature}=ON"
        if any(option not in block for block in (linux, macos, windows, android, web)):
            errors.append(f"{feature}: disabled in at least one platform's "
                          "product composition / Release configuration")
    for feature in ("CHAT", "WALLET"):
        option = f"-DFARCASTER_ENABLE_{feature}=OFF"
        if any(option not in block for block in (linux, macos, windows, android, web)):
            errors.append(f"{feature}: deferred from the initial Release but not "
                          "explicitly disabled on every platform")

    # Channels discovery is a real Qt route, not an advertised stub. Browsing,
    # signed membership selection, and privileged administration must stay
    # separated: unverified membership/roles must never grant write authority.
    if all("-DFARCASTER_ENABLE_CHANNELS=ON" in block
           for block in (linux, macos, windows, android, web)):
        channels_contract = {
            "src/application/configured_features.cpp.in": (
                ".hasChannels = @FARCASTER_ENABLE_CHANNELS_CPP@",),
            "ui/CMakeLists.txt": (
                "components/social/ChannelsSurface.qml",
                "app/social/channels.cpp",
                "app/social/channel_administration.cpp",),
            "ui/app/main.cpp": (
                "channelBrowserControllerOwner",
                "channelAdministrationControllerOwner",
                '{"channelBrowser", &channelBrowser}',
                '{"channelAdmin", &channelAdmin}',),
            "ui/components/shell/AppShell.qml": (
                'active: root.shellRoute === "channels"',
                "channelSource: channelBrowser",
                "adminSource: channelAdmin",),
            "ui/components/social/ChannelsSurface.qml": (
                "channelSource.configureReadSource(readContract, readEndpoint)",
                "root.channelSource.refreshDiscovery()",
                "Boolean(channel.roleKnown) && Boolean(channel.isModerator)",),
            "ui/app/social/channels.cpp": (
                "void ChannelPickerController::refreshDiscovery()",
                "if (m_viewerFid == 0) return {};",
                "const auto it = m_members.constFind(id);",),
            "ui/app/social/channel_administration.cpp": (
                'm_contract != QLatin1String("neynar-v2")',
                'm_writeProviderId != QLatin1String("neynar-managed")',
                "m_roleVerified && m_canModerate",
                "No verified pending invitation for this channel",),
        }
        for path, required in channels_contract.items():
            source = read(path)
            if any(token not in source for token in required):
                errors.append(f"CHANNELS: enabled but required scoped runtime "
                              f"or signer authorization is missing: {path}")

    # Search is already an implemented product path on every advertised Qt
    # target. Do not let merely setting -DSEARCH=ON advertise it if the
    # presentation bridge, provider-neutral read pipeline or cache disappears.
    if all("-DFARCASTER_ENABLE_SEARCH=ON" in block
           for block in (linux, macos, windows, android, web)):
        search_contract = {
            "src/application/configured_features.cpp.in": (
                ".hasSearch = @FARCASTER_ENABLE_SEARCH_CPP@",),
            "ui/components/shell/AppShell.qml": (
                "SearchPalette {",
                "onSuggestionsRequested: query => presentationBridge.requestSearchSuggestions(query)",),
            "src/presentation/qt/presentation_bridge.cpp": (
                "void PresentationBridge::requestSearchSuggestions(",
                "void PresentationBridge::clearSearchSuggestions()",
                "if (query.trimmed() != m_searchQuery)",),
            "ui/app/runtime/bindings.cpp": (
                "runtime->cancelSearchRequest();",
                "runtime->service->openSearch(",
                "bridge.applySearchSuggestions(",
                "runtime->keepSearchRequest(",),
            "src/application/read_async_service.cpp": (
                "ReadService::openSearch(",
                "m_store.searchCachedProfiles(query.text, query.limit)",
                "m_store.searchCachedChannels(query.text, query.limit)",
                "m_gateway.search(",
                "putChannelSuggestions(",
                "failedMetadata("),
            "src/storage/read_store.cpp": (
                "SqliteReadStore::searchCachedProfiles(",
                "SqliteReadStore::searchCachedChannels("),
        }
        for path, required in search_contract.items():
            source = read(path)
            if any(token not in source for token in required):
                errors.append(f"SEARCH: enabled but required runtime path is "
                              f"missing or incomplete: {path}")


    if "-DFARCASTER_ENABLE_MINIAPPS=ON" not in linux or (
            "-DGENYCASTER_REQUIRE_LINUX_MINIAPP_HOST=ON" not in linux):
        errors.append("Linux: native Mini Apps not required by Release")
    if "-DFARCASTER_ENABLE_MINIAPPS=ON" not in macos or (
            "farcaster_miniapp_browser" not in macos):
        errors.append("macOS: WKWebView Mini App host not verified by Release")
    # A Windows/Android build must have an actual native SDK host, not only
    # FARCASTER_ENABLE_MINIAPPS=ON or Android's public, bridge-free WebView.
    # A release must build and deploy the audited WebEngine runtime,
    # rather than merely setting a CMake definition.
    if not all(proof in ui for proof in (
            "GENYCASTER_HAS_WINDOWS_MINIAPP_HOST=1",
            "app/miniapp/browser_linux.cpp",
            "app/miniapp/browser_linux_quick.cpp",
            "Qt6::WebEngineWidgets Qt6::WebEngineQuick",
            "genycaster_windows_miniapp_quick")) or not all(
            proof in windows for proof in (
                "-DFARCASTER_ENABLE_MINIAPPS=ON",
                "farcaster_miniapp_browser",
                "Install separately versioned Qt WebEngine extension",
                "--modules qtwebengine --noarchives",
                "QtWebEngineProcess.exe",
                "qtwebenginequickplugin.dll")):
        errors.append("Windows: no verified origin-bound native Mini App SDK host")
    # Android's v1 Mini App scope is an explicitly advertised public-only
    # preview, not an EIP-1193 wallet host. This is a supported limitation,
    # provided that every layer retains the unprivileged origin-lock boundary.
    # A Release may never claim SDK support just because public pages render.
    android_scope = "-DGENYCASTER_ANDROID_MINIAPP_SCOPE=public-only"
    android_qml = read("ui/components/apps/AndroidWebView.qml")
    shell = read("ui/components/shell/AppShell.qml")
    cmake = read("CMakeLists.txt")
    public_only = (
        android_scope in android
        and "set(GENYCASTER_ANDROID_MINIAPP_SCOPE \"public-only\" CACHE STRING" in cmake
        and 'if(NOT GENYCASTER_ANDROID_MINIAPP_SCOPE STREQUAL "public-only")' in cmake
        and 'if(FARCASTER_ENABLE_MINIAPPS)' in cmake
        and "components/apps/AndroidWebView.qml" in ui
        and 'import QtWebView' in android_qml
        and "function publicOrigin(candidate)" in android_qml
        and "function blockForeignNavigation(index, destination)" in android_qml
        and "if (root.blockForeignNavigation(index, nextUrl)) stop()" in android_qml
        and 'url: model.blocked ? "" : String(model.url)' in android_qml
        and 'active: Qt.platform.os === "android"' in shell
        and "AndroidMiniAppWebView.qml" in shell
        and not re.search(
            r"runJavaScript\s*\(|addJavascriptInterface|nativeWallet|"
            r"signOpenProofWalletChallenge|GENYCASTER_HAS_ANDROID_MINIAPP_SDK_HOST"
            r"|WebChannel|WebEngine",
            android_qml, re.IGNORECASE)
    )
    if not public_only:
        errors.append(
            "Android: public-only Mini App scope is not explicitly declared "
            "and verified; privileged SDK/wallet access remains unavailable")

    # The browser uses its own origin-filtered Farcaster JS iframe host.
    # The native Qt Mini App *runtime* remains intentionally disabled in
    # WASM, but the C++ FeatureSet must advertise the browser counterpart.
    application = read("src/application/CMakeLists.txt")
    if re.search(r"set\(FARCASTER_ENABLE_MINIAPPS OFF CACHE BOOL", wasm) and not (
            "if(EMSCRIPTEN AND FARCASTER_BUILD_UI)" in application
            and "set(FARCASTER_ENABLE_MINIAPPS_CPP true)" in application
            and "miniapp-host-bridge-entry.mjs" in ui
            and "miniapp-wallet-provider.mjs" in ui
            and "miniapp-host-bridge.mjs" in ui
            and "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/wasm/miniapp-host-pre.js" in ui):
        errors.append("Web: browser Mini App SDK and advertised FeatureSet disagree")
    for path in ("ui/wasm/miniapp-host-bridge-entry.mjs",
                 "ui/wasm/miniapp-wallet-provider.mjs"):
        if not (ROOT / path).is_file():
            errors.append(f"Web: missing SDK implementation: {path}")

    # The deferred Wallet *product* is not the Mini App external wallet
    # transport. A release that disables or disconnects the actual external
    # provider while advertising Mini App wallet interactions must fail.
    external_wallet_contract = {
        "ui/wasm/miniapp-wallet-provider.mjs": (
            "export function createDiscoveredBrowserWallet(",
            "export function createMiniAppEthereumProvider(",
            "walletConnectFactory",
        ),
        "ui/wasm/host-sdk/walletconnect-entry.mjs": (
            "import UniversalProvider from '@walletconnect/universal-provider'",
            "export async function connectWalletConnect(",
            "asEthereumProvider(provider)",
        ),
        "ui/app/main.cpp": (
            "FARCASTER_HAS_MINIAPP_EVM_WALLET",
            "miniAppWalletBridgeOwner = std::move(*walletBridge)",
            "miniAppHost}](const std::string& url)",
        ),
        "ui/app/miniapp/runtime.cpp": (
            "GenyCaster (External Wallet)",
            "m_wallet->request(",
            "ensureWalletSession(session)",
        ),
    }
    for path, required in external_wallet_contract.items():
        source = read(path)
        if any(token not in source for token in required):
            errors.append(f"EXTERNAL_WALLET: Mini App external wallet transport "
                          f"is missing or disconnected: {path}")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--strict", action="store_true",
                        help="Return failure instead of reporting advisory blockers")
    args = parser.parse_args()
    outstanding = blockers()
    if not outstanding:
        print("Advertised cross-platform Release feature parity: PASS")
        return 0
    print("Advertised cross-platform Release feature parity: BLOCKED")
    for item in outstanding:
        print("  - " + item)
    if args.strict:
        print("No version-tag release may silently ship disabled features.")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
