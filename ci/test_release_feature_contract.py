#!/usr/bin/env python3
"""Ensure partial-platform feature support cannot be mistaken for a Release."""
from __future__ import annotations

import re
import unittest
from unittest.mock import patch

from ci.verify_release_feature_contract import blockers
from ci import verify_release_feature_contract as feature_contract


class ReleaseFeatureContractTest(unittest.TestCase):
    def test_missing_backends_are_reported_before_expensive_tag_builds(self):
        from ci.verify_release_feature_contract import read

        current = blockers()
        ui = read("ui/CMakeLists.txt")
        wasm = read("cmake/platform/WebAssembly/configure.cmake")
        if "GENYCASTER_HAS_WINDOWS_MINIAPP_HOST=1" not in ui:
            self.assertTrue(
                any(item.startswith("Windows:") for item in current),
                "A missing Windows Mini App SDK host must block Release",
            )
        # Android v1 intentionally admits an origin-locked public WebView,
        # not a privileged SDK or EIP-1193 wallet provider.
        if "GENYCASTER_HAS_ANDROID_MINIAPP_SDK_HOST=1" not in ui:
            self.assertFalse(
                any(item.startswith("Android:") for item in current),
                "The audited Android public-only variant is a valid v1 scope",
            )
        if "set(FARCASTER_ENABLE_MINIAPPS OFF CACHE BOOL" in wasm:
            app = read("src/application/CMakeLists.txt")
            browser_feature = ("if(EMSCRIPTEN AND FARCASTER_BUILD_UI)" in app
                               and "set(FARCASTER_ENABLE_MINIAPPS_CPP true)" in app)
            browser_sdk_blocked = any(
                item.startswith("Web: browser Mini App SDK")
                or item.startswith("Web: missing SDK implementation:")
                for item in current)
            self.assertEqual(
                browser_feature, not browser_sdk_blocked,
                "Native Mini Apps OFF must still advertise a bundled browser SDK")

    def test_complete_native_mini_apps_are_required(self):
        current = blockers()
        self.assertFalse(any(item.startswith("Linux:") for item in current))
        self.assertFalse(any(item.startswith("macOS:") for item in current))


    def test_cmake_default_on_cannot_hide_missing_release_feature(self):
        original = feature_contract.read

        def weakened_release(path):
            source = original(path)
            if path == "CMakeLists.txt":
                source, replaced = re.subn(
                    r'(option\(FARCASTER_ENABLE_CHANNELS\b[^\n]*\b)OFF\)',
                    r'\1ON)',
                    source,
                )
                self.assertEqual(replaced, 1)
            if path == ".github/workflows/ci.yml":
                source = source.replace(
                    "-DFARCASTER_ENABLE_CHANNELS=ON",
                    "-DFARCASTER_ENABLE_CHANNELS=OFF",
                    1,
                )
            return source

        with patch.object(feature_contract, "read", side_effect=weakened_release):
            self.assertTrue(
                any(item.startswith("CHANNELS:") for item in blockers()),
                "Release validation must not trust a changed CMake default",
            )

    def test_release_search_is_enabled_with_a_real_read_pipeline(self):
        workflow = feature_contract.read(".github/workflows/ci.yml")
        blocks = (
            workflow.split("  linux:\n", 1)[1].split("  macos:\n", 1)[0],
            workflow.split("  macos:\n", 1)[1].split("  windows:\n", 1)[0],
            workflow.split("  windows:\n", 1)[1].split("  android:\n", 1)[0],
            workflow.split("  android:\n", 1)[1].split("  ios:\n", 1)[0],
            workflow.split("  web:\n", 1)[1].split("  publish:\n", 1)[0],
        )
        self.assertTrue(all("-DFARCASTER_ENABLE_SEARCH=ON" in block for block in blocks))
        self.assertFalse(any(item.startswith("SEARCH:") for item in blockers()))

    def test_search_flag_alone_is_insufficient_without_real_runtime(self):
        original = feature_contract.read

        def missing_search_gateway(path):
            source = original(path)
            if path == "ui/app/runtime/bindings.cpp":
                return source.replace("runtime->service->openSearch(",
                                      "runtime->service->openUnavailable(")
            return source

        with patch.object(feature_contract, "read", side_effect=missing_search_gateway):
            self.assertTrue(any(item.startswith("SEARCH:") for item in blockers()))

    def test_missing_search_release_target_fails_closed(self):
        original = feature_contract.read

        def missing_android_search(path):
            source = original(path)
            if path == ".github/workflows/ci.yml":
                source = source.replace(
                    "            -DFARCASTER_ENABLE_SEARCH=ON\n"
                    "            -DFARCASTER_ENABLE_CHANNELS=ON\n"
                    "            -DFARCASTER_REQUIRE_SECURE_SIGNER_VAULT=ON",
                    "            -DFARCASTER_REQUIRE_SECURE_SIGNER_VAULT=ON",
                )
            return source

        with patch.object(feature_contract, "read", side_effect=missing_android_search):
            self.assertTrue(any(item.startswith("SEARCH:") for item in blockers()))


    def test_release_channels_requires_genuine_discovery_and_role_guards(self):
        self.assertFalse(any(item.startswith("CHANNELS:") for item in blockers()))

    def test_channels_flag_alone_cannot_bypass_verified_moderator_authorization(self):
        original = feature_contract.read

        def missing_role_proof(path):
            source = original(path)
            if path == "ui/app/social/channel_administration.cpp":
                return source.replace("m_roleVerified && m_canModerate",
                                      "m_canModerate")
            return source

        with patch.object(feature_contract, "read", side_effect=missing_role_proof):
            self.assertTrue(any(item.startswith("CHANNELS:") for item in blockers()))

    def test_channels_release_cannot_skip_wasm(self):
        original = feature_contract.read

        def missing_wasm_channels(path):
            source = original(path)
            if path == ".github/workflows/ci.yml":
                source = source.replace(
                    '            -DFARCASTER_ENABLE_CHANNELS=ON \\\n'
                    '            -DFARCASTER_ENABLE_CHAT=OFF',
                    '            -DFARCASTER_ENABLE_CHAT=OFF',
                )
            return source

        with patch.object(feature_contract, "read", side_effect=missing_wasm_channels):
            self.assertTrue(any(item.startswith("CHANNELS:") for item in blockers()))

    def test_release_defers_standalone_chat_and_wallet_without_disabling_custody(self):
        workflow = feature_contract.read(".github/workflows/ci.yml")
        blocks = (
            workflow.split("  linux:\n", 1)[1].split("  macos:\n", 1)[0],
            workflow.split("  macos:\n", 1)[1].split("  windows:\n", 1)[0],
            workflow.split("  windows:\n", 1)[1].split("  android:\n", 1)[0],
            workflow.split("  android:\n", 1)[1].split("  ios:\n", 1)[0],
            workflow.split("  web:\n", 1)[1].split("  publish:\n", 1)[0],
        )
        for block in blocks:
            for feature in ("CHAT", "WALLET"):
                self.assertIn(f"-DFARCASTER_ENABLE_{feature}=OFF", block)
        self.assertFalse(any(message.startswith(("CHAT:", "WALLET:")) for message in blockers()))
        self.assertIn("FARCASTER_REQUIRE_SECURE_SIGNER_VAULT=ON", workflow)
        self.assertIn("app/identity/native_wallet.cpp", feature_contract.read("ui/CMakeLists.txt"))

    def test_deferred_features_cannot_be_implicitly_enabled_on_one_target(self):
        original = feature_contract.read

        for feature in ("CHAT", "WALLET"):
            def incorrectly_advertised(path):
                source = original(path)
                if path == ".github/workflows/ci.yml":
                    source = source.replace(
                        f"-DFARCASTER_ENABLE_{feature}=OFF",
                        f"-DFARCASTER_ENABLE_{feature}=ON", 1)
                return source

            with self.subTest(feature=feature):
                with patch.object(feature_contract, "read", side_effect=incorrectly_advertised):
                    self.assertTrue(
                        any(message.startswith(feature + ":") for message in blockers()))

    def test_android_requires_explicit_public_only_scope_and_unprivileged_qml(self):
        self.assertFalse(any(message.startswith("Android:") for message in blockers()))
        original = feature_contract.read

        modifications = (
            (".github/workflows/ci.yml",
             "-DGENYCASTER_ANDROID_MINIAPP_SCOPE=public-only",
             "-DGENYCASTER_ANDROID_MINIAPP_SCOPE=full-sdk"),
            ("ui/components/apps/AndroidWebView.qml",
             "function blockForeignNavigation(index, destination)",
             "function navigateWithoutOriginCheck(index, destination)"),
            ("ui/components/apps/AndroidWebView.qml",
             "import QtWebView",
             "import QtWebChannel\nimport QtWebView"),
            ("CMakeLists.txt",
             'if(NOT GENYCASTER_ANDROID_MINIAPP_SCOPE STREQUAL "public-only")',
             'if(FALSE)'),
        )
        for target, before, after in modifications:
            def weakened(path):
                source = original(path)
                if path == target:
                    self.assertIn(before, source)
                    return source.replace(before, after, 1)
                return source
            with self.subTest(path=target, mutation=after):
                with patch.object(feature_contract, "read", side_effect=weakened):
                    self.assertTrue(
                        any(message.startswith("Android:") for message in blockers()))

    def test_android_cmake_refuses_claims_of_privileged_mini_app_sdk(self):
        source = feature_contract.read("CMakeLists.txt")
        self.assertIn('if(FARCASTER_ENABLE_MINIAPPS)', source)
        self.assertIn("Android has no privileged Mini App SDK host.", source)
        self.assertIn('if(NOT GENYCASTER_ANDROID_MINIAPP_SCOPE STREQUAL "public-only")', source)

    def test_external_wallet_transport_is_required_despite_deferred_wallet_product(self):
        self.assertFalse(
            any(message.startswith("EXTERNAL_WALLET:") for message in blockers()))
        original = feature_contract.read
        mutations = (
            ("ui/wasm/miniapp-wallet-provider.mjs",
             "export function createMiniAppEthereumProvider(",
             "export function removedEthereumProvider("),
            ("ui/wasm/host-sdk/walletconnect-entry.mjs",
             "export async function connectWalletConnect(",
             "export async function unsupportedWalletConnect("),
            ("ui/app/miniapp/runtime.cpp",
             "m_wallet->request(", "m_wallet->unavailable("),
        )
        for target, before, after in mutations:
            def missing_bridge(path):
                source = original(path)
                if path == target:
                    self.assertIn(before, source)
                    return source.replace(before, after)
                return source
            with self.subTest(path=target):
                with patch.object(feature_contract, "read", side_effect=missing_bridge):
                    self.assertTrue(any(
                        message.startswith("EXTERNAL_WALLET:") for message in blockers()))


if __name__ == "__main__":
    unittest.main()
