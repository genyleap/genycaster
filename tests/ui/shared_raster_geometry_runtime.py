#!/usr/bin/env python3
"""Qt runtime tests for the production shared image, with network/visual chrome stubbed."""
from pathlib import Path
import os
import re
import shutil
import struct
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
IMAGE = ROOT/"ui/components/visual/SuperellipseImage.qml"
TEST = """import QtQuick
import QtTest
import Farcaster.Client
Item {
    width: 640; height: 480
    property QtObject dataStorage: QtObject {
        signal policyChanged()
        function autoDownloadAllowed(category) { return true }
        function remoteImageSource(source, category, manual) {
            return source.startsWith("image://remote/") ? Qt.resolvedUrl("landscape.png").toString() : source
        }
    }
    property QtObject renderingController: QtObject {
        property real mediaDecodeScale: 1
        property int layerSampleCount: 1
        property bool mediaDiagnosticsEnabled: false
        property int diagnosticCount: 0
        property var lastGeometry: []
        function recordRasterGeometry(category, requestedWidth, requestedHeight,
                                      sourceWidth, sourceHeight, viewWidth, viewHeight) {
            ++diagnosticCount
            lastGeometry = [category,requestedWidth,requestedHeight,sourceWidth,
                            sourceHeight,viewWidth,viewHeight]
        }
        property bool shuttingDown: false
        property bool autoGrant: true
        property int requests: 0
        property int cancellations: 0
        function requestMediaPresentation(requester, generation) {
            ++requests
            if (autoGrant)
                requester.acceptMediaPresentation(generation)
        }
        function cancelMediaPresentation(requester) { ++cancellations }
    }
    Item {
        id: card
        width: 640; height: 480
    SuperellipseImage {
        id: image
        width: 240; height: 240
        framePacedPresentation: false
        clipToSuperellipse: false
        smoothReveal: true
        showLoadingIndicator: false
        fillMode: Image.PreserveAspectFit
    }
    }
    TestCase {
        name: "SharedRasterGeometry"
        when: windowShown
        function init() {
            card.visible = true
            renderingController.shuttingDown = false
            renderingController.autoGrant = true
            image.framePacedPresentation = false
            image.source = ""
            image.width = 240
            image.height = 240
            image.fillMode = Image.PreserveAspectFit
            image.source = Qt.resolvedUrl("landscape.png")
            tryCompare(image, "ready", true)
            tryVerify(() => image.intrinsicAspect > 0)
        }
        function test_optInDiagnostics() {
            renderingController.diagnosticCount = 0
            renderingController.mediaDiagnosticsEnabled = false
            image.source = Qt.resolvedUrl("portrait.png")
            tryCompare(image, "ready", true)
            compare(renderingController.diagnosticCount, 0)
            renderingController.mediaDiagnosticsEnabled = true
            image.source = Qt.resolvedUrl("landscape.png")
            tryCompare(image, "ready", true)
            tryVerify(() => renderingController.diagnosticCount > 0)
            const geometry = renderingController.lastGeometry
            compare(geometry.length, 7)
            verify(geometry[1] > 0 && geometry[2] > 0)
            verify(geometry[3] > 0 && geometry[4] > 0)
            compare(geometry[5], 240)
            compare(geometry[6], 240)
            renderingController.mediaDiagnosticsEnabled = false
        }
        function test_pendingTicketWithdrawal() {
            renderingController.autoGrant = false
            image.framePacedPresentation = true
            image.source = "image://remote/one"
            tryCompare(image,"presentationAdmissionPending",true)
            verify(image.decodedReady)
            verify(!image.ready)
            const oldGeneration = image.presentationGeneration
            const requests = renderingController.requests
            const cancellations = renderingController.cancellations
            const resident = image.activeSource.toString()
            card.visible = false
            tryCompare(image,"rasterEligible",false)
            compare(image.presentationAdmissionPending,false)
            compare(renderingController.cancellations,cancellations+1)
            image.acceptMediaPresentation(oldGeneration)
            compare(image.ready,false)
            compare(image.activeSource.toString(),resident)
            card.visible = true
            tryCompare(image,"presentationAdmissionPending",true)
            compare(renderingController.requests,requests+1)
            const freshGeneration = image.presentationGeneration
            verify(freshGeneration !== oldGeneration)
            image.acceptMediaPresentation(oldGeneration)
            compare(image.presentationAdmissionPending,true)
            image.acceptMediaPresentation(freshGeneration)
            compare(image.ready,true)
            compare(image.presentationAdmissionPending,false)
            card.visible = false
            card.visible = true
            compare(image.ready,true)
            compare(renderingController.requests,requests+1)
        }
        function test_shutdownBlocksNewRaster() {
            renderingController.shuttingDown = true
            const requests = renderingController.requests
            image.framePacedPresentation = true
            image.source = "image://remote/one"
            wait(150)
            compare(image.activeSource.toString(),"")
            compare(renderingController.requests,requests)
            compare(image.presentationReady,false)
            renderingController.shuttingDown = false
        }
        function test_resizeAndEviction() {
            const ratio = image.intrinsicAspect
            fuzzyCompare(ratio, 2, 0.02)
            const intrinsic = Qt.size(image.rememberedIntrinsicSize.width,
                                      image.rememberedIntrinsicSize.height)
            compare(image.paintedWidth, 240)
            fuzzyCompare(image.paintedHeight, 120, 1)
            const firstWidth = image.stableDecodeSize.width
            image.width = 600
            image.height = 300
            tryVerify(() => image.stableDecodeSize.width > firstWidth)
            tryCompare(image, "ready", true)
            compare(image.rememberedIntrinsicSize, intrinsic)
            const large = Qt.size(image.stableDecodeSize.width, image.stableDecodeSize.height)
            image.width = 100
            image.height = 100
            wait(350)
            compare(image.stableDecodeSize, large)
            fuzzyCompare(image.paintedHeight, 50, 1)
            image.activeSource = ""
            wait(20)
            compare(image.rememberedIntrinsicSize, intrinsic)
            fuzzyCompare(image.paintedHeight, 50, 1)
            image.synchronizeActiveSource()
            tryCompare(image, "ready", true)
            compare(image.rememberedIntrinsicSize, intrinsic)
            verify(image.stableDecodeSize.width < large.width)
        }
        function test_newIdentityAndCrop() {
            image.fillMode = Image.PreserveAspectCrop
            fuzzyCompare(image.paintedWidth, 480, 2)
            compare(image.paintedHeight, 240)
            image.source = Qt.resolvedUrl("portrait.png")
            tryCompare(image, "ready", true)
            tryVerify(() => Math.abs(image.intrinsicAspect - 0.5) < 0.02)
            fuzzyCompare(image.paintedHeight, 480, 2)
            compare(image.paintedWidth, 240)
            image.fillMode = Image.PreserveAspectFit
            fuzzyCompare(image.paintedWidth, 120, 1)
            compare(image.paintedHeight, 240)
        }
    }
}
"""
def png(path, w, h):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind+data))
    pixels = (b"\0" + bytes((30, 120, 200, 255))*w)*h
    path.write_bytes(b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",w,h,8,6,0,0,0))
                     +chunk(b"IDAT",zlib.compress(pixels))+chunk(b"IEND",b""))
