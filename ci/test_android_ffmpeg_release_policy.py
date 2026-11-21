#!/usr/bin/env python3
"""Regression policy: Android public beta cannot ship Qt FFmpeg 16-KiB ELF defects."""
from __future__ import annotations

from pathlib import Path
import struct
import unittest

from ci.verify_platform_package import validate_android_elf_alignment

ROOT = Path(__file__).resolve().parent.parent


class AndroidFFmpegReleasePolicyTests(unittest.TestCase):
    def test_actual_release_validator_rejects_incongruent_pt_load(self):
        blob = bytearray(160)
        blob[:6] = b"\x7fELF\x02\x01"
        struct.pack_into("<H", blob, 18, 183)
        struct.pack_into("<Q", blob, 32, 64)
        struct.pack_into("<H", blob, 54, 56)
        struct.pack_into("<H", blob, 56, 1)
        struct.pack_into("<I", blob, 64, 1)
        struct.pack_into("<Q", blob, 64 + 48, 16384)
        struct.pack_into("<Q", blob, 64 + 8, 0x2ACB90)
        struct.pack_into("<Q", blob, 64 + 16, 0x2BC000)
        with self.assertRaisesRegex(ValueError, "incongruent ELF LOAD"):
            validate_android_elf_alignment(
                "lib/arm64-v8a/libavformat.so", bytes(blob))
        # The same mapping is safe when rebuilt with 16 KiB congruence.
        struct.pack_into("<Q", blob, 64 + 16, 0x2ACB90 + 0x4000)
        validate_android_elf_alignment("lib/arm64-v8a/libavformat.so", bytes(blob))

    def test_official_android_ffmpeg_source_is_pinned_and_rebuilt(self):
        script = (ROOT / "ci/build_android_ffmpeg_16k.sh").read_text()
        workflow = (ROOT / ".github/workflows/ci.yml").read_text()
        android = workflow.split("\n  android:\n", 1)[1].split("\n  ios:\n", 1)[0]
        validator = (ROOT / "ci/verify_platform_package.py").read_text()
        self.assertIn("cf38e0e28c7e5605942c4a77755349b0145804a397af37eb1fb4c77cb237f635", script)
        self.assertIn("https://ffmpeg.org/releases/ffmpeg-", script)
        self.assertIn("sha256sum -c --status", script)
        self.assertIn("-Wl,-z,max-page-size=16384", script)
        self.assertIn("-Wl,-z,common-page-size=16384", script)
        self.assertIn("Qt FFmpeg SONAME incompatibility", script)
        self.assertNotIn("patchelf", script.lower())
        self.assertIn("Rebuild Qt FFmpeg Android libraries for 16 KiB pages", android)
        self.assertLess(android.index("Prepare Android OpenSSL"),
                        android.index("Rebuild Qt FFmpeg"))
        self.assertLess(android.index("Rebuild Qt FFmpeg"),
                        android.index("Configure and build Android APK"))
        self.assertNotIn("vendor_ffmpeg_ci_exception", validator)
        self.assertIn("validate_android_elf_alignment(library, apk.read(library))", validator)
        self.assertIn('python3 ci/verify_platform_package.py android "$artifact"', android)
        self.assertIn('zipalign" -c -P 16 -v 4', android)


if __name__ == "__main__":
    unittest.main()
