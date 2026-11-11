#!/usr/bin/env python3
"""Regression: otool -L headers from a mounted DMG are not dependencies."""
from __future__ import annotations

import unittest
from pathlib import Path

try:
    from ci.audit_macos_bundle import otool_dependencies
except ModuleNotFoundError:
    from audit_macos_bundle import otool_dependencies


class OtoolHeaderTest(unittest.TestCase):
    def test_mounted_dmg_arm64_header_is_not_a_library(self) -> None:
        binary = Path("/Users/runner/work/_temp/genycaster-dmg-arm64/"
                      "Genycaster.app/Contents/Frameworks/libswresample.7.dylib")
        output = (f"{binary} (architecture arm64):\n"
                  "\t@rpath/libavutil.59.dylib "
                  "(compatibility version 0.0.0, current version 59.0.0)\n"
                  "\t/usr/lib/libSystem.B.dylib "
                  "(compatibility version 1.0.0, current version 1.0.0)\n")
        self.assertEqual(otool_dependencies(output, binary),
                         ["@rpath/libavutil.59.dylib", "/usr/lib/libSystem.B.dylib"])

    def test_multi_arch_headers_and_real_forbidden_dependency(self) -> None:
        binary = Path("/Applications/Test.app/Contents/MacOS/Test")
        output = (f"{binary} (architecture arm64):\n"
                  "\t/opt/homebrew/lib/libbad.dylib "
                  "(compatibility version 1.0.0, current version 1.0.0)\n"
                  f"{binary} (architecture x86_64):\n"
                  "\t@rpath/QtCore.framework/Versions/A/QtCore "
                  "(compatibility version 6.0.0, current version 6.12.0)\n")
        self.assertEqual(otool_dependencies(output, binary),
                         ["/opt/homebrew/lib/libbad.dylib",
                          "@rpath/QtCore.framework/Versions/A/QtCore"])


if __name__ == "__main__":
    unittest.main()
