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
    cmake = read("CMakeLists.txt")
    ui = read("ui/CMakeLists.txt")
    workflow = read(".github/workflows/ci.yml")
    wasm = read("cmake/platform/WebAssembly/configure.cmake")
    errors: list[str] = []

    linux = workflow.split("  linux:\n", 1)[1].split("  macos:\n", 1)[0]
    macos = workflow.split("  macos:\n", 1)[1].split("  windows:\n", 1)[0]
    windows = workflow.split("  windows:\n", 1)[1].split("  android:\n", 1)[0]
    android = workflow.split("  android:\n", 1)[1].split("  ios:\n", 1)[0]
    web = workflow.split("  web:\n", 1)[1].split("  publish:\n", 1)[0]

    # Every advertised product capability must exist in the published
    # FeatureSet, not just as an unconnected QML navigation item.
    for feature in ("CHANNELS", "CHAT", "SEARCH", "WALLET"):
        if re.search(rf"option\(FARCASTER_ENABLE_{feature}\b[^\n]*\bOFF\)", cmake):
            option = f"-DFARCASTER_ENABLE_{feature}=ON"
            if any(option not in block for block in (linux, macos, windows, android, web)):
                errors.append(f"{feature}: disabled in at least one platform's "
                              "product composition / Release configuration")

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
    if "GENYCASTER_HAS_ANDROID_MINIAPP_SDK_HOST=1" not in ui:
        errors.append("Android: only unprivileged public WebView, no Mini App SDK host")

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
