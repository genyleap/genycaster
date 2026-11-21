#!/usr/bin/env python3
"""Keep packaged macOS Intel startup validation headless without disabling it."""
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch

from ci.audit_macos_bundle import smoke


class MacOSSmokeBackendTests(unittest.TestCase):
    def run_smoke(self, arch: str):
        captured = []
        def fake_run(args, **kwargs):
            captured.append((args, kwargs))
            return subprocess.CompletedProcess(args, 0, "QML shell: PASS\n", "")
        with patch("ci.audit_macos_bundle.subprocess.run", side_effect=fake_run), \
             patch.dict("os.environ", {
                 "QSG_RHI_BACKEND": "software",
                 "QT_QUICK_BACKEND": "bad-ci-override",
                 "QT_PLUGIN_PATH": "/untrusted/qt-sdk",
             }):
            smoke(Path("/build/Genycaster.app"), arch)
        return captured

    def test_intel_uses_supported_software_scenegraph_not_invalid_rhi(self):
        calls = self.run_smoke("x86_64")
        self.assertEqual(len(calls), 1)
        args, kwargs = calls[0]
        self.assertIn("--smoke-test", args)
        env = kwargs["env"]
        self.assertEqual(env["QT_QPA_PLATFORM"], "cocoa")
        self.assertEqual(env["QT_QUICK_BACKEND"], "software")
        self.assertNotIn("QSG_RHI_BACKEND", env)
        self.assertNotIn("QT_PLUGIN_PATH", env)
        self.assertEqual(kwargs["timeout"], 120)

    def test_arm_uses_ordinary_native_renderer_without_ci_override(self):
        args, kwargs = self.run_smoke("arm64")[0]
        self.assertNotIn("QSG_RHI_BACKEND", kwargs["env"])
        self.assertNotIn("QT_QUICK_BACKEND", kwargs["env"])
        self.assertEqual(kwargs["timeout"], 45)

    def test_nonzero_smoke_still_fails_closed(self):
        with patch("ci.audit_macos_bundle.subprocess.run",
                   return_value=subprocess.CompletedProcess(["Genycaster"], 2,
                       "Smoke: root shell or feed readiness failed", "")):
            with self.assertRaisesRegex(ValueError, "failed startup smoke"):
                smoke(Path("/build/Genycaster.app"), "x86_64")


if __name__ == "__main__":
    unittest.main()
