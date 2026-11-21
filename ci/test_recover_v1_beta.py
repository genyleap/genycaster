"""Safety tests for one-time reuse of seven intact v1.0.0 artifacts."""
import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from ci import recover_v1_beta as recovery


class RecoveredReleaseSafetyTests(unittest.TestCase):
    def test_whitelisted_recovery_changes_never_touch_application(self):
        self.assertEqual(len(recovery.PRESERVED), 7)
        self.assertEqual(len(set(recovery.PRESERVED)), 7)
        self.assertNotIn(recovery.INTEL, recovery.PRESERVED)
        self.assertIn("ci/audit_macos_bundle.py",
                      recovery.PERMITTED_RECOVERY_CHANGES)
        self.assertIn(".github/workflows/ci.yml",
                      recovery.PERMITTED_RECOVERY_CHANGES)
        self.assertFalse(any(p.startswith(("ui/", "src/", "services/"))
                             for p in recovery.PERMITTED_RECOVERY_CHANGES))
        self.assertEqual(len(recovery.BASE_SHA), 40)
        self.assertNotEqual(recovery.BASE_SHA, recovery.INTEL_SHA)

    def test_checksum_rejects_wrong_hash_without_ignoring_expected_version(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            filename = "GenyCaster-v1.0.0-macos-x86_64-adhoc-UNNOTARIZED.dmg"
            payload = b"verified build"
            (folder/filename).write_bytes(payload)
            checksum = hashlib.sha256(payload).hexdigest()
            (folder/"SHA256SUMS").write_text(
                f"{checksum}  {filename}\n")
            recovery.verify_checksum_inventory(folder)
            (folder/filename).write_bytes(b"changed build")
            with self.assertRaisesRegex(ValueError, "Invalid source artifact checksum"):
                recovery.verify_checksum_inventory(folder)
            (folder/"SHA256SUMS").write_text(
                f"{checksum}  ../{filename}\n")
            with self.assertRaisesRegex(ValueError, "Unsafe SHA256SUMS filename"):
                recovery.verify_checksum_inventory(folder)

    def test_recovery_publish_has_no_other_platform_builds(self):
        workflow = (Path(__file__).resolve().parents[1] /
                    ".github/workflows/recover-v1-beta.yml").read_text()
        self.assertIn("confirm_recovery:", workflow)
        self.assertIn("default: false", workflow)
        self.assertIn("actions: read", workflow)
        self.assertIn("contents: write", workflow)
        self.assertIn("python3 ci/recover_v1_beta.py --preflight", workflow)
        self.assertIn("python3 ci/recover_v1_beta.py --download artifacts", workflow)
        self.assertIn("python3 ci/release_artifacts.py artifacts release", workflow)
        self.assertIn("sha256sum --check --strict SHA256SUMS", workflow)
        self.assertIn("tagger[name]=github-actions[bot]", workflow)
        self.assertIn("softprops/action-gh-release@v2", workflow)
        self.assertIn("prerelease: true", workflow)
        self.assertIn("draft: false", workflow)
        self.assertIn("f5016b6dcddc42dc464146941d2b980dc1c646d7", workflow)
        for forbidden in ("cmake --build", "install-qt-action",
                          "make -j", "gradlew", "ninja "):
            self.assertNotIn(forbidden, workflow)

    def test_recovery_provenance_rejects_wrong_head(self):
        with patch.object(recovery, "command") as invoke:
            invoke.side_effect = [
                "ui/app/main.cpp\n",
            ]
            with self.assertRaisesRegex(ValueError, "application files"):
                recovery.unchanged_application_source()


if __name__ == "__main__":
    unittest.main()
