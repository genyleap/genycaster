#!/usr/bin/env python3
"""Exercise the actual Windows CPack configuration through two CMake parses."""
from __future__ import annotations
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from pathlib import Path

SOURCE = Path(__file__).resolve().parent.parent

@unittest.skipUnless(shutil.which("cmake"), "CMake unavailable")
class NsisConfigurationTests(unittest.TestCase):
    def test_windows_cpack_config_parses_and_preserves_nsis_commands(self):
        for arch, suffix in (("arm64", "arm64"), ("x86_64", "x64")):
            with self.subTest(arch=arch):
                self._check_config_for_architecture(arch, suffix)

    def _check_config_for_architecture(self, arch, suffix):
        with tempfile.TemporaryDirectory(prefix="genycaster-nsis-") as tmp:
            root = Path(tmp)
            src = root / "src"
            src.mkdir()
            for name in ("properties", "ui", "LICENSE", "THIRD_PARTY_NOTICES.md"):
                (src / name).symlink_to(SOURCE / name,
                    target_is_directory=name in ("properties", "ui"))
            cmake_source = (
                "cmake_minimum_required(VERSION 3.25)\n"
                "project(GenyCaster VERSION 1.0.0 LANGUAGES CXX)\n"
                'file(WRITE "@{CMAKE_BINARY_DIR}/stub.cpp" "int main(){return 0;}")\n'
                'add_executable(genycaster "@{CMAKE_BINARY_DIR}/stub.cpp")\n'
                'file(WRITE "@{CMAKE_BINARY_DIR}/thirdparty.hpp" "SDK-only")\n'
                'install(FILES "@{CMAKE_BINARY_DIR}/thirdparty.hpp" DESTINATION include)\n'
                'set(GENYCASTER_RELEASE_VERSION "1.0.0-rc1")\n'
                f'set(GENYCASTER_WINDOWS_PACKAGE_ARCH "{arch}")\n'
                'include("@{CMAKE_SOURCE_DIR}/properties/Windows/Configure.cmake")\n'
            ).replace("@{", chr(36) + "{")
            # Simulate one runtime payload and a third-party CMake install
            # rule (e.g., QtKeychain headers). Only Runtime may be shipped.
            stage = src / "dist/app"
            stage.mkdir(parents=True)
            (stage / "Genycaster.exe").write_bytes(b"fake-runtime")
            (src / "CMakeLists.txt").write_text(cmake_source)
            build = root / "build"
            config = subprocess.run(
                ["cmake", "-S", str(src), "-B", str(build)],
                capture_output=True, text=True, timeout=90)
            self.assertEqual(config.returncode, 0, config.stdout + config.stderr)
            cpack = build / "CPackConfig.cmake"
            self.assertTrue(cpack.is_file())
            parsed = subprocess.run(["cmake", "-P", str(cpack)],
                capture_output=True, text=True, timeout=30)
            self.assertEqual(parsed.returncode, 0, parsed.stdout + parsed.stderr)
            options = build / "GenyCasterNSISOptions.cmake"
            data = options.read_text()
            self.assertIn("CreateShortCut", data)
            self.assertIn("vc_redist." + suffix + ".exe", data)
            self.assertNotIn("SetRebootFlag true", data)
            self.assertIn("StrCmp $0 3010 genycaster_vc_runtime_done", data)
            license_text = (build / "GenyCaster-Installer-License.txt").read_text()
            self.assertTrue(license_text.startswith("GNU GENERAL PUBLIC LICENSE"),
                            license_text[:150])
            self.assertNotIn("Software License and Publisher Notice", license_text)
            self.assertIn("CPACK_NSIS_WELCOME_TITLE", cpack.read_text())
            self.assertNotIn("@GENYCASTER_VC_REDIST_SUFFIX@", data)
            probe = root / "probe.cmake"
            probe.write_text(
                'set(CPACK_GENERATOR NSIS)\n'
                'include("' + options.as_posix() + '")\n'
                'if(NOT CPACK_NSIS_CREATE_ICONS_EXTRA MATCHES "CreateShortCut")\n'
                '  message(FATAL_ERROR "Missing desktop shortcut")\n'
                'endif()\n'
                f'if(NOT CPACK_NSIS_EXTRA_INSTALL_COMMANDS MATCHES "vc_redist.{suffix}.exe")\n'
                '  message(FATAL_ERROR "Missing VC runtime installation")\n'
                'endif()\n')
            check = subprocess.run(["cmake", "-P", str(probe)],
                capture_output=True, text=True, timeout=30)
            self.assertEqual(check.returncode, 0, check.stdout + check.stderr)
            # CPack must not leak the install rules from other components
            # into the real NSIS install payload. Use the portable TGZ
            # generator to inspect CPack's component selection without
            # requiring the Windows-only NSIS compiler on this test runner.
            output = root / "release"
            package = subprocess.run(
                ["cpack", "--config", str(cpack), "-G", "TGZ",
                 "-B", str(output)],
                capture_output=True, text=True, timeout=90,
            )
            self.assertEqual(package.returncode, 0,
                             package.stdout + package.stderr)
            packages = list(output.glob("*.tar.gz"))
            self.assertEqual(len(packages), 1)
            with tarfile.open(packages[0], "r:gz") as archive:
                members = archive.getnames()
            self.assertTrue(any(name.endswith("/Genycaster.exe")
                                for name in members), members)
            self.assertFalse(any("/include/" in name
                                 or name.endswith("/include")
                                 for name in members), members)
            self.assertFalse(any(name.endswith("thirdparty.hpp")
                                 for name in members), members)

if __name__ == "__main__":
    unittest.main()
