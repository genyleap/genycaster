#!/usr/bin/env python3
"""Guard against accidentally publishing or disabling a partial platform build.

This protects CI YAML source from duplicated stages or truncated commands,
notably the Linux and macOS release workflows. Real builds and package audits
remain mandatory and are not replaced by these structural assertions.
"""
from __future__ import annotations

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parent.parent
WORKFLOW = ROOT / ".github/workflows/ci.yml"
LOCAL = ROOT / "ci/build_local_linux_release.sh"

def job_source(workflow: str, name: str, following: str) -> str:
    begin = "\n  " + name + ":\n"
    end = "\n  " + following + ":\n"
    if workflow.count(begin) != 1 or workflow.count(end) != 1:
        raise AssertionError(f"Expected exactly one {name}/{following} job")
    return workflow.split(begin, 1)[1].split(end, 1)[0]

class ReleaseWorkflowIntegrityTest(unittest.TestCase):
    def test_single_complete_linux_stage(self) -> None:
        s = WORKFLOW.read_text(encoding="utf-8")
        linux = job_source(s, "linux", "macos")
        for marker in (
            "      - name: Build Qt client",
            "      - name: Package AppImage",
            "          python3 ci/stage_linux_webengine.py AppDir",
            "          python3 ci/bundle_linux_webengine_runtime.py AppDir",
            "          cmake --build build/native --target genycaster",
            "          grep -q '^FARCASTER_ENABLE_MINIAPPS:BOOL=ON$'",
            "          grep -q '^GENYCASTER_REQUIRE_LINUX_MINIAPP_HOST:BOOL=ON$'",
            "          python3 ci/audit_linux_appdir.py AppDir --require-miniapps",
        ):
            self.assertIn(marker, linux)
        self.assertEqual(linux.count("      - name: Package AppImage"), 1)
        self.assertEqual(linux.count("          cmake --build build/native --target genycaster"), 1)
        self.assertEqual(linux.count("          python3 ci/stage_linux_webengine.py AppDir"), 1)
        self.assertEqual(linux.count("          python3 ci/bundle_linux_webengine_runtime.py AppDir"), 1)

    def test_macos_has_one_valid_full_webkit_build_and_dmg_audit(self) -> None:
        s = WORKFLOW.read_text(encoding="utf-8")
        mac = job_source(s, "macos", "windows")
        self.assertEqual(mac.count("      - name: Build and deploy macOS application"), 1)
        self.assertEqual(mac.count("          cmake --build build/native --target genycaster"), 1)
        self.assertEqual(mac.count("          cmake --build build/native --target farcaster_miniapp_browser"), 1)
        self.assertIn("          grep -q '^FARCASTER_ENABLE_MINIAPPS:BOOL=ON$'", mac)
        self.assertIn('          APP=build/native/bin/Genycaster.app', mac)
        self.assertIn('ci/audit_macos_bundle.py "$APP"', mac)
        self.assertIn('ci/audit_macos_bundle.py "$MOUNT/Genycaster.app"', mac)
        self.assertEqual(mac.count("name: GenyCaster-macos-${{ matrix.arch }}"), 1)

    def test_local_linux_pipeline_not_repeated(self) -> None:
        s = LOCAL.read_text(encoding="utf-8")
        self.assertEqual(s.count("python3 ci/stage_linux_webengine.py"), 1)
        self.assertEqual(s.count("python3 ci/bundle_linux_webengine_runtime.py"), 1)
        self.assertEqual(s.count('cmake --build "$build_dir" --target genycaster'), 1)
        self.assertEqual(s.count("printf '\\nLocal %s release-equivalent packages:"), 1)
        self.assertIn("grep -q '^FARCASTER_ENABLE_MINIAPPS:BOOL=ON$'", s)

    def test_only_tag_pushes_publish_and_feature_gate_blocks_partial_release(self) -> None:
        s = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("    tags:\n      - 'v*'", s)
        self.assertIn("python3 ci/verify_release_feature_contract.py --strict", s)
        self.assertIn("          python3 ci/audit_linux_appdir.py AppDir --require-miniapps", s)
        # Truncated grep commands were accepted as YAML block text, then
        # silently skipped when the remaining steps were duplicated.
        self.assertNotRegex(s, re.compile(r"grep -q '\^FARCASTER_ENABLE_MINIAPPS:BOOL=ON\n"))

if __name__ == "__main__":
    unittest.main()
