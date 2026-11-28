#!/usr/bin/env python3
"""Run the real GalleryLayout QML with deterministic, delayed media metadata."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
GALLERY = ROOT / "ui/components/media/GalleryLayout.qml"
WALL = (ROOT / "ui/components/feed/FeedMasonryView.qml").read_text()
cache = WALL[WALL.index("    property var aspectMemory:"):WALL.index("    function heightKey(")]
THEME = """pragma Singleton
import QtQuick
QtObject {
    property real space2: 8
    property real space5: 20
    property color background: "#eeeeee"
    property color surface: "#dddddd"
}
"""
TILE = """import QtQuick
Item {
    property var embed: ({})
    property bool asynchronousAmbient: true
    property bool preserveAspect: false
    property bool useLayeredMask: false
    property color cornerCoverColor: "transparent"
    property bool ambientEnabled: false
    property int loadDelayMs: 0
    property string continuationText: ""
    signal openRequested(string url)
    signal aspectRatioResolved(real ratio)
}
"""
TEST = """import QtQuick
import QtTest
Item {
    width: 960
    height: 650
    property QtObject renderingController: QtObject { property bool shuttingDown: false }
    QtObject {
        id: memory
""" + cache + """
    }
    Flickable {
        id: reader
        anchors.fill: parent
        contentHeight: 4000
        Item {
            id: shown
            objectName: "masonrySlot"
            property string stableKey: "cast:shown"
            function mayRefineMediaGeometry() { return false }
            width: 320
            height: 500
        }
        Item {
            id: ahead
            objectName: "masonrySlot"
            property string stableKey: "cast:ahead"
            property bool canRefine: true
            function mayRefineMediaGeometry() { return canRefine }
            y: 900
            width: 320
            height: 700
        }
        Item {
            id: timeline
            objectName: "timelineRow"
            property string stableKey: "cast:timeline"
            y: 2600
            width: 320
            height: 700
        }
        Item {
            id: nextCast
            objectName: "masonrySlot"
            property string stableKey: "cast:next"
            function mayRefineMediaGeometry() { return false }
            y: 1800
            width: 320
            height: 700
        }
    }
    Component {
        id: gallery
        GalleryLayout {
            width: 320
            compact: true
            lockFeedGeometry: true
            dynamicAspectDiscovery: true
            sharedMediaGeometry: memory
        }
    }
    TestCase {
        name: "StableMediaGeometry"
        when: windowShown
        function make(owner, url, aspect) {
            const embed = {kind:"image",url:url}
            if (aspect) embed.aspectRatio = aspect
            const item = gallery.createObject(owner, {embeds:[embed]})
            verify(item !== null)
            tryCompare(item, "geometryInitialized", true)
            compare(item.geometryOwner, owner)
            return item
        }
        function test_exposedDecodeAndReverseScroll() {
            const url = "https://example.invalid/portrait.jpg"
            let item = make(shown, url, 0)
            compare(item.planHeight, 240)
            item.rememberAspect(0, 0.5)
            wait(250)
            compare(item.planHeight, 240)
            compare(memory.aspectForKey(url), 0.5)
            item.rememberAspect(0, 2.0)
            compare(item.planHeight, 240)
            compare(memory.aspectForKey(url), 0.5)
            // Same URL with later metadata still retains its original slot.
            item.embeds = [{kind:"image",url:url,width:300,height:600}]
            compare(item.planHeight, 240)
            item.destroy()
            wait(30)
            item = make(shown, url, 0)
            compare(item.planHeight, 240)
            item.destroy()
            wait(30)
            // A different cast may use the known intrinsic ratio immediately.
            item = make(nextCast, url, 0)
            compare(item.planHeight, 640)
            item.destroy()
        }
        function test_unseenPreAdmissionRefinement() {
            const url = "https://example.invalid/ahead.jpg"
            let item = make(ahead, url, 0)
            compare(item.planHeight, 240)
            item.rememberAspect(0, 0.5)
            compare(item.planHeight, 640)
            ahead.canRefine = false
            item.rememberAspect(0, 1.5)
            compare(item.planHeight, 640)
            item.destroy()
            wait(30)
            item = make(ahead, url, 0)
            compare(item.planHeight, 640)
            item.destroy()
        }
        function test_desktopTimelineReuseAndLateMetadata() {
            const url = "https://example.invalid/timeline.jpg"
            timeline.stableKey = "cast:timeline"
            let item = make(timeline, url, 0)
            item.dynamicAspectDiscovery = false
            const initialHeight = item.planHeight
            item.rememberAspect(0, 0.5)
            compare(item.planHeight, initialHeight)
            item.embeds = [{kind:"image",url:url,width:300,height:600}]
            compare(item.planHeight, initialHeight)
            item.destroy()
            wait(30)
            item = make(timeline, url, 0)
            item.dynamicAspectDiscovery = false
            compare(item.planHeight, initialHeight)
            timeline.stableKey = "cast:timeline-other"
            verify(item.planHeight > initialHeight)
            item.destroy()
        }
        function test_hiddenAdmissionAndReuse() {
            shown.visible = false
            const item = make(shown, "https://example.invalid/hidden.jpg", 2)
            const loader = findChild(item,"galleryTileLoader")
            verify(loader)
            const reserved = item.planHeight
            wait(120)
            compare(loader.active,false)
            shown.visible = true
            tryCompare(loader,"status",Loader.Ready,2000)
            const rendered = loader.item
            shown.visible = false
            wait(120)
            compare(loader.item,rendered)
            compare(item.planHeight,reserved)
            shown.visible = true
            compare(loader.item,rendered)
            item.destroy()
        }
        function test_shutdownBlocksGalleryAdmission() {
            shown.visible = false
            renderingController.shuttingDown = false
            const item = make(shown, "https://example.invalid/quit.jpg", 2)
            const loader = findChild(item,"galleryTileLoader")
            verify(loader)
            compare(loader.active,false)
            shown.visible = true
            renderingController.shuttingDown = true
            wait(120)
            compare(loader.active,false)
            shown.visible = false
            shown.visible = true
            wait(120)
            compare(loader.active,false)
            item.destroy()
            renderingController.shuttingDown = false
        }
        function test_declaredMetadataAndResponsiveWidth() {
            ahead.canRefine = true
            const item = make(ahead, "https://example.invalid/known.jpg", 2)
            compare(item.planHeight, 160)
            item.rememberAspect(0, 0.5)
            compare(item.planHeight, 160)
            item.width = 400
            compare(item.planHeight, 200)
            item.width = 320
            compare(item.planHeight, 160)
            item.destroy()
        }
    }
}
"""
def main():
    runner = os.environ.get("QML_TEST_RUNNER") or shutil.which("qmltestrunner")
    if not runner:
        raise SystemExit("Set QML_TEST_RUNNER to your installed Qt qmltestrunner")
    with tempfile.TemporaryDirectory(prefix="genycaster-stable-gallery-") as tmp:
        path = Path(tmp)
        module = path / "Farcaster/Client"
        module.mkdir(parents=True)
        (module / "qmldir").write_text("module Farcaster.Client\nsingleton Theme 1.0 Theme.qml\n")
        (module / "Theme.qml").write_text(THEME)
        (path / "GalleryTile.qml").write_text(TILE)
        shutil.copyfile(GALLERY, path / "GalleryLayout.qml")
        shutil.copyfile(ROOT / "ui/components/composer/StickerCatalog.js", path / "StickerCatalog.js")
        (path / "tst_stable_gallery.qml").write_text(TEST)
        env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software")
        result = subprocess.run([runner, "-import", tmp, "-input", tmp], env=env, text=True,
                                capture_output=True, timeout=20)
        print(result.stdout + result.stderr)
        if result.returncode or "Binding loop detected" in result.stdout + result.stderr:
            raise SystemExit(1)

if __name__ == "__main__":
    main()
