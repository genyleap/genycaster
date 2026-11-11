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
        self.assertIn("keys/ubuntu-toolchain-r-test.asc",script)
        self.assertIn("https://ppa.launchpadcontent.net/ubuntu-toolchain-r/test/ubuntu",script)
        self.assertIn("https://ppa.launchpad.net/ubuntu-toolchain-r/test/ubuntu",script)
        self.assertIn("if [[ -z \"$selected\" ]]",script)
        self.assertIn("--max-time 10",script)
        # DNS outages on Canonical endpoints should not require disabling the
        # signed PPA or silently falling back to an older compiler. DNS-over-
        # HTTPS maps public IPv4 addresses only; TLS and APT still authenticate.
        self.assertIn("https://dns.google/resolve?name=",script)
        self.assertIn("https://cloudflare-dns.com/dns-query?name=",script)
        self.assertIn("addr.is_global",script)
        self.assertIn("sudo tee -a /etc/hosts",script)
        self.assertIn('if ! resolve_ppa_hostname "$ppa_host" || ! probe_ppa "$repo_url"; then',script)
        self.assertNotIn("curl -k",script)
        self.assertIn('install_linux_gcc16_conda.sh',script)
        fallback=(ROOT/"ci/install_linux_gcc16_conda.sh").read_text()
        self.assertIn('micromamba_sha=9f93b974adcb4d166996af969b6cd371287d1a3e52733704727884d9b74cb7a7',fallback)
        self.assertIn('sha256sum --check --status',fallback)
        self.assertIn("'gcc_linux-aarch64=16.2.0'",fallback)
        self.assertIn("'gxx_linux-aarch64=16.2.0'",fallback)
        self.assertIn('test -x "$compiler"',fallback)
        self.assertIn('test -x "$cxx"',fallback)
        self.assertIn('g++-16 -dumpfullversion',fallback)
        self.assertIn("'binutils_linux-aarch64'",fallback)
        self.assertIn('COMPILER_PATH=$COMPILER_PATH',fallback)
        self.assertIn('g++-16 -std=c++26 -fuse-ld=lld',fallback)
        self.assertIn('"$work/link-smoke"',fallback)
        self.assertNotIn('no-tests',fallback)
        self.assertNotIn("keyserver.ubuntu.com",script)
        self.assertTrue((ROOT/"ci/keys/ubuntu-toolchain-r-test.asc").is_file())
        self.assertIn("signed-by=/usr/share/keyrings/genycaster-ubuntu-toolchain-r-test.gpg",script)
        self.assertIn("apt-get install -y --no-install-recommends",script)
        self.assertIn("gcc-16 g++-16 lld",script)
        self.assertIn("timeout 180s",script)
        self.assertIn("Dir::Etc::sourcelist=/etc/apt/sources.list.d/genycaster-toolchain-r.list",script)
        self.assertIn("bash ci/prepare_ubuntu_apt.sh",linux)
        self.assertIn("sudo timeout 240s apt-get install",linux)
        mirror=(ROOT/"ci/prepare_ubuntu_apt.sh").read_text()
        self.assertIn("https://us.ports.ubuntu.com/ubuntu-ports",mirror)
        self.assertIn("Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg",mirror)
        self.assertIn("APT::Update::Error-Mode=any",mirror)
        self.assertIn("timeout 100s apt-get update",mirror)

    def test_android_does_not_require_apt_to_use_hosted_tools(self):
        workflow=(ROOT/".github/workflows/ci.yml").read_text()
        android=workflow.split("  android:\n",1)[1].split("  ios:\n",1)[0]
        setup=android.split("      - name: Verify Android build tools",1)[1].split("      - name: Install Android Qt host dependencies",1)[0]
        self.assertNotIn("sudo apt-get",setup)
        self.assertNotIn("sudo add-apt-repository",setup)
        self.assertIn("SDK/NDK and build tools already installed",setup)
        self.assertIn("timeout 360s",setup)
        self.assertIn('test -f "$ndk/source.properties"',setup)
        self.assertIn("timeout-minutes: 20",android)
        qt_host_deps=android.split("      - name: Install Android Qt host dependencies",1)[1].split("      - uses: jurplel/install-qt-action",1)[0]
        self.assertIn("bash ci/prepare_ubuntu_apt.sh",qt_host_deps)
        self.assertIn("sudo timeout 180s apt-get install",qt_host_deps)
        self.assertIn("libxkbcommon-x11-0",qt_host_deps)
        qt_action=android.split("      - uses: jurplel/install-qt-action",1)[1].split("      - name: Reject Android SDKs",1)[0]
        self.assertIn("install-deps: false",qt_action)
        self.assertNotIn("qt.mirror.constant.com",qt_action)


    def test_android_manifest_does_not_override_gradle_native_library_packaging(self):
        # AGP 8+ rejects android:extractNativeLibs in a source manifest.
        # Let the Gradle plugin select packaging; CI checks page alignment
        # on the actual signed artifact with zipalign -P 16.
        manifest=(ROOT/"properties/Android/AndroidManifest.xml").read_text()
        self.assertNotIn("android:extractNativeLibs=",manifest)
        workflow=(ROOT/".github/workflows/ci.yml").read_text()
        android=workflow.split("  android:\n",1)[1].split("  ios:\n",1)[0]
        self.assertIn('zipalign" -c -P 16 -v 4',android)
        self.assertIn('"$signer" verify --verbose --print-certs',android)

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
