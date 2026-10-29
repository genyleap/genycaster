"""Portable, offline tests for release gates and artifact packaging."""
from __future__ import annotations

from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch
import sys
import unittest

from ci.package_web import select_files
from ci.release_artifacts import EXPECTED, main as assemble_release
from ci.release_gate import project_version, release_version


class PipelineTests(unittest.TestCase):
    def test_version_tag_gate(self) -> None:
        version = project_version(Path.cwd())
        self.assertEqual(release_version("refs/heads/main", version), (version, False))
        self.assertEqual(release_version(f"refs/tags/v{version}", version), (version, True))
        for bad in ("refs/tags/v1.2", "refs/tags/latest", "refs/tags/v01.2.3",
                    "refs/tags/v1.2.3-rc1", "refs/tags/v999.0.0"):
            with self.assertRaises(ValueError):
                release_version(bad, version)

    def test_web_archive_selection_excludes_build_artifacts(self) -> None:
        with TemporaryDirectory() as path:
            root = Path(path)
            (root / "farcaster_client.html").write_text("<html></html>")
            (root / "farcaster_client.wasm").write_bytes(b"\x00asm")
            (root / "farcaster_client_autogen").mkdir()
            (root / "farcaster_client_autogen" / "secret.js").write_text("ignored")
            (root / "farcaster_client.cpp.o").write_bytes(b"obj")
            selected = {p.name for p in select_files(root)}
            self.assertEqual(selected, {"farcaster_client.html", "farcaster_client.wasm"})

    def test_release_package_contract(self) -> None:
        self.assertEqual(len(EXPECTED), 5)
        self.assertEqual(EXPECTED["GenyCaster-android-arm64-v8a"], ".apk")
        self.assertEqual(EXPECTED["GenyCaster-web-wasm"], ".tar.gz")
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            artifacts = root / "artifacts"
            release = root / "release"
            for platform, extension in EXPECTED.items():
                directory = artifacts / platform
                directory.mkdir(parents=True)
                (directory / f"{platform}{extension}").write_bytes(platform.encode())
            with patch.object(sys, "argv", ["release_artifacts.py", str(artifacts), str(release)]):
                assemble_release()
            self.assertEqual(len(list(release.iterdir())), len(EXPECTED) + 1)
            self.assertEqual(len((release / "SHA256SUMS").read_text().splitlines()), len(EXPECTED))


if __name__ == "__main__":
    unittest.main()
