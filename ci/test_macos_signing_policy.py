#!/usr/bin/env python3
"""Keep macOS Gatekeeper policy and Linux Qt SerialPort release prerequisites explicit.

These source checks complement the real macOS codesign/notarytool and Linux
linuxdeploy smoke checks; they do not claim host runtime validation.
"""
from __future__ import annotations

from pathlib import Path
import unittest
import os
import subprocess
import tempfile
import hashlib

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
        self.assertIn('signing_mode="$(bash ci/select_macos_signing_mode.sh)"', mac)
        self.assertIn('suffix="-adhoc-UNNOTARIZED"', mac)
        self.assertIn('suffix="-selfsigned-UNNOTARIZED"', mac)
        self.assertIn('bash ci/verify_macos_self_signed.sh "$MOUNT/Genycaster.app"', mac)
        self.assertIn('bash ci/sign_macos_bundle.sh "$APP" "$signing_mode"', mac)
        self.assertIn('bash ci/notarize_macos_dmg.sh "$DMG"', mac)
        self.assertIn('REQUIRE_MACOS_RELEASE_SIGNING', mac)
        self.assertIn('Require persistent macOS release signing before installing toolchains', self.workflow)
        self.assertIn('MACOS_SELF_SIGNED_CERT_SHA1', mac)
        self.assertIn('NOT Apple Developer ID signed', self.workflow)
        self.assertIn('spctl --assess --type execute', mac)
        self.assertIn('codesign --verify --deep --strict', mac)
        self.assertLess(mac.index("ci/prune_macos_bundle.py"),
                        mac.index("ci/sign_macos_bundle.sh"))
        self.assertLess(mac.index("ci/sign_macos_bundle.sh"),
                        mac.index('ci/audit_macos_bundle.py "$APP"'))
        self.assertIn("Preserve signed Intel bundle if the CI smoke fails", mac)
        self.assertIn("GenyCaster-macos-intel-signed-recovery", mac)
        self.assertLess(mac.index("Genycaster-intel-signed.app.zip"),
                        mac.index('ci/audit_macos_bundle.py "$APP"'))
        self.assertLess(mac.index("ci/notarize_macos_dmg.sh"),
                        mac.index('ci/audit_macos_bundle.py "$MOUNT/Genycaster.app"'))

    def test_signing_mode_selection_fail_closed(self):
        keys = ("MACOS_SELF_SIGNED_P12_BASE64", "MACOS_SELF_SIGNED_P12_PASSWORD",
                "MACOS_SELF_SIGNED_CERT_SHA1")
        paid = ("MACOS_DEVELOPER_ID_P12_BASE64", "MACOS_DEVELOPER_ID_P12_PASSWORD",
                "MACOS_NOTARY_KEY_P8_BASE64", "MACOS_NOTARY_KEY_ID", "MACOS_NOTARY_ISSUER_ID")
        base = {k: v for k, v in os.environ.items() if k not in (*keys, *paid, "REQUIRE_MACOS_RELEASE_SIGNING")}
        self_signed = dict(zip(keys, ("fixture", "fixture", "a" * 40)))
        developer = dict.fromkeys(paid, "fixture")
        cases = [
            ({}, "ad-hoc"), ({"REQUIRE_MACOS_RELEASE_SIGNING": "true"}, None),
            ({keys[0]: "fixture"}, None), (self_signed, "self-signed"),
            ({**self_signed, "REQUIRE_MACOS_RELEASE_SIGNING": "true"}, "self-signed"),
            ({**self_signed, keys[2]: "wrong"}, None),
            (developer, "developer-id"), ({paid[0]: "fixture"}, None),
            ({**developer, **self_signed}, "developer-id"),
            ({**self_signed, paid[0]: "fixture"}, None),
        ]
        for extra, expected in cases:
            with self.subTest(extra=extra):
                result = subprocess.run(["bash", str(ROOT / "ci/select_macos_signing_mode.sh")],
                                        env={**base, **extra}, text=True, capture_output=True)
                if expected is None:
                    self.assertNotEqual(result.returncode, 0)
                else:
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(result.stdout.strip(), expected)

    def test_self_signed_catalog_preserves_trust_disclosure(self):
        from ci.generate_native_release_catalog import generate, EXPECTED
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            sums = []
            for artifact, suffix in EXPECTED.items():
                name = "GenyCaster-v1.0.0-" + artifact.removeprefix("GenyCaster-") + suffix
                if artifact.startswith("GenyCaster-macos-"):
                    name = name.removesuffix(".dmg") + "-selfsigned-UNNOTARIZED.dmg"
                data = name.encode()
                (root / name).write_bytes(data)
                sums.append(f"{hashlib.sha256(data).hexdigest()}  {name}")
            (root / "SHA256SUMS").write_text("\n".join(sums) + "\n")
            native, landing = generate(root, "v1.0.0")
            for record in native["platforms"]["macos"].values():
                self.assertEqual(record["distribution"], "self-signed-unnotarized")
                self.assertTrue(record["url"].endswith("-selfsigned-UNNOTARIZED.dmg"))
            for record in landing["platforms"]["macos"]["assets"]:
                self.assertEqual(record["distribution"], "self-signed-unnotarized")
            (root / "GenyCaster-v1.0.0-macos-arm64-adhoc-UNNOTARIZED.dmg").write_bytes(b"old")
            with self.assertRaisesRegex(ValueError, "ambiguous"):
                generate(root, "v1.0.0")

    def test_dmg_verification_retries_only_transient_busy_runner_state(self):
        mac = self.mac
        self.assertIn('verify_output="$(hdiutil verify "$DMG" 2>&1)"', mac)
        self.assertIn('for attempt in 1 2 3 4; do', mac)
        self.assertIn('*"Resource temporarily unavailable"*', mac)
        self.assertIn('"$attempt" -eq 4', mac)
        self.assertIn('test "$dmg_verified" -eq 1', mac)
        self.assertIn('DMG integrity verification failed', mac)
        self.assertLess(mac.index('test "$dmg_verified" -eq 1'),
                        mac.index('ci/notarize_macos_dmg.sh'))

    def test_signer_requires_nested_codesign_verification(self):
        for script in ("sign_macos_bundle.sh", "select_macos_signing_mode.sh",
                       "verify_macos_self_signed.sh", "create_macos_self_signed_identity.sh"):
            subprocess.run(["bash", "-n", str(ROOT / "ci" / script)], check=True)
        signer = SIGN.read_text(encoding="utf-8")
        self.assertIn('case "${binary#"$app/Contents/"}" in', signer)
        self.assertIn('certificate leaf = H', signer)
        self.assertIn('security remove-trusted-cert "$trusted_cert"', signer)
        self.assertIn('bash ci/verify_macos_self_signed.sh "$app"', signer)
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
