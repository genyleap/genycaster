#!/usr/bin/env python3
"""Ensure partial-platform feature support cannot be mistaken for a Release."""
from __future__ import annotations

import unittest

from ci.verify_release_feature_contract import blockers


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


if __name__ == "__main__":
    unittest.main()
