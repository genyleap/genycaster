#!/usr/bin/env python3
"""Fast guard for accidental per-commit CI/release triggers and release bypasses."""
from __future__ import annotations

from pathlib import Path
import re


def main() -> None:
    workflow = Path(".github/workflows/ci.yml").read_text(encoding="utf-8")
    assert re.search(r"^on:\n  workflow_dispatch:", workflow, re.M)
    assert re.search(r"^  push:\n    tags:\n      - 'v\*'", workflow, re.M)
    assert not re.search(r"^  (branches|pull_request|schedule|release):", workflow, re.M)
    assert not re.search(r"^    branches:", workflow, re.M)
    assert re.search(r"^  publish:\n", workflow, re.M)
    assert "github.event_name == 'push' && needs.gate.outputs.release == 'true'" in workflow
    assert "needs: [gate, core, linux, macos, windows, android, web]" in workflow
    assert "FARCASTER_REQUIRE_SECURE_SIGNER_VAULT=ON" in workflow
    assert "ANDROID_KEYSTORE_BASE64" in workflow
    assert "if-no-files-found: error" in workflow
    assert "Refuse overwriting an existing release" in workflow
    assert "ci/release_gate.py" in workflow
    assert "ci/release_artifacts.py" in workflow
    print("CI trigger and release gate invariants passed")


if __name__ == "__main__":
    main()
