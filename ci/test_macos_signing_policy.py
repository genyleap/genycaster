#!/usr/bin/env python3
"""Keep macOS Gatekeeper policy and Linux Qt SerialPort release prerequisites explicit.

These source checks complement the real macOS codesign/notarytool and Linux
linuxdeploy smoke checks; they do not claim host runtime validation.
"""
from __future__ import annotations

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parent.parent
WORKFLOW = ROOT / ".github/workflows/ci.yml"
SIGN = ROOT / "ci/sign_macos_bundle.sh"
NOTARY = ROOT / "ci/notarize_macos_dmg.sh"
PREFLIGHT = ROOT / "ci/preflight_linux_miniapp_host.py"
AUDIT = ROOT / "ci/audit_linux_appdir.py"


class MacOSSigningPolicyTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workflow = WORKFLOW.read_text(encoding="utf-8")
        cls.mac = cls.workflow.split("  macos:\n", 1)[1].split("  windows:\n", 1)[0]
        cls.linux = cls.workflow.split("  linux:\n", 1)[1].split("  macos:\n", 1)[0]

    def test_macos_adhoc_is_never_claimed_to_be_notarized(self):
        mac = self.mac
        self.assertIn('signing_mode="ad-hoc"', mac)
        self.assertIn('suffix="-adhoc-UNNOTARIZED"', mac)
        self.assertIn('signing_mode="developer-id"', mac)
        self.assertIn('bash ci/sign_macos_bundle.sh "$APP" "$signing_mode"', mac)
        self.assertIn('bash ci/notarize_macos_dmg.sh "$DMG"', mac)
        self.assertIn('Public macOS release requires Developer ID and Apple notarization', mac)
        self.assertIn('spctl --assess --type execute', mac)
        self.assertIn('codesign --verify --deep --strict', mac)
        self.assertLess(mac.index("ci/prune_macos_bundle.py"),
                        mac.index("ci/sign_macos_bundle.sh"))
        self.assertLess(mac.index("ci/sign_macos_bundle.sh"),
                        mac.index('ci/audit_macos_bundle.py "$APP"'))
        self.assertLess(mac.index("ci/notarize_macos_dmg.sh"),
                        mac.index('ci/audit_macos_bundle.py "$MOUNT/Genycaster.app"'))

    def test_signer_requires_nested_codesign_verification(self):
        signer = SIGN.read_text(encoding="utf-8")
        self.assertIn('find "$app/Contents" -depth -type d', signer)
        self.assertIn('codesign --verify --deep --strict', signer)
        self.assertIn('--options runtime --timestamp', signer)
        self.assertIn('properties/macOS/Genycaster.entitlements.plist', signer)
        self.assertNotIn('codesign --force --deep --sign', signer)

    def test_notary_requires_actual_apple_approval(self):
        code = NOTARY.read_text(encoding="utf-8")
        self.assertIn('notarytool submit', code)
        self.assertIn('result.get("status") != "Accepted"', code)
        self.assertIn('stapler staple', code)
        self.assertIn('stapler validate', code)

    def test_linux_positioning_plugin_dependency_must_be_installed(self):
        self.assertIn('--modules qtwebchannel qtpositioning qtserialport qtwebengine', self.linux)
        self.assertIn('"Qt6SerialPort"', PREFLIGHT.read_text(encoding="utf-8"))
        self.assertIn('"libQt6SerialPort.so.6"', AUDIT.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
