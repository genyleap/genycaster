#!/usr/bin/env python3
"""Test the production backdrop's capture resource and profile bindings."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
THEME = """pragma Singleton
import QtQuick
QtObject {
    property bool dark: true
    property string colorThemeId: "fixture"
    property color customAccent: "blue"
    property int materialSurfaceDuration: 120
}
"""
TEST = """import QtQuick
import QtTest
Item {
    width: 800; height: 600
    property QtObject renderingController: QtObject {
        property string profile: "adaptive"
        property string effectiveQuality: "balanced"
    }
    Rectangle { id: workspace; anchors.fill: parent; color: "#6889ac" }
    DrawerBackdrop {
        id: backdrop
        sourceItem: workspace
        efficientNativeBlur: true
    }
    TestCase {
        name: "BackdropResourceBudget"
        when: windowShown
        function test_captureLifecycle() {
            const capture = findChild(backdrop, "drawerBackdropCapture")
            verify(capture !== null)
            compare(capture.sourceItem, null)
            compare(capture.textureSize, Qt.size(1,1))
            backdrop.active = true
            tryCompare(capture, "sourceItem", workspace)
            compare(capture.textureSize, Qt.size(384,288))
            const initial = backdrop.captureScale
            const initialBlur = backdrop.blurStrength
            compare(initial, 0.90)
            renderingController.effectiveQuality = "efficiency"
            compare(backdrop.captureScale, initial)
            compare(backdrop.blurStrength, initialBlur)
            renderingController.effectiveQuality = "fidelity"
            compare(backdrop.captureScale, initial)
            compare(capture.textureSize, Qt.size(384,288))
            renderingController.profile = "efficiency"
            compare(backdrop.captureScale, 0.72)
            compare(backdrop.blurStrength, 0.46)
            backdrop.active = false
            compare(capture.sourceItem, null)
            compare(capture.textureSize, Qt.size(1,1))
            backdrop.active = true
            tryCompare(capture, "sourceItem", workspace)
            compare(capture.textureSize, Qt.size(384,288))
            backdrop.active = false
        }
        function test_gaussianSnapshot() {
            if (backdrop.GraphicsInfo.api === GraphicsInfo.Software)
                skip("Pixel comparison requires a real RHI backend")
            renderingController.profile = "adaptive"
            backdrop.efficientNativeBlur = false
            backdrop.addScrim = false
            workspace.color = "red"
            backdrop.active = true
            const intermediate = findChild(backdrop, "drawerBackdropIntermediate")
            verify(intermediate && intermediate.sourceItem)
            compare(intermediate.live, false)
            const red = () => {
                const image = grabImage(backdrop)
                return image.red(400,300) > 240 && image.blue(400,300) < 15
            }
            const blue = () => {
                const image = grabImage(backdrop)
                return image.blue(400,300) > 240 && image.red(400,300) < 15
            }
            tryVerify(red,3000)
            wait(400)
            verify(red(),"opening snapshot must settle")
            workspace.color = "blue"
            wait(250)
            const frozen = grabImage(backdrop)
            console.info("Frozen pixel",frozen.red(400,300),frozen.green(400,300),frozen.blue(400,300))
            verify(red(),"source mutation must not replace the frozen backdrop")
            backdrop.refreshSnapshot()
            tryVerify(blue,3000)
            backdrop.active = false
            compare(intermediate.sourceItem,null)
            compare(intermediate.textureSize,Qt.size(1,1))
        }
    }
}
"""
def main():
    runner = os.environ.get("QML_TEST_RUNNER") or shutil.which("qmltestrunner")
    if not runner:
        raise SystemExit("Set QML_TEST_RUNNER to the Qt qmltestrunner path")
    with tempfile.TemporaryDirectory(prefix="genycaster-backdrop-") as tmp:
        path = Path(tmp)
        module = path / "Farcaster/Client"
        module.mkdir(parents=True)
        (module/"qmldir").write_text("module Farcaster.Client\nsingleton Theme 1.0 Theme.qml\n")
        (module/"Theme.qml").write_text(THEME)
        # The standalone fixture has no application qrc; use the identical
        # compiled shader through a local resource URL.
        source = (ROOT/"ui/components/visual/DrawerBackdrop.qml").read_text()
        (path/"DrawerBackdrop.qml").write_text(source.replace("qrc:/genycaster/effects/gaussian.frag.qsb", "gaussian.frag.qsb"))
        shutil.copyfile(ROOT/"ui/shaders/gaussian.frag.qsb",path/"gaussian.frag.qsb")
        (path/"tst_backdrop.qml").write_text(TEST)
        env = dict(os.environ, QT_QPA_PLATFORM=os.environ.get("QT_QPA_PLATFORM","offscreen"),
                   QT_QUICK_BACKEND=os.environ.get("QT_QUICK_BACKEND","software"))
        result = subprocess.run([runner, "-import", tmp, "-input", tmp], env=env,
                                capture_output=True, text=True, timeout=20)
        output = result.stdout + result.stderr
        print(output)
        if result.returncode or any(x in output for x in ("Binding loop detected","ReferenceError:","TypeError:")):
            raise SystemExit(1)
if __name__ == "__main__":
    main()