def main():
    runner = os.environ.get("QML_TEST_RUNNER") or shutil.which("qmltestrunner")
    if not runner:
        raise SystemExit("Set QML_TEST_RUNNER to the Qt qmltestrunner path")
    with tempfile.TemporaryDirectory(prefix="genycaster-shared-raster-") as tmp:
        path=Path(tmp)
        shutil.copyfile(IMAGE,path/"SuperellipseImage.qml")
        shutil.copyfile(ROOT/"ui/components/media/MediaResourceScope.qml",path/"MediaResourceScope.qml")
        module=path/"Farcaster/Client"
        module.mkdir(parents=True)
        (module/"qmldir").write_text("module Farcaster.Client\nsingleton Theme 1.0 Theme.qml\n")
        props=set(re.findall(r"\bTheme\.([A-Za-z]\w*)",IMAGE.read_text()))
        lines=["pragma Singleton","import QtQuick","QtObject {"]
        for prop in sorted(props):
            if prop in ("iconGlyph","iconFamily"):
                lines.append(f'function {prop}(value) {{ return "Sans Serif" }}')
            elif prop=="iconUsesRegular":
                lines.append("function iconUsesRegular(value) { return true }")
            elif prop=="uiDirection":
                lines.append("property int uiDirection: Qt.LeftToRight")
            elif prop in ("reduceMotion","powerSave","animateAvatars"):
                lines.append(f"property bool {prop}: false")
            elif prop.startswith("icon") and prop!="iconSizeSmall":
                lines.append(f'property string {prop}: "x"')
            elif prop in ("ink","accent","textTertiary") or prop.startswith(("surface","border")):
                lines.append(f'property color {prop}: "#777777"')
            elif prop=="weightDemiBold":
                lines.append("property int weightDemiBold: Font.DemiBold")
            else:
                lines.append(f"property real {prop}: "+("4" if prop=="superellipseExponent" else "12"))
        lines.append("}")
        (module/"Theme.qml").write_text("\n".join(lines))
        stubs={
            "Superellipse":'Rectangle { property color fillColor: "transparent"; property real borderWidth: 0; property color borderColor: "transparent"; property real curveDepth: 0; property real exponent: 4; property real elevation: 0; color: fillColor }',
            "GenySpinner":'Item { property bool running: false; property real strokeWidth: 1; property color color: "white" }',
            "Icon":'Item { property string text: ""; property color color: "white"; property real preferredSize: 24 }',
            "DirectionAwareLabel":"Label {}",
        }
        for name,body in stubs.items():
            (path/(name+".qml")).write_text("import QtQuick\nimport QtQuick.Controls\n"+body)
        png(path/"landscape.png",800,400)
        png(path/"portrait.png",400,800)
        (path/"tst_raster.qml").write_text(TEST)
        result=subprocess.run([runner,"-import",tmp,"-input",tmp],env=dict(os.environ,QT_QPA_PLATFORM="offscreen",QT_QUICK_BACKEND="software"),
                              text=True,capture_output=True,timeout=20)
        output=result.stdout+result.stderr
        print(output)
        if result.returncode or any(x in output for x in ("Binding loop detected","TypeError:","ReferenceError:")):
            raise SystemExit(1)
if __name__=="__main__":
    main()
