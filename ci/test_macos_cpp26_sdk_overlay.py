"""Regression: fix only mixed Apple SDK enum math, retaining C++26 ObjC++."""
import importlib.util
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
SCRIPT = ROOT / "ci" / "prepare_macos_sdk_cpp26.py"
spec = importlib.util.spec_from_file_location("macos_cpp26_sdk", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

CG_HEADER = """#pragma once
enum CGImageAlphaInfo {alpha = 1};
enum CGImageComponentInfo {component = 2};
enum CGImageByteOrder {byteOrder = 4};
enum CGImagePixelFormat {pixelFormat = 8};
using CGBitmapInfo = unsigned int;
inline CGBitmapInfo info() {
    return (CGBitmapInfo)(alpha | component | byteOrder | pixelFormat);
}
"""
CS_HEADER = """#pragma once
enum {cmGraySpace = 3, cmRGBSpace = 4};
enum {cm8_8ColorPacking = 2, cmLittleEndianPacking = 8};
enum {
    cmGray8Space = cmGraySpace + cm8_8ColorPacking,
    cmGray16LSpace = cmGraySpace + cmLittleEndianPacking,
    cmRGB16LSpace = cmRGBSpace + cm8_8ColorPacking + cmLittleEndianPacking,
};
"""

class AppleSdkOverlayTests(unittest.TestCase):
    def test_cpp26_enum_rewrite_and_vfs_overlay(self):
        with tempfile.TemporaryDirectory(prefix="genycaster-sdk-cpp26-") as tmp:
            root = pathlib.Path(tmp)
            sdk = root / "sdk"
            for relative, body in ((module.COREGRAPHICS, CG_HEADER),
                                   (module.COLORSYNC, CS_HEADER)):
                original = sdk / relative
                original.parent.mkdir(parents=True, exist_ok=True)
                original.write_text(body, encoding="utf-8")
            output = root / "generated"
            changes = module.prepare(sdk, output)
            self.assertEqual(changes["CGImage.h"], 1)
            self.assertEqual(changes["ColorSyncDeprecated.h"], 3)
            self.assertEqual((sdk / module.COREGRAPHICS).read_text(), CG_HEADER)
            self.assertEqual((sdk / module.COLORSYNC).read_text(), CS_HEADER)
            generated = (output / "macos_cpp26_sdk_overlay.json")
            self.assertTrue(generated.is_file())
            import json
            overlay = json.loads(generated.read_text())
            self.assertEqual(len(overlay["roots"]), 2)
            self.assertEqual(overlay["version"], 0)
            clang = shutil.which("clang++")
            if not clang:
                return
            program = root / "demo.cpp"
            program.write_text(
                '#include "CGImage.h"\n#include "ColorSyncDeprecated.h"\n'
                "static_assert(cmRGB16LSpace == 14);\n"
                "static_assert(cmGray8Space == 5);\n"
                "int main() { return info() == 15 ? 0 : 1; }\n",
                encoding="utf-8")
            includes = [
                "-I", str((sdk / module.COREGRAPHICS).parent),
                "-I", str((sdk / module.COLORSYNC).parent),
            ]
            command = [clang, "-std=c++2c", "-Wall", "-Wextra",
                       "-Wpedantic", "-fsyntax-only", *includes]
            before = subprocess.run([*command, str(program)],
                                    capture_output=True, text=True)
            self.assertNotEqual(before.returncode, 0, "Expected genuine C++26 SDK enum error")
            after = subprocess.run(
                [*command, "-ivfsoverlay", str(generated), str(program)],
                capture_output=True, text=True)
            self.assertEqual(after.returncode, 0, after.stderr)

    def test_versioned_sdk_and_macosx_alias_are_both_redirected(self):
        with tempfile.TemporaryDirectory(prefix="genycaster-sdk-link-") as tmp:
            root = pathlib.Path(tmp)
            sdk = root / "MacOSX26.5.sdk"
            for relative, body in ((module.COREGRAPHICS, CG_HEADER),
                                   (module.COLORSYNC, CS_HEADER)):
                file = sdk / relative
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_text(body, encoding="utf-8")
            alias = root / "MacOSX.sdk"
            alias.symlink_to(sdk, target_is_directory=True)
            generated = root / "generated"
            module.prepare(sdk, generated)
            import json
            overlay = json.loads(
                (generated / "macos_cpp26_sdk_overlay.json").read_text())
            self.assertEqual(len(overlay["roots"]), 4)
            names = [x["name"] for x in overlay["roots"]]
            self.assertTrue(any("/MacOSX.sdk/" in x for x in names))
            self.assertTrue(any("/MacOSX26.5.sdk/" in x for x in names))
            compiler = shutil.which("clang++")
            if not compiler:
                return
            probe = root / "probe.cpp"
            probe.write_text(
                '#include "CGImage.h"\n#include "ColorSyncDeprecated.h"\n'
                'static_assert(cmGray8Space == 5);\n'
                'int main(){return info() == 15 ? 0 : 1;}\n',
                encoding="utf-8")
            cmd = [compiler, "-std=c++2c", "-Wall", "-Wextra", "-Wpedantic",
                   "-fsyntax-only",
                   "-I", str(alias / pathlib.Path(module.COREGRAPHICS).parent),
                   "-I", str(alias / pathlib.Path(module.COLORSYNC).parent),
                   "-ivfsoverlay", str(generated / "macos_cpp26_sdk_overlay.json"),
                   str(probe)]
            result = subprocess.run(cmd, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_header_already_fixed_is_left_unchanged(self):
        safe = CG_HEADER.replace(module.CG_EXPRESSIONS[0], module.CG_REWRITTEN)
        self.assertEqual(module.rewrite_cg(safe), (safe, 0))
        newest = CG_HEADER.replace(module.CG_EXPRESSIONS[0], module.CG_EXPRESSIONS[1])
        converted, count = module.rewrite_cg(newest)
        self.assertEqual(count, 1)
        self.assertIn(module.CG_REWRITTEN, converted)

    def test_does_not_rewrite_unrelated_enum_expressions(self):
        other = "enum { x = a + b, cmGray8Space = cmGraySpace, t = c + d };"
        self.assertEqual(module.rewrite_color(other), (other, 0))

    def test_config_stays_objcxx26_without_warning_suppression(self):
        cmake = (ROOT / "cmake/platform/macOS/configure.cmake").read_text()
        self.assertIn("OBJCXX_STANDARD 26", cmake)
        self.assertIn("-std=c++26 -fobjc-arc", cmake)
        self.assertIn("prepare_macos_sdk_cpp26.py", cmake)
        self.assertIn("-ivfsoverlay", cmake)
        self.assertNotIn("OBJCXX_STANDARD 23", cmake)
        self.assertNotIn("-Wno-enum-enum-conversion", cmake)

if __name__ == "__main__":
    unittest.main()
