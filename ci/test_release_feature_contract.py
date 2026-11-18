#!/usr/bin/env python3
"""Ensure partial-platform feature support cannot be mistaken for a Release."""
from __future__ import annotations

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
        for platform, definition in (
            ("Windows", "GENYCASTER_HAS_WINDOWS_MINIAPP_HOST=1"),
            ("Android", "GENYCASTER_HAS_ANDROID_MINIAPP_SDK_HOST=1"),
        ):
            if definition not in ui:
                self.assertTrue(
                    any(item.startswith(platform + ":") for item in current),
                    f"Missing Mini App SDK backend not reported for {platform}",
                )
        if "set(FARCASTER_ENABLE_MINIAPPS OFF CACHE BOOL" in wasm:
            app = read("src/application/CMakeLists.txt")
            browser_feature = ("if(EMSCRIPTEN AND FARCASTER_BUILD_UI)" in app
                               and "set(FARCASTER_ENABLE_MINIAPPS_CPP true)" in app)
            self.assertEqual(
                browser_feature, not any(item.startswith("Web:") for item in current),
                "Native Mini Apps OFF must still advertise a bundled browser SDK")

    def test_complete_native_mini_apps_are_required(self):
        current = blockers()
        self.assertFalse(any(item.startswith("Linux:") for item in current))
        self.assertFalse(any(item.startswith("macOS:") for item in current))


    def test_release_search_is_enabled_with_a_real_read_pipeline(self):
        workflow = feature_contract.read(".github/workflows/ci.yml")
        blocks = (
            workflow.split("  linux:\\n", 1)[1].split("  macos:\\n", 1)[0],
            workflow.split("  macos:\\n", 1)[1].split("  windows:\\n", 1)[0],
            workflow.split("  windows:\\n", 1)[1].split("  android:\\n", 1)[0],
            workflow.split("  android:\\n", 1)[1].split("  ios:\\n", 1)[0],
            workflow.split("  web:\\n", 1)[1].split("  publish:\\n", 1)[0],
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
                    "            -DFARCASTER_ENABLE_SEARCH=ON\\n"
                    "            -DFARCASTER_REQUIRE_SECURE_SIGNER_VAULT=ON",
                    "            -DFARCASTER_REQUIRE_SECURE_SIGNER_VAULT=ON",
                )
            return source

        with patch.object(feature_contract, "read", side_effect=missing_android_search):
            self.assertTrue(any(item.startswith("SEARCH:") for item in blockers()))


if __name__ == "__main__":
    unittest.main()
