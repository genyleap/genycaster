#!/usr/bin/env python3
"""Ensure partial-platform feature support cannot be mistaken for a Release."""
from __future__ import annotations

import unittest

from ci.verify_release_feature_contract import blockers


class ReleaseFeatureContractTest(unittest.TestCase):
    def test_missing_backends_are_reported_before_expensive_tag_builds(self):
        current = blockers()
        # The current application has two native host implementations.
        # Keep missing-platform adapters visible until replaced by real code.
        self.assertTrue(any(item.startswith("Windows:") for item in current))
        self.assertTrue(any(item.startswith("Android:") for item in current))
        self.assertTrue(any(item.startswith("Web:") for item in current))

    def test_complete_native_mini_apps_are_required(self):
        current = blockers()
        self.assertFalse(any(item.startswith("Linux:") for item in current))
        self.assertFalse(any(item.startswith("macOS:") for item in current))


if __name__ == "__main__":
    unittest.main()
