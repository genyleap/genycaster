#!/usr/bin/env python3
"""Check real Qt RHI reflection captures and frozen browser cadence.

The browser branch is exercised through a fixture-only platform selector;
this does not replace browser/device acceptance testing.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
THEME = """pragma Singleton
import QtQuick
QtObject {
    property real reflectionBlurRadius: 24
    property real reflectionGain: 1
    property bool paletteSwitchInProgress: false
}
"""
QML = """import QtQuick
import QtTest
import Farcaster.Client
Item {
    width: 400; height: 220
    Rectangle { id: workspace; width: 160; height: 160; color: "red" }
    NeonReflectionSource { id: reflection; sourceItem: workspace; active: false }
    ShaderEffect {
        id: projected
        x: 200; width: 160; height: 160
        property var source: reflection.texture
    }
    TestCase {
        name: "ReflectionCaptureLifecycle"
        when: windowShown
        function isRed() {
            const sample = grabImage(projected)
            return sample.red(80,80) > 240 && sample.blue(80,80) < 15
        }
        function isBlue() {
            const sample = grabImage(projected)
            return sample.blue(80,80) > 240 && sample.red(80,80) < 15
        }
        function test_lifecycle() {
            const capture = findChild(reflection, "reflectionCapture")
            const horizontal = findChild(reflection, "reflectionHorizontal")
            const vertical = findChild(reflection, "reflectionVertical")
            verify(capture && horizontal && vertical)
            const stages = [capture,horizontal,vertical]
            for (const stage of stages) {
                compare(stage.textureSize, Qt.size(1,1))
                compare(stage.sourceItem,null)
            }
            reflection.active = true
            tryCompare(reflection, "capturing", true)
            tryVerify(isRed, 3000)
            const extent = Qt.size(reflection.textureExtent.width,reflection.textureExtent.height)
            for (const stage of stages) {
                compare(stage.textureSize,extent)
                compare(stage.live,!reflection.browserRenderer)
            }
            reflection.motionActive = true
            workspace.color = "blue"
            wait(350)
            verify(isRed(),"motion must retain the previous complete color field")
            reflection.motionActive = false
            tryVerify(isBlue,3000)
            workspace.color = "red"
            tryVerify(isRed,3000)
            // Resizing keeps the previous allocation until the settle event.
            workspace.width = 200
            verify(reflection.resizingCapture)
            compare(capture.textureSize,extent)
            tryCompare(reflection,"resizingCapture",false,2000)
            verify(capture.textureSize.width > extent.width)
            Theme.paletteSwitchInProgress = true
            compare(reflection.capturing,false)
            compare(capture.textureSize.width,reflection.textureExtent.width)
            Theme.paletteSwitchInProgress = false
            tryVerify(isRed,3000)
            reflection.active = false
            for (const stage of stages) {
                compare(stage.textureSize,Qt.size(1,1))
                compare(stage.sourceItem,null)
            }
            workspace.color = "blue"
            reflection.active = true
            tryVerify(isBlue,3000)
        }
    }
}
"""
def main():
    runner=os.environ.get("QML_TEST_RUNNER") or shutil.which("qmltestrunner")
    if not runner:
        raise SystemExit("Set QML_TEST_RUNNER to Qt qmltestrunner")
    source=(ROOT/"ui/components/visual/NeonReflectionSource.qml").read_text()
    shaders=ROOT/"ui/shaders"
    with tempfile.TemporaryDirectory(prefix="genycaster-reflection-") as tmp:
        path=Path(tmp)
        module=path/"Farcaster/Client"
        module.mkdir(parents=True)
        (module/"qmldir").write_text("module Farcaster.Client\nsingleton Theme 1.0 Theme.qml\n")
        (module/"Theme.qml").write_text(THEME)
        for shader in ("gaussian","reflection-gaussian"):
            shutil.copyfile(shaders/(shader+".frag.qsb"),path/(shader+".frag.qsb"))
        (path/"tst_reflection.qml").write_text(QML)
        for browser in (False,True):
            src=source.replace('Qt.platform.os === "wasm"',str(browser).lower()).replace("qrc:/genycaster/effects/","")
            (path/"NeonReflectionSource.qml").write_text(src)
            result=subprocess.run([runner,"-import",tmp,"-input",tmp],text=True,capture_output=True,timeout=25)
            output=result.stdout+result.stderr
            print("Browser cadence fixture" if browser else "Native live fixture")
            print(output)
            if result.returncode or any(x in output for x in ("Binding loop detected","TypeError:","ReferenceError:","Failed to create")):
                raise SystemExit(1)
if __name__=="__main__":
    main()
