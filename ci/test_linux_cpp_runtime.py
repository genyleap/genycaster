#!/usr/bin/env python3
"""Contract tests for GCC 16 C++ runtime inclusion in Linux releases."""
from __future__ import annotations

from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from ci.bundle_linux_cpp_runtime import (LAUNCHER, compiler_runtime, install,
                                          required_glibc_versions, symbol_versions)


class CppRuntimePackagingTests(unittest.TestCase):
    def make_stage(self, directory: str) -> tuple[Path, Path]:
        appdir = Path(directory) / "AppDir"
        executable = appdir / "usr/bin/Genycaster"
        executable.parent.mkdir(parents=True)
        executable.write_bytes(b"ELF executable fixture")
        launcher = appdir / "AppRun"
        launcher.symlink_to("usr/bin/Genycaster")
        compiler_lib = Path(directory) / "compiler/libstdc++.so.6"
        compiler_lib.parent.mkdir()
        compiler_lib.write_bytes(b"\x7fELF" + b"\x00" * 150_000)
        return appdir, compiler_lib

    def test_links_cpp_runtime_before_system_and_preserves_app_args(self):
        with tempfile.TemporaryDirectory() as directory:
            appdir, compiler_lib = self.make_stage(directory)
            with (
                patch("ci.bundle_linux_cpp_runtime.compiler_runtime",
                      return_value=compiler_lib),
                patch("ci.bundle_linux_cpp_runtime.elf_machine",
                      return_value="AArch64"),
                patch("ci.bundle_linux_cpp_runtime.symbol_versions",
                      side_effect=lambda path: (
                          {(3, 4, 29), (3, 4, 35)}
                          if path.name == "Genycaster"
                          else {(3, 4, 29), (3, 4, 35), (3, 4, 36)}
                      )),
                patch("ci.bundle_linux_cpp_runtime.required_glibc_versions",
                      return_value={(2, 34), (2, 39)}),
            ):
                result = install(appdir)
            self.assertEqual(result.read_bytes(), compiler_lib.read_bytes())
            launcher = appdir / "AppRun"
            self.assertTrue(launcher.is_file())
            self.assertFalse(launcher.is_symlink())
            self.assertTrue(launcher.stat().st_mode & 0o111)
            self.assertEqual(launcher.read_text(), LAUNCHER)
            self.assertIn("exec \"$appdir/usr/bin/Genycaster\" \"$@\"", LAUNCHER)
            self.assertIn("LD_LIBRARY_PATH", LAUNCHER)
            subprocess.run(["sh", "-n", str(launcher)], check=True)

    def test_wrong_compiler_runtime_architecture_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            appdir, compiler_lib = self.make_stage(directory)
            with (
                patch("ci.bundle_linux_cpp_runtime.compiler_runtime",
                      return_value=compiler_lib),
                patch("ci.bundle_linux_cpp_runtime.elf_machine",
                      side_effect=lambda path: (
                          "AArch64" if path.name == "Genycaster" else "x86-64"
                      )),
            ):
                with self.assertRaisesRegex(ValueError, "different architectures"):
                    install(appdir)
            self.assertTrue((appdir / "AppRun").is_symlink())
            self.assertFalse((appdir / "usr/lib/libstdc++.so.6").exists())

    def test_insufficient_glibcxx_versions_rejected_before_mutation(self):
        with tempfile.TemporaryDirectory() as directory:
            appdir, compiler_lib = self.make_stage(directory)
            with (
                patch("ci.bundle_linux_cpp_runtime.compiler_runtime",
                      return_value=compiler_lib),
                patch("ci.bundle_linux_cpp_runtime.elf_machine",
                      return_value="AArch64"),
                patch("ci.bundle_linux_cpp_runtime.symbol_versions",
                      side_effect=lambda path: (
                          {(3, 4, 35)} if path.name == "Genycaster"
                          else {(3, 4, 29)}
                      )),
            ):
                with self.assertRaisesRegex(ValueError, "lacks executable"):
                    install(appdir)
            self.assertTrue((appdir / "AppRun").is_symlink())
            self.assertFalse((appdir / "usr/lib/libstdc++.so.6").exists())

    def test_newer_glibc_requirement_fails_before_copy(self):
        with tempfile.TemporaryDirectory() as directory:
            appdir, compiler_lib = self.make_stage(directory)
            with (
                patch("ci.bundle_linux_cpp_runtime.compiler_runtime",
                      return_value=compiler_lib),
                patch("ci.bundle_linux_cpp_runtime.elf_machine",
                      return_value="AArch64"),
                patch("ci.bundle_linux_cpp_runtime.symbol_versions",
                      return_value={(3, 4, 35)}),
                patch("ci.bundle_linux_cpp_runtime.required_glibc_versions",
                      return_value={(2, 40)}),
            ):
                with self.assertRaisesRegex(ValueError, "newer than Ubuntu"):
                    install(appdir)
            self.assertTrue((appdir / "AppRun").is_symlink())

    @unittest.skipUnless(shutil.which("g++-16") and shutil.which("readelf"),
                         "GCC 16 or readelf unavailable")
    def test_actual_compiler_provides_required_versions(self):
        library = compiler_runtime("g++-16")
        self.assertIn((3, 4, 35), symbol_versions(library))
        self.assertTrue(all(version <= (2, 39)
                            for version in required_glibc_versions(library)))

    @unittest.skipUnless(shutil.which("g++-16") and shutil.which("readelf"),
                         "GCC 16 or readelf unavailable")
    def test_real_compiled_binary_starts_with_bundled_cpp_library(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            appdir = root / "AppDir"
            executable = appdir / "usr/bin/Genycaster"
            executable.parent.mkdir(parents=True)
            sample = root / "sample.cpp"
            # gps_clock::now() is a genuine GLIBCXX_3.4.35 symbol in
            # GCC 16, matching the arm64 failure reported by GitHub CI.
            sample.write_text(
                '#include <chrono>\n#include <cstdlib>\n#include <iostream>\n'
                'int main() { auto now = std::chrono::gps_clock::now(); '
                'auto* p = std::getenv("LD_LIBRARY_PATH"); '
                'std::cout << (p ? p : "") << "\\n"; '
                'return now.time_since_epoch().count() == 0; }\n')
            subprocess.run(
                ["g++-16", "-std=c++26", str(sample), "-o", str(executable)],
                check=True, capture_output=True, text=True, timeout=30)
            self.assertIn((3, 4, 35), symbol_versions(executable))
            (appdir / "AppRun").symlink_to("usr/bin/Genycaster")
            # On systems with older libstdc++, confirm the exact former
            # GLIBCXX failure before installing the bundled runtime.
            native = "aarch64" if os.uname().machine == "aarch64" else "x86_64"
            old_cpp = Path(f"/usr/lib/{native}-linux-gnu/libstdc++.so.6")
            if old_cpp.is_file() and (3, 4, 35) not in symbol_versions(old_cpp):
                unbundled = subprocess.run(
                    [str(executable)],
                    env={**os.environ,
                         "LD_LIBRARY_PATH": str(old_cpp.parent)},
                    capture_output=True, text=True, timeout=10)
                self.assertNotEqual(unbundled.returncode, 0)
                self.assertIn("GLIBCXX_3.4.35", unbundled.stderr)
            install(appdir)
            environment = os.environ.copy()
            environment["LD_LIBRARY_PATH"] = "/nonexistent-test-lib"
            process = subprocess.run(
                [str(appdir / "AppRun")], env=environment,
                check=True, capture_output=True, text=True, timeout=15)
            self.assertTrue(
                process.stdout.startswith(str(appdir / "usr/lib") + ":"),
                process.stdout)
            resolved = subprocess.run(
                ["ldd", str(executable)],
                env={**environment,
                     "LD_LIBRARY_PATH": str(appdir / "usr/lib")},
                capture_output=True, text=True, check=True)
            self.assertIn(str(appdir / "usr/lib/libstdc++.so.6"),
                          resolved.stdout)
            without_library_path = os.environ.copy()
            without_library_path.pop("LD_LIBRARY_PATH", None)
            clean = subprocess.run(
                [str(appdir / "AppRun")], env=without_library_path,
                check=True, capture_output=True, text=True, timeout=15)
            self.assertEqual(clean.stdout.strip(), str(appdir / "usr/lib"))

    def test_missing_compiler_lib_cannot_succeed(self):
        with tempfile.TemporaryDirectory() as directory:
            appdir, _ = self.make_stage(directory)
            with patch("ci.bundle_linux_cpp_runtime.compiler_runtime",
                       side_effect=ValueError("compiler runtime not found")):
                with self.assertRaisesRegex(ValueError, "not found"):
                    install(appdir)
            self.assertTrue((appdir / "AppRun").is_symlink())


if __name__ == "__main__":
    unittest.main()
