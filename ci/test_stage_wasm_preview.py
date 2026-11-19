#!/usr/bin/env python3
"""Offline regression checks for preview-only WASM staging safety."""
from __future__ import annotations

import hashlib
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

from ci import stage_wasm_preview as stage


class StageWasmPreviewTests(unittest.TestCase):
    def test_sha256_manifest_is_exact_and_fail_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            bundle = p / "GenyCaster-v1.0.0-rc1-web-wasm.tar.gz"
            bundle.write_bytes(b"verified-source-bytes")
            digest = hashlib.sha256(bundle.read_bytes()).hexdigest()
            sums = p / "SHA256SUMS"
            sums.write_text(f"{digest}  {bundle.name}\n")
            self.assertEqual(stage.verify_checksum(bundle, sums), digest)
            sums.write_text(f"{'0'*64}  {bundle.name}\n")
            with self.assertRaisesRegex(ValueError, "SHA-256 differs"):
                stage.verify_checksum(bundle, sums)
            sums.write_text(f"{digest}  other.tar.gz\n")
            with self.assertRaisesRegex(ValueError, "matching"):
                stage.verify_checksum(bundle, sums)

    def test_root_must_be_exact_empty_and_vhost_disabled(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "app" / "public"
            root.mkdir(parents=True)
            with patch.object(stage, "APP_ROOT", root), \
                 patch.object(stage, "APP_VHOST", Path(tmp) / "vhost"):
                stage.check_root(root, offline=True)
                with self.assertRaises(ValueError):
                    stage.check_root(root, offline=False)
                with self.assertRaises(ValueError):
                    stage.check_root(Path(tmp), offline=True)
                (root / "existing.html").write_text("do not overwrite")
                with self.assertRaisesRegex(ValueError, "empty"):
                    stage.check_root(root, offline=True)
                (root / "existing.html").unlink()
                (Path(tmp) / "vhost").write_text("already live")
                with self.assertRaisesRegex(ValueError, "vhost already exists"):
                    stage.check_root(root, offline=True)

    def test_publish_promotes_entrypoint_last_and_preserves_assets(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "public"
            source = Path(tmp) / "stage"
            root.mkdir()
            source.mkdir()
            (source / "genycaster.html").write_text("preview")
            (source / "genycaster.js").write_text("loader")
            (source / "genycaster.wasm").write_bytes(b"\x00asm")
            stage.publish_once(source, root,
                               ["genycaster.html", "genycaster.wasm", "genycaster.js"])
            self.assertEqual((root / "genycaster.html").read_text(), "preview")
            self.assertEqual((root / "genycaster.wasm").read_bytes(), b"\x00asm")
            self.assertEqual((root / "genycaster.js").read_text(), "loader")

    def test_archive_extraction_rejects_path_traversal(self):
        with tempfile.TemporaryDirectory() as tmp:
            import io
            p = Path(tmp)
            archive = p / "malicious.tar.gz"
            with tarfile.open(archive, "w:gz") as out:
                info = tarfile.TarInfo("../outside.txt")
                info.size = len(b"malice")
                out.addfile(info, io.BytesIO(b"malice"))
            staging = p / "staging"
            staging.mkdir()
            with self.assertRaisesRegex(ValueError, "Unsafe"):
                stage.extract_to_stage(archive, staging)
            self.assertFalse((p / "outside.txt").exists())


if __name__ == "__main__":
    unittest.main()
