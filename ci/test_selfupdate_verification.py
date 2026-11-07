"""Adversarial acceptance tests for native runtime snapshot artifacts."""
from __future__ import annotations

import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
import zipfile

from ci.package_selfupdate import create_snapshot
from ci.verify_selfupdate_snapshot import verify_snapshot


class NativeSnapshotVerificationTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="genycaster-snapshot-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def snapshot(self, platform: str = "windows") -> Path:
        source = self.root / ("mac-source" if platform == "macos" else "win-source")
        executable = (source / "Contents/MacOS/Genycaster"
                      if platform == "macos" else source / "Genycaster.exe")
        executable.parent.mkdir(parents=True, exist_ok=True)
        executable.write_bytes(b"GenyCaster test executable\x00")
        (source / ("Contents/Resources/info.dat" if platform == "macos"
                   else "resources/info.dat")).parent.mkdir(parents=True, exist_ok=True)
        (source / ("Contents/Resources/info.dat" if platform == "macos"
                   else "resources/info.dat")).write_bytes(b"changed-file-hash-check")
        return create_snapshot(platform, "arm64", "1.0.0-rc1",
                               source, self.root / "dist")

    def test_windows_snapshot_checks_every_manifest_hash(self) -> None:
        result = verify_snapshot(self.snapshot())
        self.assertEqual(result["platform"], "windows")
        self.assertEqual(result["installAction"], "unsupported")
        self.assertEqual(len(result["files"]), 2)

    def test_macos_snapshot_checks_every_manifest_hash(self) -> None:
        result = verify_snapshot(self.snapshot("macos"))
        self.assertEqual(result["platform"], "macos")
        self.assertEqual(len(result["files"]), 2)

    def test_changed_payload_rejected_even_when_file_size_matches(self) -> None:
        original = self.snapshot()
        corrupted = self.root / "corrupted.zip"
        with zipfile.ZipFile(original) as before, zipfile.ZipFile(corrupted, "w") as after:
            for name in before.namelist():
                data = before.read(name)
                if name == "app/Genycaster.exe":
                    data = b"X" + data[1:]
                after.writestr(name, data)
        with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
            verify_snapshot(corrupted)

    def test_manifest_cannot_claim_automatic_unsigned_installation(self) -> None:
        original = self.snapshot()
        malicious = self.root / "unsupported-action.zip"
        with zipfile.ZipFile(original) as before, zipfile.ZipFile(malicious, "w") as after:
            for name in before.namelist():
                data = before.read(name)
                if name == "selfupdate.json":
                    doc = json.loads(data)
                    doc["installAction"] = "replace-running-executable"
                    data = json.dumps(doc).encode()
                after.writestr(name, data)
        with self.assertRaisesRegex(ValueError, "Unknown/unapproved"):
            verify_snapshot(malicious)

    def test_zip_path_traversal_rejected(self) -> None:
        original = self.snapshot()
        malicious = self.root / "traversal.zip"
        with zipfile.ZipFile(original) as before, zipfile.ZipFile(malicious, "w") as after:
            for name in before.namelist():
                after.writestr(name, before.read(name))
            after.writestr("../user-private-key", b"unsafe")
        with self.assertRaisesRegex(ValueError, "Unsafe archive path"):
            verify_snapshot(malicious)

    def test_macos_symlink_must_not_escape_app_bundle(self) -> None:
        original = self.snapshot("macos")
        malicious = self.root / "malicious.tar.gz"
        with tarfile.open(original, "r:gz") as before, tarfile.open(malicious, "w:gz") as after:
            for entry in before:
                if entry.isfile():
                    with before.extractfile(entry) as source:
                        after.addfile(entry, io.BytesIO(source.read()))
                else:
                    after.addfile(entry)
            link = tarfile.TarInfo("Genycaster.app/Contents/Frameworks/Outside")
            link.type = tarfile.SYMTYPE
            link.linkname = "../../../../etc/passwd"
            after.addfile(link)
        with self.assertRaisesRegex(ValueError, "Symlink escapes"):
            verify_snapshot(malicious)

    def test_missing_manifest_file_is_rejected(self) -> None:
        original = self.snapshot()
        missing = self.root / "missing.zip"
        with zipfile.ZipFile(original) as before, zipfile.ZipFile(missing, "w") as after:
            after.writestr("selfupdate.json", before.read("selfupdate.json"))
        with self.assertRaisesRegex(ValueError, "Snapshot contents differ"):
            verify_snapshot(missing)


if __name__ == "__main__":
    unittest.main()
