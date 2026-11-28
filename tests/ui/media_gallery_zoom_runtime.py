#!/usr/bin/env python3
"""Exercise real Qt pointer handlers and full-screen gallery without a full app build."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
MEDIA = ROOT / "ui/components/media"
ZOOM_TEST = """import QtQuick
import QtTest
Item {
    width: 640; height: 480
    MediaZoomViewport {
        id: zoom
        anchors.fill: parent
        fittedWidth: 640
        fittedHeight: 480
        Rectangle { anchors.fill: parent; color: "red" }
    }
    TestCase {
        name: "MediaZoomGestures"
        when: windowShown
        function init() {
            zoom.interactionEnabled = true
            zoom.reset()
            verify(zoom.canInteract)
            verify(zoom.visible && zoom.enabled)
            wait(50)
        }
        function test_wheelAndPan() {
            mouseWheel(zoom, 480, 320, 0, 120)
            tryVerify(() => zoom.zoomFactor > 1)
            verify(zoom.offsetX < 0)
            verify(zoom.offsetY < 0)
            mousePress(zoom, 300, 230)
            mouseMove(zoom, 350, 230, 30)
            mouseMove(zoom, 395, 230, 30)
            mouseRelease(zoom, 395, 230)
            verify(zoom.offsetX > -50)
            mouseWheel(zoom, 480, 320, 0, -120)
            tryCompare(zoom, "zoomFactor", 1)
            compare(zoom.offsetX, 0)
            compare(zoom.offsetY, 0)
        }
        function test_doubleTapAndLimits() {
            const touch = touchEvent(zoom)
            touch.press(0, zoom, 320, 240).commit()
            wait(30)
            touch.release(0, zoom, 320, 240).commit()
            wait(40)
            touch.press(0, zoom, 320, 240).commit()
            wait(30)
            touch.release(0, zoom, 320, 240).commit()
            tryCompare(zoom, "zoomFactor", 2.5, 1000)
            zoom.zoomAt(100, 320, 240)
            compare(zoom.zoomFactor, 5)
            zoom.panBy(1e8, -1e8)
            compare(zoom.offsetX, 1280)
            compare(zoom.offsetY, -960)
            zoom.interactionEnabled = false
            compare(zoom.zoomFactor, 1)
            compare(zoom.offsetX, 0)
            compare(zoom.offsetY, 0)
        }
        function test_touchPinch() {
            const touch = touchEvent(zoom)
            touch.press(0, zoom, 245, 240).press(1, zoom, 395, 240).commit()
            wait(30)
            touch.move(0, zoom, 225, 240).move(1, zoom, 415, 240).commit()
            wait(30)
            touch.move(0, zoom, 190, 240).move(1, zoom, 450, 240).commit()
            wait(30)
            touch.move(0, zoom, 150, 240).move(1, zoom, 490, 240).commit()
            wait(30)
            touch.move(0, zoom, 75, 240).move(1, zoom, 565, 240).commit()
            wait(30)
            touch.release(0, zoom, 75, 240).release(1, zoom, 565, 240).commit()
            tryCompare(zoom, "gestureActive", false)
            verify(zoom.zoomFactor > 1.2)
        }
        function test_fittedPortraitBounds() {
            zoom.fittedWidth = 240
            zoom.zoomAt(2, 600, 100)
            compare(zoom.offsetX, 0)
            verify(zoom.offsetY > 0)
            zoom.fittedWidth = 640
        }
    }
}
"""
GALLERY_TEST = """import QtQuick
import QtQuick.Controls
import QtTest
import Farcaster.Client
Item {
    width: 800; height: 600
    property QtObject dataStorage: QtObject {
        function remoteImageSource(source, category, manual) { return source }
    }
    property QtObject renderingController: QtObject {
        property real mediaDecodeScale: 1
    }
    property var openProofConnections: null
    MediaGallery { id: viewer }
    TestCase {
        name: "MediaGalleryIntegration"
        when: windowShown
        function init() { Theme.reduceMotion = false }
        function openGallery() {
            viewer.openAt([{imageSource: Qt.resolvedUrl("wide.svg")},
                           {imageSource: Qt.resolvedUrl("portrait.svg")}], 0)
            tryCompare(viewer, "opened", true, 2000)
            tryVerify(() => viewer.currentZoom && viewer.currentZoom.canInteract, 2000)
        }
        function test_navigationAndZoom() {
            openGallery()
            const stage = findChild(viewer.contentItem, "mediaGalleryStage")
            verify(stage !== null)
            const zoom = viewer.currentZoom
            compare(zoom.zoomFactor, 1)
            verify(stage.interactive)
            mouseWheel(zoom, 400, 300, 0, 120)
            tryVerify(() => zoom.zoomFactor > 1)
            compare(stage.interactive, false)
            zoom.zoomAt(3, 400, 300)
            compare(zoom.zoomFactor, 3)
            compare(zoom.fittedWidth, 800)
            compare(zoom.fittedHeight, 400)
            viewer.next()
            tryCompare(viewer, "currentIndex", 1)
            tryVerify(() => viewer.currentZoom && viewer.currentZoom.canInteract)
            compare(viewer.currentZoom.zoomFactor, 1)
            compare(zoom.zoomFactor, 1)
            viewer.previous()
            tryCompare(viewer, "currentIndex", 0)
            tryVerify(() => viewer.currentZoom && viewer.currentZoom.canInteract)
            compare(viewer.currentZoom.zoomFactor, 1)
            viewer.close()
            tryCompare(viewer, "visible", false, 2000)
            compare(viewer.itemCount, 0)
        }
        function test_openAnimation() {
            Theme.reduceMotion = false
            const stage = findChild(viewer.contentItem, "mediaGalleryStage")
            verify(stage !== null)
            viewer.openAt([{imageSource: Qt.resolvedUrl("wide.svg")}], 0)
            tryVerify(() => stage.scale < 1, 300)
            tryCompare(viewer, "opened", true)
            compare(stage.scale, 1)
            viewer.close()
            tryCompare(viewer, "visible", false)
        }
        function test_reduceMotion() {
            Theme.reduceMotion = true
            openGallery()
            const stage = findChild(viewer.contentItem, "mediaGalleryStage")
            compare(stage.scale, 1)
            compare(viewer.currentZoom.zoomFactor, 1)
            viewer.close()
            tryCompare(viewer, "visible", false)
        }
    }
}
"""
def main():
    runner = os.environ.get("QML_TEST_RUNNER") or shutil.which("qmltestrunner")
    if not runner:
        raise SystemExit("Set QML_TEST_RUNNER to the Qt qmltestrunner path")
    with tempfile.TemporaryDirectory(prefix="genycaster-media-viewer-") as tmp:
        path = Path(tmp)
        for f in ("MediaGallery.qml", "MediaZoomViewport.qml"):
            shutil.copyfile(MEDIA/f, path/f)
        shutil.copyfile(ROOT/"ui/components/composer/StickerCatalog.js", path/"StickerCatalog.js")
        module = path/"Farcaster/Client"
        module.mkdir(parents=True)
        (module/"qmldir").write_text("module Farcaster.Client\nsingleton Theme 1.0 Theme.qml\nsingleton LocaleFormat 1.0 LocaleFormat.qml\n")
        props = set(re.findall(r"\bTheme\.([A-Za-z]\w*)", (MEDIA/"MediaGallery.qml").read_text()))
        lines = ["pragma Singleton", "import QtQuick", "QtObject {"]
        for prop in sorted(props):
            if prop == "keyboardFocusVisible":
                lines.append("function keyboardFocusVisible(item) { return false }")
            elif prop == "reduceMotion" or prop == "rtl":
                lines.append(f"property bool {prop}: false")
            elif prop == "uiDirection":
                lines.append("property int uiDirection: Qt.LeftToRight")
            elif prop.startswith("icon"):
                lines.append(f'property string {prop}: "x"')
            elif prop.startswith("mediaViewer") or prop == "accent":
                lines.append(f'property color {prop}: "#777777"')
            elif prop == "weightDemiBold":
                lines.append("property int weightDemiBold: Font.DemiBold")
            else:
                lines.append(f"property int {prop}: " + ("100" if prop.startswith("motion") else "12"))
        lines.append("}")
        (module/"Theme.qml").write_text("\n".join(lines))
        (module/"LocaleFormat.qml").write_text("pragma Singleton\nimport QtQuick\nQtObject { function formatInteger(n) { return String(n) } }")
        stubs = {
            "GenySpinner": "Item { property bool running: false }",
            "Icon": 'Item { property string text: ""; property color color: "white"; property int preferredSize: 24 }',
            "DirectionAwareLabel": "Label { property bool forceLeftToRight: false }",
            "Superellipse": 'Rectangle { property color fillColor: "transparent"; property real borderWidth: 0; property color borderColor: "transparent"; property real curveDepth: 0; color: fillColor }',
            "ActionButton": 'Button { property bool compact: false; property bool quiet: false; property bool iconOnly: false; property string iconGlyph: "" }',
            "SuperellipseImage": 'Item { property url source; property string mediaCategory; property bool manualLoadRequested; property int requestedSourceWidth; property int requestedSourceHeight; property int fillMode; property real curveDepth; property real borderWidth; property color borderColor; property color fillColor }',
        }
        for name, body in stubs.items():
            (path/(name+".qml")).write_text("import QtQuick\nimport QtQuick.Controls\n"+body)
        for name,w,h in [("wide",400,200),("portrait",200,400)]:
            (path/(name+".svg")).write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}"><rect width="{w}" height="{h}" fill="red"/></svg>')
        (path/"tst_zoom.qml").write_text(ZOOM_TEST)
        (path/"tst_gallery.qml").write_text(GALLERY_TEST)
        env = dict(os.environ, QT_QPA_PLATFORM=os.environ.get("QT_QPA_PLATFORM", "offscreen"),
                   QT_QUICK_BACKEND="software")
        args = [runner,"-import",tmp,"-input",str(path / os.environ["ZOOM_TEST_FILE"])] if "ZOOM_TEST_FILE" in os.environ else [runner,"-import",tmp,"-input",tmp]
        result = subprocess.run(args,env=env,text=True,capture_output=True,timeout=25)
        output=result.stdout+result.stderr
        print(output)
        if result.returncode or any(x in output for x in ("Binding loop detected", "TypeError:", "ReferenceError:")):
            raise SystemExit(1)
if __name__ == "__main__":
    main()
