#!/usr/bin/env python3
"""Check neutral Halo pixel parity and sampled/analytic transitions on Qt RHI."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SHADERS = ROOT / "ui/shaders"

QML = """import QtQuick
import QtTest
Item {
    width: 256; height: 128
    Rectangle { id: chroma; width: 16; height: 16; color: "red"; visible: false }
    ShaderEffectSource {
        id: field
        sourceItem: chroma
        hideSource: false
        visible: false
    }
    component Material: ShaderEffect {
        width: 128; height: 128
        property vector4d surfaceParameters: Qt.vector4d(width,height,20,4)
        property vector4d focusParameters: Qt.vector4d(64,0,40,0)
        property vector4d lightParameters: Qt.vector4d(0.85,0.9,20,1)
        property vector4d tintParameters: Qt.vector4d(0.2,0.8,0.4,0)
        property vector4d cornerFlags: Qt.vector4d(1,1,1,1)
        property variant environmentSource: field
        property vector2d environmentSize: Qt.vector2d(128,128)
        property rect environmentRect: Qt.rect(128,0,128,128)
        property vector4d environmentParameters: Qt.vector4d(0,90,1,48)
        property real steadyActivity: 0
    }
    Material { id: reference }
    Material {
        id: optimized
        x: 128
        property bool sampled: false
        property string prefix: "native"
        fragmentShader: Qt.resolvedUrl(prefix + (sampled ? "-sampled.qsb" : "-analytic.qsb"))
        environmentSource: sampled ? field : null
        environmentParameters: Qt.vector4d(sampled ? 0.9 : 0,90,1,48)
        surfaceParameters: reference.surfaceParameters
        focusParameters: reference.focusParameters
        lightParameters: reference.lightParameters
        tintParameters: reference.tintParameters
        cornerFlags: reference.cornerFlags
        steadyActivity: reference.steadyActivity
        opacity: reference.opacity
    }
    TestCase {
        name: "NeonShaderEquivalence"
        when: windowShown
        function test_neutral_data() {
            const cases = []
            for (const prefix of ["native","web"])
                for (const steady of [0,0.45,1])
                    for (const tint of [0,0.65])
                        for (const square of [false,true])
                            cases.push({tag:prefix+"-"+steady+"-"+tint+"-"+square,
                                        prefix,steady,tint,square})
            return cases
        }
        function comparePixels() {
            wait(40)
            const a = grabImage(reference)
            const b = grabImage(optimized)
            let maxDelta=0, nonzero=false
            for(let y=0;y<a.height;++y)
                for(let x=0;x<a.width;++x) {
                    nonzero=nonzero || a.red(x,y)>10 || a.green(x,y)>10 || a.blue(x,y)>10
                    maxDelta=Math.max(maxDelta,Math.abs(a.red(x,y)-b.red(x,y)),
                        Math.abs(a.green(x,y)-b.green(x,y)),
                        Math.abs(a.blue(x,y)-b.blue(x,y)),
                        Math.abs(a.alpha(x,y)-b.alpha(x,y)))
                }
            verify(nonzero,"reference rim cannot be blank")
            verify(maxDelta<=1,"maximum channel delta "+maxDelta)
        }
        function test_neutral(row) {
            optimized.prefix=row.prefix
            optimized.sampled=false
            reference.fragmentShader=Qt.resolvedUrl(row.prefix+"-sampled.qsb")
            reference.environmentParameters=Qt.vector4d(0,90,1,48)
            reference.steadyActivity=row.steady
            reference.tintParameters=Qt.vector4d(0.2,0.8,0.4,row.tint)
            reference.cornerFlags=row.square ? Qt.vector4d(0,1,1,0) : Qt.vector4d(1,1,1,1)
            reference.opacity=0.8
            tryCompare(reference,"status",ShaderEffect.Compiled)
            tryCompare(optimized,"status",ShaderEffect.Compiled)
            comparePixels()
        }
        function test_switchTextureProvider() {
            optimized.prefix="native"
            reference.fragmentShader=Qt.resolvedUrl("native-sampled.qsb")
            reference.steadyActivity=1
            reference.tintParameters=Qt.vector4d(0,0,0,0)
            reference.opacity=1
            reference.environmentParameters=Qt.vector4d(0,90,1,48)
            optimized.sampled=false
            tryCompare(optimized,"status",ShaderEffect.Compiled)
            comparePixels()
            optimized.sampled=true
            tryCompare(optimized,"status",ShaderEffect.Compiled)
            wait(60)
            const neutral=grabImage(reference)
            const colored=grabImage(optimized)
            let changed=false
            for(let y=0;y<neutral.height;++y)
                for(let x=0;x<neutral.width;++x)
                    changed=changed || Math.abs(neutral.green(x,y)-colored.green(x,y))>5
            verify(changed,"sampled variant must produce actual chromatic reflection")
            optimized.sampled=false
            tryCompare(optimized,"status",ShaderEffect.Compiled)
            comparePixels()
        }
    }
}
"""

def main():
    runner = os.environ.get("QML_TEST_RUNNER") or shutil.which("qmltestrunner")
    if not runner:
        raise SystemExit("Set QML_TEST_RUNNER")
    qsb = os.environ.get("QT_QSB_EXECUTABLE") or str(Path(runner).with_name("qsb"))
    with tempfile.TemporaryDirectory(prefix="genycaster-neon-") as directory:
        path = Path(directory)
        shutil.copyfile(SHADERS/"neon.frag.qsb", path/"native-sampled.qsb")
        shutil.copyfile(SHADERS/"neon-analytic.frag.qsb", path/"native-analytic.qsb")
        web = (SHADERS/"neon.frag").read_text().replace("fwidth(sdf)", "1.0")
        source = path/"neon-web.frag"
        source.write_text(web)
        for kind in ("sampled","analytic"):
            define = ["-D","GENYCASTER_NEON_ANALYTIC=1"] if kind=="analytic" else []
            subprocess.run([qsb,"--glsl","100es,300es,120,150","--hlsl","50","--msl","12",
                            *define,"-o",str(path/f"web-{kind}.qsb"),str(source)],check=True)
        for prefix in ("native","web"):
            dump=subprocess.check_output([qsb,"--dump",str(path/f"{prefix}-analytic.qsb")],text=True)
            if "environmentSource" in dump or "sampler2D" in dump:
                raise SystemExit("Analytic variant unexpectedly contains a sampler")
            if "cornerFlags" not in dump:
                raise SystemExit("Analytic variant lost its contour ABI")
        (path/"tst_neon.qml").write_text(QML)
        result=subprocess.run([runner,"-input",directory],capture_output=True,text=True,timeout=60)
        output=result.stdout+result.stderr
        print(output)
        if result.returncode or any(x in output for x in (
                "Failed to create","ShaderEffect: Failed","QWARN","Binding loop detected",
                "TypeError:","ReferenceError:")):
            raise SystemExit(1)

if __name__=="__main__":
    main()
