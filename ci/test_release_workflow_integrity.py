#!/usr/bin/env python3
"""Guard against accidentally publishing or disabling a partial platform build.

This protects CI YAML source from duplicated stages or truncated commands,
notably the Linux and macOS release workflows. Real builds and package audits
remain mandatory and are not replaced by these structural assertions.
"""
from __future__ import annotations

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parent.parent
WORKFLOW = ROOT / ".github/workflows/ci.yml"
LOCAL = ROOT / "ci/build_local_linux_release.sh"

def job_source(workflow: str, name: str, following: str) -> str:
    begin = "\n  " + name + ":\n"
    end = "\n  " + following + ":\n"
    if workflow.count(begin) != 1 or workflow.count(end) != 1:
        raise AssertionError(f"Expected exactly one {name}/{following} job")
    return workflow.split(begin, 1)[1].split(end, 1)[0]

class ReleaseWorkflowIntegrityTest(unittest.TestCase):
    def test_single_complete_linux_stage(self) -> None:
        s = WORKFLOW.read_text(encoding="utf-8")
        linux = job_source(s, "linux", "macos")
        for marker in (
            "      - name: Build Qt client",
            "      - name: Package AppImage",
            "          python3 ci/stage_linux_webengine.py AppDir",
            "          python3 ci/bundle_linux_webengine_runtime.py AppDir",
            "          cmake --build build/native --target genycaster",
            "          grep -q '^FARCASTER_ENABLE_MINIAPPS:BOOL=ON$'",
            "          grep -q '^GENYCASTER_REQUIRE_LINUX_MINIAPP_HOST:BOOL=ON$'",
            "          python3 ci/audit_linux_appdir.py AppDir --require-miniapps",
        ):
            self.assertIn(marker, linux)
        self.assertEqual(linux.count("      - name: Package AppImage"), 1)
        self.assertEqual(linux.count("          cmake --build build/native --target genycaster"), 1)
        self.assertEqual(linux.count("          python3 ci/stage_linux_webengine.py AppDir"), 1)
        self.assertEqual(linux.count("          python3 ci/bundle_linux_webengine_runtime.py AppDir"), 1)

    def test_macos_has_one_valid_full_webkit_build_and_dmg_audit(self) -> None:
        s = WORKFLOW.read_text(encoding="utf-8")
        mac = job_source(s, "macos", "windows")
        self.assertEqual(mac.count("      - name: Build and deploy macOS application"), 1)
        self.assertEqual(mac.count("          cmake --build build/native --target genycaster"), 1)
        self.assertEqual(mac.count("          cmake --build build/native --target farcaster_miniapp_browser"), 1)
        self.assertIn("          grep -q '^FARCASTER_ENABLE_MINIAPPS:BOOL=ON$'", mac)
        self.assertIn('          APP=build/native/bin/Genycaster.app', mac)
        self.assertIn('ci/audit_macos_bundle.py "$APP"', mac)
        self.assertIn('ci/audit_macos_bundle.py "$MOUNT/Genycaster.app"', mac)
        self.assertEqual(mac.count("name: GenyCaster-macos-${{ matrix.arch }}"), 1)

    def test_windows_webengine_install_uses_official_separate_archives(self) -> None:
        from ci.install_qt_windows import (
            archives_from_index, webengine_archive_layout,
        )

        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("Install separately versioned Qt WebEngine extension", workflow)
        self.assertIn("ci/install_qt_webengine_windows.py --destination Qt --architecture "
                      "${{ matrix.arch }}", workflow)
        self.assertNotIn("python -m aqt install-qt windows desktop 6.12.0", workflow)
        for arch, platform, folder, suffix, stamp in (
            ("x86_64", "windows_x86", "msvc2022_64", "win64_msvc2022_64",
             "6.12.0-0-202609281005"),
            ("arm64", "windows_arm64", "msvc2022_arm64",
             "win64_msvc2022_arm64", "6.12.0-0-202611110526"),
        ):
            with self.subTest(arch=arch):
                base, groups, selected_stamp = webengine_archive_layout(arch)
                self.assertEqual(selected_stamp, stamp)
                self.assertEqual(
                    base,
                    "https://download.qt.io/online/qtsdkrepository/"
                    f"{platform}/extensions/qtwebengine/6120/61400/{folder}/",
                )
                self.assertEqual(
                    list(groups),
                    [f"extensions.qtwebengine.6120.61400.{suffix}"],
                )
                html = (
                    f'<a href="{stamp}qtwebengine-Windows-Release.7z"></a>'
                    f'<a href="{stamp}qtwebview_qtwebengine_plugin.7z"></a>'
                    '<a href="untrustedqtwebengine-Windows-Release.7z"></a>'
                )
                self.assertEqual(
                    archives_from_index(html, groups[next(iter(groups))],
                                        stamp=stamp),
                    [f"{stamp}qtwebengine-Windows-Release.7z",
                     f"{stamp}qtwebview_qtwebengine_plugin.7z"],
                )
                with self.assertRaises(ValueError):
                    archives_from_index(html, ("qtwebengine",),
                                        stamp="6.12.0-0-unknown")

    def test_one_unified_dispatch_and_tag_release_dependency_graph(self) -> None:
        # GenyConnect-inspired orchestration: one invocation fan-outs to all
        # requested native architectures and the site-only WebAssembly job.
        # Individual diagnostic runs must never publish a Release.
        s = WORKFLOW.read_text(encoding="utf-8")
        dispatch = s.split("  workflow_dispatch:\\n", 1)[1].split("  push:\\n", 1)[0]
        self.assertIn("        default: all\\n        options:\\n          - all", dispatch)
        self.assertIn("confirm_full_matrix:", dispatch)
        gate = job_source(s, "gate", "linux")
        self.assertIn("GENYCASTER_FULL_MATRIX_CONFIRMED", gate)
        for job, following in (
            ("linux", "macos"), ("macos", "windows"),
            ("windows", "android"), ("android", "ios"),
            ("web", "publish"),
        ):
            with self.subTest(job=job):
                source = job_source(s, job, following)
                self.assertIn("    needs: gate", source)
                self.assertIn("inputs.platform == 'all'", source)
                self.assertIn("github.event_name == 'push'", source)
                self.assertIn("actions/upload-artifact@v4", source)
        for optional, following in (("ios", "nixos"), ("nixos", "web")):
            source = job_source(s, optional, following)
            self.assertIn("inputs.platform == '" + optional + "'", source)
            self.assertNotIn("inputs.platform == 'all'", source)
        publish = s.split("\\n  publish:\\n", 1)[1]
        self.assertEqual(s.count("\\n  publish:\\n"), 1)
        self.assertIn("needs: [gate, linux, macos, windows, android, web]", publish)
        self.assertIn("github.event_name == 'push'", publish)
        self.assertIn("actions/download-artifact@v4", publish)
        self.assertIn("python3 ci/release_artifacts.py artifacts release", publish)
        self.assertIn("softprops/action-gh-release@v2", publish)

    def test_local_linux_pipeline_not_repeated(self) -> None:
        s = LOCAL.read_text(encoding="utf-8")
        self.assertEqual(s.count("python3 ci/stage_linux_webengine.py"), 1)
        self.assertEqual(s.count("python3 ci/bundle_linux_webengine_runtime.py"), 1)
        self.assertEqual(s.count('cmake --build "$build_dir" --target genycaster'), 1)
        self.assertEqual(s.count("printf '\\nLocal %s release-equivalent packages:"), 1)
        self.assertIn("grep -q '^FARCASTER_ENABLE_MINIAPPS:BOOL=ON$'", s)

    def test_only_tag_pushes_publish_and_feature_gate_blocks_partial_release(self) -> None:
        s = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("    tags:\n      - 'v*'", s)
        self.assertIn("python3 ci/verify_release_feature_contract.py --strict", s)
        # Manual development builds remain available, but publishing a tag
        # cannot bypass architecture, translation, privacy or design gates.
        gate = job_source(s, "gate", "linux")
        self.assertIn("run: python3 ci/server_acceptance.py --strict-design", gate)
        self.assertIn("Refuse an unsigned public release before installing toolchains", gate)
        self.assertIn("MACOS_DEVELOPER_ID_P12_BASE64", gate)
        self.assertIn("ANDROID_KEYSTORE_BASE64", gate)
        # Browser WASM stays a separately verified site artifact, not a
        # public downloadable installer in GitHub Release assets.
        from ci.release_artifacts import EXPECTED
        self.assertNotIn("GenyCaster-web-wasm", EXPECTED)
        self.assertIn("if: steps.release.outputs.release == 'true'", gate)
        self.assertIn("          python3 ci/audit_linux_appdir.py AppDir --require-miniapps", s)
        # Truncated grep commands were accepted as YAML block text, then
        # silently skipped when the remaining steps were duplicated.
        self.assertNotRegex(s, re.compile(r"grep -q '\^FARCASTER_ENABLE_MINIAPPS:BOOL=ON\n"))

if __name__ == "__main__":
    unittest.main()
