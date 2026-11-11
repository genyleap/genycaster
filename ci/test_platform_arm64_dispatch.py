#!/usr/bin/env python3
"""Guard ARM64-only workflow requests and signed GCC bootstrap."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parent.parent

class Arm64DispatchTest(unittest.TestCase):
    def test_linux_compiler_install_does_not_use_launchpad_api(self):
        workflow=(ROOT/".github/workflows/ci.yml").read_text()
        linux=workflow.split("  linux:\n",1)[1].split("  macos:\n",1)[0]
        self.assertIn("bash ci/install_linux_gcc16.sh",linux)
        self.assertNotIn("add-apt-repository -y ppa:",linux)
        script=(ROOT/"ci/install_linux_gcc16.sh").read_text()
        self.assertIn("C8EC952E2A0E1FBDC5090F6A2C277A0A352154E5",script)
        self.assertIn("signed-by=/usr/share/keyrings/genycaster-ubuntu-toolchain-r-test.gpg",script)
        self.assertIn("apt-get install -y --no-install-recommends gcc-16 g++-16 lld",script)

    def test_manual_android_arm64_requests_only_arm64_v8a(self):
        workflow=(ROOT/".github/workflows/ci.yml").read_text()
        android=workflow.split("  android:\n",1)[1].split("  ios:\n",1)[0]
        self.assertIn("inputs.platform == 'android' && inputs.target_arch == 'arm64'",android)
        self.assertIn("'[{\"abi\":\"arm64-v8a\",\"qt_arch\":\"android_arm64_v8a\",\"openssl_arch\":\"arm64\",\"machine\":\"AArch64\"}]'",android)
        self.assertIn("\"abi\":\"armeabi-v7a\"",android)
        self.assertIn("-DANDROID_ABI=${{ matrix.abi }}",android)
        self.assertIn("apksigner",android)
        self.assertIn("zipalign",android)
        self.assertIn("CI-TEST.apk",android)

if __name__=="__main__":
    unittest.main()
