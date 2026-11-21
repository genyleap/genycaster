"""The v1.0.0 refresh cannot create a release or publish test-signed artifacts."""
from __future__ import annotations

import os
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch

from ci import refresh_existing_release as refresh

ROOT = Path(__file__).resolve().parent.parent

class ExistingReleaseRefreshTests(unittest.TestCase):
    def run_gate(self, **values):
        env={key:value for key,value in os.environ.items()
             if not key.startswith("GENYCASTER_")}
        env.update({
            "GENYCASTER_MANUAL_RELEASE":"false",
            "GENYCASTER_REFRESH_EXISTING_RELEASE":"true",
            "GENYCASTER_DISPATCH_PLATFORM":"all",
            "GENYCASTER_DISPATCH_ARCH":"all",
            "GENYCASTER_FULL_MATRIX_CONFIRMED":"true",
        })
        env.update(values)
        return subprocess.run(
            ["python3","ci/release_gate.py","--ref","refs/heads/main",
             "--event","workflow_dispatch"],
            cwd=ROOT,env=env,text=True,capture_output=True,timeout=25,
        )

    def test_refresh_full_matrix_gets_permanent_release_signing(self):
        process=self.run_gate()
        self.assertEqual(process.returncode,0,process.stderr)
        self.assertIn("release: True",process.stdout)

    def test_refresh_may_not_create_another_release(self):
        result=self.run_gate(GENYCASTER_MANUAL_RELEASE="true")
        self.assertNotEqual(result.returncode,0)
        self.assertIn("Cannot create a new Release",result.stderr)

    def test_refresh_requires_main_complete_matrix(self):
        for key, value in (
            ("GENYCASTER_DISPATCH_PLATFORM","web"),
            ("GENYCASTER_DISPATCH_ARCH","arm64"),
            ("GENYCASTER_FULL_MATRIX_CONFIRMED","false"),
        ):
            with self.subTest(key=key):
                result=self.run_gate(**{key:value})
                self.assertNotEqual(result.returncode,0)

    def test_current_release_assets_match_exactly_and_exclude_ci_test_apk(self):
        self.assertEqual(len(refresh.EXPECTED_ASSETS),16)
        self.assertEqual(len(refresh.REQUIRED_ARTIFACTS),8)
        self.assertIn("GenyCaster-v1.0.0-android-arm64-v8a.apk",
                      refresh.EXPECTED_ASSETS)
        self.assertFalse(any("CI-TEST" in item for item in refresh.EXPECTED_ASSETS))
        self.assertNotIn("GenyCaster-v1.0.0-web-wasm.tar.gz",
                         refresh.EXPECTED_ASSETS)

    def test_metadata_change_or_mismatched_assets_is_rejected(self):
        identity={
            "id":453701445,
            "tag_name":"v1.0.0",
            "name":"Genycaster 1.0 Beta",
            "draft":False,
            "prerelease":False,
            "author":{"login":"github-actions[bot]"},
            "assets":[{"name":name} for name in refresh.EXPECTED_ASSETS],
        }
        result=refresh.release_identity(identity,"a"*40)
        self.assertEqual(result["id"],453701445)
        identity["assets"].pop()
        with self.assertRaisesRegex(ValueError,"asset inventory"):
            refresh.release_identity(identity,"a"*40)

    def test_workflow_uses_only_existing_release_upload_and_signed_full_matrix(self):
        workflow=(ROOT/".github/workflows/ci.yml").read_text()
        section=workflow.split("\n  refresh_existing:\n",1)[1]
        self.assertIn("needs: [gate, linux, macos, windows, android, web]",section)
        self.assertIn("inputs.publish_public_beta != true",section)
        self.assertIn("needs.gate.outputs.release == 'true'",section)
        self.assertIn("gh release upload v1.0.0 release/* --clobber",section)
        self.assertIn("ci/refresh_existing_release.py preflight",section)
        self.assertIn("ci/refresh_existing_release.py postflight",section)
        self.assertNotIn("gh release create",section)
        self.assertNotIn("gh release edit",section)
        self.assertNotIn("softprops/action-gh-release",section)
        self.assertNotIn("gh api -X POST",section)
        self.assertIn("GENYCASTER_REFRESH_EXISTING_RELEASE:",section)
        self.assertIn('SIGN_RELEASE: ${{ needs.gate.outputs.release }}', workflow)

if __name__=="__main__":
    unittest.main()
