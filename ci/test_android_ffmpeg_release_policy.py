#!/usr/bin/env python3
"""Regression policy: Android public beta cannot ship Qt FFmpeg 16-KiB ELF defects."""
from __future__ import annotations

from pathlib import Path
import struct
import subprocess
import tempfile
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

    def test_sdk_plugin_discovery_without_imported_target(self):
        # A host-only CMake fixture exercises the real Android deployment
        # helper without pretending to cross-compile a Qt APK.
        helper = ROOT / "cmake/platform/Android/configure.cmake"
        with tempfile.TemporaryDirectory() as tmp:
            fixture = Path(tmp)
            sdk = fixture / "sdk"
            plugins = sdk / "plugins/multimedia"
            plugins.mkdir(parents=True)
            config = sdk / "lib/cmake/Qt6Multimedia"
            config.mkdir(parents=True)
            name = "libplugins_multimedia_ffmpegmediaplugin_arm64-v8a.so"
            plugin = plugins / name
            plugin.write_bytes(b"fixture plugin")
            (fixture / "CMakeLists.txt").write_text(f"""
cmake_minimum_required(VERSION 3.22)
project(AndroidPluginFixture NONE)
include("{helper.as_posix()}")
add_custom_target(app)
set(Qt6Multimedia_DIR "{config.as_posix()}")
set(ANDROID_ABI arm64-v8a)
genycaster_android_configure_media_backend(app)
get_target_property(extra app QT_ANDROID_EXTRA_PLUGINS)
if(EXPECT_FILE)
    if(EXISTS "${{Qt6Multimedia_DIR}}/../../../lib/libavformat.so")
        get_target_property(deps app QT_ANDROID_EXTRA_LIBS)
        if(NOT deps MATCHES "libavformat.so")
            message(FATAL_ERROR "Missing SDK FFmpeg dependency staging")
        endif()
    endif()
    if(NOT EXISTS "${{extra}}/multimedia/{name}")
        message(FATAL_ERROR "No staged SDK plugin")
    endif()
elseif(extra)
    message(FATAL_ERROR "Unexpected plugin staging")
endif()
""")
            def configure(case, strict, expect_file):
                return subprocess.run([
                    "cmake", "-S", str(fixture), "-B", str(fixture / case),
                    f"-DGENYCASTER_REQUIRE_ANDROID_FFMPEG={strict}",
                    f"-DEXPECT_FILE={expect_file}",
                ], capture_output=True, text=True, check=False)
            result = configure("binary-only", "ON", "ON")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            plugin.unlink()
            # Qt 6.12 can expose the unsuffixed Android plugin SDK name.
            unsuffixed = plugins / "libplugins_multimedia_ffmpegmediaplugin.so"
            unsuffixed.write_bytes(b"fixture unsuffixed plugin")
            (sdk / "lib/libavformat.so").write_bytes(b"fixture dependency")
            result = configure("unsuffixed", "ON", "ON")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("libplugins_multimedia_ffmpegmediaplugin.so", result.stdout)
            unsuffixed.unlink()
            # An incompatible ABI must never satisfy arm64 discovery.
            (plugins / name.replace("arm64-v8a", "armeabi-v7a")).write_bytes(b"other ABI")
            result = configure("local-missing", "OFF", "OFF")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("unsafe native fallback is disabled", " ".join(result.stderr.split()))
            result = configure("release-missing", "ON", "OFF")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Android FFmpeg plugin missing", result.stderr)

    def test_official_android_ffmpeg_source_is_pinned_and_rebuilt(self):
        script = (ROOT / "ci/build_android_ffmpeg_16k.sh").read_text()
        workflow = (ROOT / ".github/workflows/ci.yml").read_text()
        android = workflow.split("\n  android:\n", 1)[1].split("\n  ios:\n", 1)[0]
        validator = (ROOT / "ci/verify_platform_package.py").read_text()
        self.assertIn("8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e", script)
        self.assertIn("https://ffmpeg.org/releases/ffmpeg-", script)
        self.assertIn("sha256sum -c --status", script)
        self.assertIn("-Wl,-z,max-page-size=16384", script)
        self.assertIn("-Wl,-z,common-page-size=16384", script)
        self.assertIn("Qt FFmpeg SONAME incompatibility", script)
        self.assertIn("version=9.0.2", script)
        self.assertIn("qt_version=6.12.1", script)
        self.assertIn('if [[ "$needs_module" == 1 ]]', script)
        self.assertIn("-DQT_DEPLOY_FFMPEG=ON -DFEATURE_ffmpeg=ON", script)
        self.assertIn("5a125bc2bede8c6512296e0f3204a9922741fd144fcae7d4a66b3c007bb125e3", script)
        self.assertIn('("avcodec", "avformat", "avutil", "swscale", "swresample")', validator)
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
