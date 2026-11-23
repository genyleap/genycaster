#!/usr/bin/env python3
"""Release publication is explicit, complete, versioned, and bot-owned."""
from __future__ import annotations

import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parent.parent


class BotReleaseGateTests(unittest.TestCase):
    @staticmethod
    def run_gate(**override: str) -> subprocess.CompletedProcess[str]:
        env = {
            key: value for key, value in os.environ.items()
            if not key.startswith("GENYCASTER_")
        }
        env.update({
            "GENYCASTER_MANUAL_RELEASE": "true",
            "GENYCASTER_DISPATCH_PLATFORM": "all",
            "GENYCASTER_DISPATCH_ARCH": "all",
            "GENYCASTER_FULL_MATRIX_CONFIRMED": "true",
        })
        env.update(override)
        return subprocess.run(
            ["python3", "ci/release_gate.py", "--ref", "refs/heads/main",
             "--event", "workflow_dispatch"],
            cwd=ROOT, env=env, text=True, capture_output=True, timeout=20
        )

    def test_explicit_publication_of_full_main_matrix_is_allowed(self) -> None:
        result = self.run_gate()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("release: True", result.stdout)
        from ci.release_gate import release_designation
        self.assertIn("version: " + release_designation(ROOT), result.stdout)

    def test_ordinary_dispatch_does_not_publish(self) -> None:
        result = self.run_gate(GENYCASTER_MANUAL_RELEASE="false")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("release: False", result.stdout)

    def test_partial_or_unapproved_dispatch_fails_closed(self) -> None:
        for key, bad in (
            ("GENYCASTER_DISPATCH_PLATFORM", "android"),
            ("GENYCASTER_DISPATCH_ARCH", "arm64"),
            ("GENYCASTER_FULL_MATRIX_CONFIRMED", "false"),
        ):
            with self.subTest(key=key):
                result = self.run_gate(**{key: bad})
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("requires an approved full-matrix", result.stderr)

    def test_publication_cannot_be_armed_for_another_event(self) -> None:
        env = {
            "GENYCASTER_MANUAL_RELEASE": "true",
            "GENYCASTER_DISPATCH_PLATFORM": "all",
            "GENYCASTER_DISPATCH_ARCH": "all",
            "GENYCASTER_FULL_MATRIX_CONFIRMED": "true",
        }
        for ref, event in (
            ("refs/heads/feature", "workflow_dispatch"),
            ("refs/tags/v1.0.0", "workflow_dispatch"),
            ("refs/heads/main", "push"),
        ):
            with self.subTest(ref=ref, event=event):
                result = subprocess.run(
                    ["python3", "ci/release_gate.py",
                     "--ref", ref, "--event", event],
                    cwd=ROOT, env={**os.environ, **env},
                    capture_output=True, text=True, timeout=20)
                self.assertNotEqual(result.returncode, 0)

    def test_tag_creation_is_protected_by_github_token_and_all_artifacts(self):
        workflow = (ROOT / ".github/workflows/ci.yml").read_text()
        publish = workflow.split("\n  publish:\n", 1)[1]
        self.assertLess(publish.index("Verify mandatory packages and aggregate checksums"),
                        publish.index("Create verified Release tag as github-actions[bot]"))
        self.assertLess(publish.index("Create verified Release tag as github-actions[bot]"),
                        publish.index("Publish versioned Release as GitHub Actions bot"))
        self.assertIn("contents: write", publish)
        self.assertIn("GH_TOKEN: ${{ github.token }}", publish)
        self.assertIn("tagger[name]=github-actions[bot]", publish)
        self.assertIn("refs/heads/main", publish)
        self.assertIn("github.event_name == 'workflow_dispatch'", publish)
        self.assertIn("Refuse overwriting an existing release", publish)
        # workflow_dispatch GITHUB_REF_NAME is main, not v1.0.0.
        self.assertIn('cp "release-notes/v${{ needs.gate.outputs.version }}.md"', publish)
        self.assertNotIn('release-notes/${GITHUB_REF_NAME}.md', publish)


if __name__ == "__main__":
    unittest.main()
