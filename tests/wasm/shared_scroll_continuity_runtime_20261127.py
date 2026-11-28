#!/usr/bin/env python3
"""Qt Quick regression: real shared image admission and aspect transactions.

Tests the production QML lifecycle bindings on native Qt and with the WASM
viewport policy substituted. No live account or remote image contents needed.
"""
from pathlib import Path
import os
import subprocess
import tempfile

PROJECT = Path(__file__).resolve().parents[2]
image = (PROJECT / "ui/components/visual/SuperellipseImage.qml").read_text()
scope = (PROJECT / "ui/components/media/MediaResourceScope.qml").read_text().replace("import Farcaster.Client\n", "")
layout = (PROJECT / "ui/components/media/GalleryLayout.qml").read_text()
start = image.index("    MediaResourceScope {\n        id: rasterScope")
end = image.index("    // Keep intrinsic dimensions", start)
lifecycle = image[start:end].replace('Qt.platform.os === "wasm"', 'root.browserPolicy')
start = layout.index("    function rememberAspect(")
end = layout.index("    function boundedHeight(", start)
aspect = layout[start:end]
fixture = """import QtQuick
import QtTest
Item {
    id: harness
    width: 390
    height: 600
    property bool browserPolicy: true
    Flickable {
        id: viewport
        anchors.fill: parent
        clip: true
        contentHeight: 5000
        Item {
            id: card
            width: 300
            height: 180
            y: 1000
            Item {
                id: root
                anchors.fill: parent
                property bool browserPolicy: harness.browserPolicy
                property bool remoteRaster: true
                property bool componentReady: true
                property string activeSource: ""
                property int admissions: 0
                property int evictions: 0
                function invalidatePresentationAdmission() { evictions += 1 }
                Timer {
                    id: sourceStartTimer
                    interval: 10
                    onTriggered: {
                        if (root.rasterEligible && !root.activeSource.length) {
                            root.admissions += 1
                            root.activeSource = "https://example.invalid/one.jpg"
                        }
                    }
                }
""" + lifecycle + """
            }
        }
    }
    Item {
        id: geometryFixture
        // Production methods refer to root. Supply their own object context
        // in a separate component, not the raster fixture's root id.
        component Geometry: Item {
            id: root
            property bool dynamicAspectDiscovery: true
            property bool lockFeedGeometry: false
            property var decodedOnce: Object.create(null)
            property var discoveredAspects: ({})
            property var pendingAspects: ({})
            property bool moving: false
            property int sharedWrites: 0
            property real sharedAspect: 0
            property var sharedMediaGeometry: ({
                rememberAspect: function(key, ratio) {
                    root.sharedWrites += 1
                    root.sharedAspect = ratio
                }
            })
            function mediaKey(index) { return "media-" + index }
            function scrollingAncestor() { return root }
""" + aspect + """
        }
        Geometry { id: aspectModel }
    }
    TestCase {
        name: "CrossPlatformScrollContinuity"
        when: windowShown
        function test_retention_data() {
            return [{tag:"wasm", browser:true, outsideAdmission:440, outsideRetention:900},
                    {tag:"native", browser:false, outsideAdmission:640, outsideRetention:1200}]
        }
        function test_retention(data) {
            harness.browserPolicy = data.browser
            viewport.contentY = 900
            card.visible = true
            tryVerify(() => root.rasterEligible)
            tryVerify(() => root.activeSource.length > 0)
            const admitted = root.admissions
            const evicted = root.evictions
            // Leave admission, but not retention. Hold longer than the old
            // 300 ms timer, then reverse. No network/decode restart allowed.
            viewport.contentY = card.y + card.height + data.outsideAdmission
            tryCompare(root, "rasterEligible", false)
            compare(root.rasterRetained, true)
            wait(500)
            verify(root.activeSource.length > 0)
            viewport.contentY = 900
            tryCompare(root, "rasterEligible", true)
            compare(root.admissions, admitted)
            compare(root.evictions, evicted)
            // A transient excursion beyond retention also keeps the texture.
            viewport.contentY = card.y + card.height + data.outsideRetention
            tryCompare(root, "rasterRetained", false)
            wait(400)
            viewport.contentY = 900
            wait(850)
            compare(root.admissions, admitted)
            compare(root.evictions, evicted)
            // Sustained distant scroll or a hidden page actually frees it.
            viewport.contentY = card.y + card.height + data.outsideRetention
            tryCompare(root, "activeSource", "", 1800)
            compare(root.evictions, evicted + 1)
            viewport.contentY = 900
            tryVerify(() => root.activeSource.length > 0)
            card.visible = false
            tryCompare(root, "activeSource", "", 700)
            card.visible = true
            tryVerify(() => root.activeSource.length > 0)
        }
        function test_geometryCommitsOnlyAtRest() {
            aspectModel.moving = true
            aspectModel.rememberAspect(0, 0.42)
            wait(250)
            compare(aspectModel.sharedWrites, 0)
            compare(Object.keys(aspectModel.discoveredAspects).length, 0)
            compare(aspectModel.pendingAspects["media-0"], 0.42)
            aspectModel.moving = false
            tryCompare(aspectModel, "sharedWrites", 1)
            compare(aspectModel.discoveredAspects["media-0"], 0.42)
            compare(aspectModel.sharedAspect, 0.42)
        }
    }
}
"""
def main():
    runner = os.environ.get("QML_TEST_RUNNER", "/opt/genycaster/Qt/6.12.0/gcc_64/bin/qmltestrunner")
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software")
    with tempfile.TemporaryDirectory(prefix="genycaster-scroll-qml-") as tmp:
        path = Path(tmp)
        (path / "MediaResourceScope.qml").write_text(scope)
        (path / "tst_scroll.qml").write_text(fixture)
        run = subprocess.run([runner, "-input", tmp], env=env, cwd=PROJECT,
                             text=True, capture_output=True, timeout=25)
    output = run.stdout + run.stderr
    print(output)
    if run.returncode or "Binding loop detected" in output:
        raise SystemExit(1)

if __name__ == "__main__":
    main()
