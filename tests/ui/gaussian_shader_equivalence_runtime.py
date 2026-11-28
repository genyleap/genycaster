#!/usr/bin/env python3
"""Compare the shipping Gaussian QSB against the original arithmetic on Qt RHI.

The WASM kernel is also compiled to GLSL ES 100/300 and compared using a
desktop GL variant of the same source. This is not a browser/Metal benchmark.
"""
from pathlib import Path
import os
import shutil
import struct
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
SHADERS = ROOT / "ui/shaders"
HEADER = """#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 sampleStep;
};
layout(binding = 1) uniform sampler2D source;
"""
CHROMA = """
vec4 chromaticSample(vec2 uv) {
    vec4 pixel = texture(source, uv);
    float visibleChroma = max(pixel.r, max(pixel.g, pixel.b))
                        - min(pixel.r, min(pixel.g, pixel.b));
    return pixel * smoothstep(0.08, 0.22, visibleChroma);
}
"""
def reference(chroma, web):
    sampler = "chromaticSample" if chroma else "texture"
    def sample(uv):
        return f"{sampler}({uv})" if chroma else f"{sampler}(source, {uv})"
    return HEADER + (CHROMA if chroma else "") + f"""
void main() {{
    vec4 result = {sample("qt_TexCoord0")};
    float total = 1.0;
    for (int i = 1; i <= {15 if web else 47}; i += 2) {{
        float first = exp(-float(i * i) / {128.0 if web else 544.5});
        float second = exp(-float((i + 1) * (i + 1)) / {128.0 if web else 544.5});
        float weight = first + second;
        vec2 offset = (float(i) + second / weight) * sampleStep;
        result += {sample("clamp(qt_TexCoord0 + offset, vec2(0.0), vec2(1.0))")} * weight;
        result += {sample("clamp(qt_TexCoord0 - offset, vec2(0.0), vec2(1.0))")} * weight;
        total += 2.0 * weight;
    }}
    fragColor = (result / total) * qt_Opacity;
}}
"""
def png(path):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind+data))
    rows=[]
    for y in range(128):
        row=bytearray([0])
        for x in range(128):
            # High-frequency color, alpha edges and nearly neutral regions.
            if x < 24:
                rgba=(x*8, x*8+5, x*8+10, 255)
            elif x < 64:
                rgba=(255, (x*11+y*3)%256, 10, (y*5)%256)
            else:
                rgba=((x*17+y*7)%256, (x*7+y*11)%256, (x*3+y*19)%256, 255)
            row.extend(rgba)
        rows.append(row)
    path.write_bytes(b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",128,128,8,6,0,0,0))
                     +chunk(b"IDAT",zlib.compress(b"".join(rows)))+chunk(b"IEND",b""))

QML = """import QtQuick
import QtTest
Item {
    width: 256; height: 128
    Image {
        id: fixture
        source: "fixture.png"
        width: 128; height: 128
        visible: false
        smooth: true
    }
    ShaderEffect {
        id: original
        width: 128; height: 128
        property var source: fixture
        property vector2d sampleStep: Qt.vector2d(0.5 / width, 0)
    }
    ShaderEffect {
        id: optimized
        x: 128
        width: 128; height: 128
        property var source: fixture
        property vector2d sampleStep: original.sampleStep
    }
    TestCase {
        name: "GaussianPixelEquivalence"
        when: windowShown
        function test_equivalence_data() {
            const rows = []
            for (const kernel of ["native", "web"])
                for (const shader of ["gaussian", "reflection-gaussian"])
                    for (const axis of ["horizontal", "vertical"])
                        for (const opacity of [1.0, 0.53])
                            rows.push({tag: kernel + "-" + shader + "-" + axis + "-" + opacity,
                                       kernel:kernel,shader:shader,axis:axis,alpha:opacity})
            return rows
        }
        function test_equivalence(row) {
            tryCompare(fixture, "status", Image.Ready)
            original.fragmentShader = Qt.resolvedUrl(row.kernel+"-"+row.shader+"-reference.qsb")
            optimized.fragmentShader = Qt.resolvedUrl(row.kernel+"-"+row.shader+"-optimized.qsb")
            original.sampleStep = row.axis === "horizontal"
                ? Qt.vector2d(0.5/128, 0) : Qt.vector2d(0, 0.5/128)
            original.opacity = row.alpha
            optimized.opacity = row.alpha
            tryCompare(original, "status", ShaderEffect.Compiled)
            tryCompare(optimized, "status", ShaderEffect.Compiled)
            // Compiled may be emitted after the first rendered frame.
            // grabImage synchronizes the scene; do not require a second
            // frame from a demand-driven scene that is already up to date.
            wait(60)
            const before = grabImage(original)
            const after = grabImage(optimized)
            verify(before.width === 128 && after.width === 128)
            let maximum = 0
            let nonzero = false
            for (let y = 0; y < 128; ++y) {
                for (let x = 0; x < 128; ++x) {
                    nonzero = nonzero || before.red(x,y) > 20
                    maximum = Math.max(maximum,
                        Math.abs(before.red(x,y)-after.red(x,y)),
                        Math.abs(before.green(x,y)-after.green(x,y)),
                        Math.abs(before.blue(x,y)-after.blue(x,y)),
                        Math.abs(before.alpha(x,y)-after.alpha(x,y)))
                }
            }
            verify(nonzero, "reference cannot be blank")
            verify(maximum <= 1, "8-bit channel delta: "+maximum)
            console.info("Maximum 8-bit channel delta:",maximum)
        }
    }
}
"""
def main():
    runner = os.environ.get("QML_TEST_RUNNER") or shutil.which("qmltestrunner")
    if not runner:
        raise SystemExit("Set QML_TEST_RUNNER to Qt qmltestrunner")
    qsb = os.environ.get("QT_QSB_EXECUTABLE") or str(Path(runner).with_name("qsb"))
    if not Path(qsb).is_file():
        raise SystemExit("Set QT_QSB_EXECUTABLE to Qt qsb")
    with tempfile.TemporaryDirectory(prefix="genycaster-gaussian-") as tmp:
        path = Path(tmp)
        def bake(source, name):
            shader = path/(name+".frag")
            shader.write_text(source)
            subprocess.run([qsb, "--glsl", "100es,300es,120,150", "--hlsl", "50",
                            "--msl", "12", "-o", str(path/(name+".qsb")), str(shader)], check=True)
        for kernel in ("native", "web"):
            for name in ("gaussian", "reflection-gaussian"):
                prefix=kernel+"-"+name
                bake(reference(name.startswith("reflection"),kernel=="web"),prefix+"-reference")
                if kernel == "native":
                    shutil.copyfile(SHADERS/(name+".frag.qsb"),path/(prefix+"-optimized.qsb"))
                else:
                    src=(SHADERS/(name+".frag")).read_text().replace(
                        "#define GENYCASTER_WEBGL_KERNEL 0","#define GENYCASTER_WEBGL_KERNEL 1")
                    bake(src,prefix+"-optimized")
        png(path/"fixture.png")
        (path/"tst_gaussian.qml").write_text(QML)
        # Respect the selected host graphics backend, including Cocoa/Metal.
        env=dict(os.environ)
        result=subprocess.run([runner,"-input",tmp],env=env,text=True,capture_output=True,timeout=60)
        output=result.stdout+result.stderr
        print(output)
        if result.returncode or any(x in output for x in ("Failed to create", "ShaderEffect: Failed","Binding loop detected","ReferenceError:","TypeError:")):
            raise SystemExit(1)
if __name__=="__main__":
    main()
