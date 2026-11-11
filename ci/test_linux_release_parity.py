#!/usr/bin/env python3
"""Keep GitHub Linux release and native Ubuntu 24.04 ARM64 reproduction aligned."""
from __future__ import annotations

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parent.parent
WORKFLOW = ROOT / ".github/workflows/ci.yml"
DOCKERFILE = ROOT / "ci/Dockerfile.linux-arm64-release"
LOCAL_SCRIPT = ROOT / "ci/build_local_linux_release.sh"

# Tools available on a hosted Ubuntu runner but installed explicitly inside
# the minimal local Ubuntu image. The remaining package set must match exactly.
CONTAINER_ONLY = {
    "ca-certificates", "gnupg", "git", "python3-pip",
    "docker.io", "nodejs",
}


def install_packages(source: str, marker: str) -> set[str]:
    """Read one apt install command, including shell continuation lines."""
    lines = source.splitlines()
    for index, line in enumerate(lines):
        if marker not in line:
            continue
        result = [line.split(marker, 1)[1].strip()]
        while line.rstrip().endswith("\\"):
            index += 1
            if index >= len(lines):
                raise ValueError(f"Unterminated apt command: {marker}")
            line = lines[index]
            result.append(line.strip())
        # Dockerfile chains apt install with later apt commands using &&.
        # Only the first install command's packages belong in the comparison.
        return set(" ".join(result).replace("\\", " ").split("&&", 1)[0].split())
    raise ValueError(f"apt command not found: {marker}")


class LinuxReleaseParityTests(unittest.TestCase):
    def test_local_and_hosted_ubuntu_build_dependencies_match(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        dockerfile = DOCKERFILE.read_text(encoding="utf-8")
        linux = workflow.split("  linux:\n", 1)[1].split("  macos:\n", 1)[0]
        hosted = install_packages(linux, "sudo apt-get install -y")
        local = install_packages(
            dockerfile, "apt-get install -y --no-install-recommends"
        )
        self.assertEqual(hosted, local - CONTAINER_ONLY)
        self.assertTrue(CONTAINER_ONLY.issubset(local))

    def test_release_gate_checks_plugins_before_compilation(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        linux = workflow.split("  linux:\n", 1)[1].split("  macos:\n", 1)[0]
        local = LOCAL_SCRIPT.read_text(encoding="utf-8")
        preflight = "python3 ci/preflight_linux_qt_runtime.py --qt-root"
        self.assertLess(linux.index(preflight), linux.index("Build Qt client"))
        self.assertLess(local.index(preflight), local.index("cmake --build"))
        for name in ("ci/preflight_linux_miniapp_host.py",
                     "ci/stage_linux_webengine.py",
                     "ci/bundle_linux_cpp_runtime.py",
                     "ci/bundle_linux_tls_runtime.py",
                     "ci/bundle_linux_webengine_runtime.py",
                     "ci/audit_linux_appdir.py",
                     "ci/smoke_linux_install.sh"):
            self.assertIn(name, linux)
            self.assertIn(name, local)


    def test_release_requires_native_mini_apps_and_packaged_chromium(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        linux = workflow.split("  linux:\n", 1)[1].split("  macos:\n", 1)[0]
        local = LOCAL_SCRIPT.read_text(encoding="utf-8")
        for required in (
            "-DFARCASTER_ENABLE_MINIAPPS=ON",
            "-DGENYCASTER_REQUIRE_LINUX_MINIAPP_HOST=ON",
            "ci/preflight_linux_miniapp_host.py --qt-root",
            "ci/stage_linux_webengine.py",
            "ci/bundle_linux_webengine_runtime.py",
            "--require-miniapps",
        ):
            self.assertIn(required, linux)
            self.assertIn(required, local)
        self.assertLess(linux.index("ci/preflight_linux_miniapp_host.py --qt-root"),
                        linux.index("cmake --build build/native"))
        self.assertIn("qtwebchannel qtpositioning qtserialport qtwebengine", linux)

    def test_release_baseline_and_cmake_version_are_pinned(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        container = DOCKERFILE.read_text(encoding="utf-8")
        self.assertIn("QT_VERSION: '6.12.0'", workflow)
        self.assertIn("CMAKE_VERSION: '4.4.4'", workflow)
        self.assertIn("FROM ubuntu:24.04", container)
        self.assertIn("'cmake==4.4.4'", container)
        self.assertIn("ubuntu-24.04-arm", workflow)
        self.assertIn("linux_gcc_arm64", workflow)


if __name__ == "__main__":
    unittest.main()
