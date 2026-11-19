#!/usr/bin/env python3
"""Validate that WASM cannot ship the native derivatives-dependent QSB bundle."""
from __future__ import annotations

from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
DERIVATIVES = re.compile(r"\b(?:fwidth|dFdx|dFdy)\s*\(")


class WebglShaderCompatibilityTests(unittest.TestCase):
    def test_wasm_generates_separate_portable_qsb(self) -> None:
        cmake = (ROOT / "ui/CMakeLists.txt").read_text()
        start = cmake.index("# WASM MUST NOT reuse")
        end = cmake.index("target_include_directories(genycaster", start)
        config = cmake[start:end]
        self.assertIn("if(EMSCRIPTEN)", config)
        self.assertIn("find_package(Qt6 REQUIRED COMPONENTS ShaderTools)", config)
        self.assertIn("qt_add_shaders(genycaster genycaster_surface_effects_webgl", config)
        self.assertIn('GLSL "100 es,300 es"', config)
        self.assertIn('BASE "${_genycasterWebglShaderRoot}"', config)
        self.assertIn("qt_add_resources(genycaster genycaster_surface_effects_texture", config)
        self.assertIn("else()", config)
        self.assertIn("qt_add_resources(genycaster genycaster_surface_effects", config)
        self.assertIn("shaders/neon.frag.qsb", config)
        self.assertIn("shaders/reflection.frag.qsb", config)
        self.assertIn("fwidth(sdf)", config)
        self.assertIn("dFdx(edgeSdf)", config)

    def test_transformed_shaders_have_no_derivatives_and_keep_resources(self) -> None:
        shaders = ROOT / "ui/shaders"
        source = {name: (shaders / (name + ".frag")).read_text()
                  for name in ("neon", "reflection", "gaussian", "reflection-gaussian")}
        self.assertIsNotNone(DERIVATIVES.search(source["neon"]))
        self.assertIsNotNone(DERIVATIVES.search(source["reflection"]))
        webgl = dict(source)
        for name in ("neon", "reflection"):
            self.assertIn("fwidth(sdf)", webgl[name])
            webgl[name] = webgl[name].replace("fwidth(sdf)", "1.0")
        needle = "length(vec2(dFdx(edgeSdf), dFdy(edgeSdf)))"
        self.assertIn(needle, webgl["reflection"])
        webgl["reflection"] = webgl["reflection"].replace(needle, "1.0")
        for name in ("gaussian", "reflection-gaussian"):
            self.assertIn("i <= 47", webgl[name])
            webgl[name] = webgl[name].replace("i <= 47", "i <= 15").replace(
                "544.5", "128.0")
            self.assertIn("i <= 15", webgl[name])
            self.assertNotIn("i <= 47", webgl[name])
        for name, shader in webgl.items():
            with self.subTest(name=name):
                self.assertIsNone(DERIVATIVES.search(shader))
                self.assertIn("#version 440", shader)
                self.assertIn("qt_Opacity", shader)
                self.assertIn("fragColor", shader)
                # Fragment-shader outputs and uniform names must remain stable.
                self.assertIn("sampler2D", shader)
        # Native QSB remain untouched to preserve Metal/D3D/Vulkan rendering.
        for name in source:
            self.assertTrue((shaders / (name + ".frag.qsb")).is_file())

    def test_webgl_reflections_use_portable_textures_and_bounded_capture(self) -> None:
        source = (ROOT / "ui/components/visual/NeonReflectionSource.qml").read_text()
        self.assertIn('readonly property bool browserRenderer: Qt.platform.os === "wasm"', source)
        self.assertEqual(source.count(
            "format: root.browserRenderer ? ShaderEffectSource.RGBA8 : ShaderEffectSource.RGBA16F"), 3)
        self.assertIn("live: root.capturing && !root.browserRenderer", source)
        self.assertIn("running: root.browserRenderer && root.capturing", source)
        self.assertIn("onTriggered: capture.scheduleUpdate()", source)
        # The native source preserves its full precision and live captures.
        self.assertEqual(source.count("live: root.capturing"), 3)

    def test_packaging_rejects_derivatives_dependent_wasm_binaries(self) -> None:
        packager = (ROOT / "ci/package_wasm_assets.py").read_text()
        self.assertIn('forbidden_shader = b"#extension GL_OES_standard_derivatives : require"', packager)
        self.assertIn("if forbidden_shader in overlap + block:", packager)
        self.assertIn("parser.error(\"WASM contains a derivatives-dependent WebGL1 shader", packager)
        self.assertLess(packager.index("forbidden_shader ="), packager.index("hash_full = digest(wasm)"))

    def test_packaging_rejects_reproduced_production_shader_payload(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)
            # Minimal WASM-like payload containing the exact browser-console
            # GLSL directive: rejection must occur before asset processing.
            (path / "genycaster.wasm").write_bytes(
                b"\\0asm\\1\\0\\0\\0" + b"#extension GL_OES_standard_derivatives : require")
            result = subprocess.run(
                [sys.executable, str(ROOT / "ci/package_wasm_assets.py"), "--dir", str(path)],
                capture_output=True, text=True, check=False)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("derivatives-dependent WebGL1 shader", result.stderr)

    def test_architecture_combobox_keeps_both_options_visible(self) -> None:
        js = (ROOT / "ui/wasm/native-downloads.js").read_text()
        popup = js.split("function layoutOptions()", 1)[1].split(
            "function closeOptions", 1)[0]
        self.assertIn("window.innerHeight", popup)
        self.assertIn("rowCount * 47", popup)
        self.assertIn("architectureOptions.style.height = panelHeight", popup)
        self.assertIn("architectureOptions.style.maxHeight = panelHeight", popup)
        self.assertNotIn("dialogBox.bottom - triggerBox.bottom", popup)
        self.assertIn("const above =", popup)


if __name__ == "__main__":
    unittest.main()
