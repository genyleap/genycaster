#!/usr/bin/env python3
"""Exercise production panel/dispatcher lifecycle with isolated leaf renderers."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
QML="""import QtQuick
import QtTest
Item {
    width: 640; height: 600
    Component {
        id: panelComponent
        CastEmbedsPanel {
            width: 600
            supportingEmbeds: [{kind:"video",url:"https://example.com/video.mp4"}]
        }
    }
    TestCase {
        name: "SupportingPreviewLifecycle"
        when: windowShown
        function test_detail() {
            const panel = createTemporaryObject(panelComponent, parent, {asynchronousTiles:false})
            verify(panel)
            const outer = findChild(panel,"supportingPreviewLoader")
            verify(outer)
            compare(outer.asynchronous,false)
            tryCompare(outer,"status",Loader.Ready)
            const inner = findChild(outer.item,"embedRendererLoader")
            verify(inner)
            compare(inner.asynchronous,false)
            compare(inner.status,Loader.Ready)
            compare(outer.item.resolvedKind,"video")
        }
        function test_timeline() {
            const panel = createTemporaryObject(panelComponent,parent,{visible:false})
            verify(panel)
            const outer = findChild(panel,"supportingPreviewLoader")
            verify(outer)
            compare(outer.asynchronous,true)
            wait(100)
            compare(outer.active,false)
            panel.visible=true
            tryCompare(outer,"status",Loader.Ready,3000)
            const preview=outer.item
            const inner=findChild(preview,"embedRendererLoader")
            verify(inner)
            compare(inner.asynchronous,false)
            compare(inner.status,Loader.Ready)
            panel.visible=false
            wait(100)
            compare(outer.item,preview)
            panel.supportingEmbeds=[{kind:"video",url:"https://example.com/another.mp4"}]
            tryCompare(preview,"targetUrl","https://example.com/another.mp4")
            compare(outer.item,preview)
            panel.visible=true
            compare(outer.item,preview)
            panel.supportingEmbeds=[{kind:"image",url:"https://example.com/image.png"}]
            tryCompare(preview,"resolvedKind","image")
            tryCompare(inner,"status",Loader.Ready)
            compare(inner.item.asynchronousTiles,false)
        }
        function test_rapidNavigation() {
            for(let i=0;i<25;++i) {
                const panel=panelComponent.createObject(parent)
                verify(panel)
                wait(i%2===0?5:25)
                panel.destroy()
                wait(1)
            }
            wait(200)
        }
    }
}
"""
LEAF="""import QtQuick
Item {
    implicitHeight: 100
    property var embed: ({})
    property bool compact: false
    property bool magazineCard: false
    property bool scrollMoving: false
    property bool enrichmentActive: false
    property string locator: ""
    property string targetUrl: ""
    property var embeds: []
    property bool mobilePresentation: false
    property color cornerCoverColor: "black"
    property bool asynchronousTiles: true
    property var sharedMediaGeometry: null
    property bool lockFeedGeometry: false
    property bool dynamicAspectDiscovery: false
    property real singleImageHeight: 0
    property real shadowTail: 0
    property real preferredMediaHeight: 100
    property bool embeddedInCard: false
    property string castCaption: ""
    property bool suppressArtwork: false
    property bool metadataManagedExternally: false
    property var externalPreview: ({})
    signal openRequested(string url)
    signal mediaRequested(var items,int index)
    signal castRequested(string castId)
}
"""
GALLERY=LEAF.replace("signal openRequested(string url)","signal openRequested(var items,int index)")
def main():
    runner=os.environ.get("QML_TEST_RUNNER") or shutil.which("qmltestrunner")
    if not runner: raise SystemExit("Set QML_TEST_RUNNER")
    with tempfile.TemporaryDirectory(prefix="genycaster-preview-") as tmp:
        path=Path(tmp)
        module=path/"Farcaster/Client";module.mkdir(parents=True)
        (module/"qmldir").write_text("module Farcaster.Client\nsingleton Theme 1.0 Theme.qml\nsingleton TokenSemantic 1.0 TokenSemantic.qml\n")
        (module/"Theme.qml").write_text('pragma Singleton\nimport QtQuick\nQtObject { property int space2: 8; property int uiDirection: Qt.LeftToRight; property color surfaceRaised: "white"; property color backgroundPrimary: "white" }')
        (module/"TokenSemantic.qml").write_text('pragma Singleton\nimport QtQuick\nQtObject { function locatorFromEmbed(embed) { return "" } }')
        for name in ("CastEmbedsPanel","EmbedPreview"):
            shutil.copyfile(ROOT/"ui/components/media"/(name+".qml"),path/(name+".qml"))
        for name in ("EmbedCard","TokenAddressPreview","CollectiblePreview","VideoPreview","AudioPreview","FilePreview","LinkPreview","GalleryLayout"):
            (path/(name+".qml")).write_text(GALLERY if name=="GalleryLayout" else LEAF)
        (path/"tst_preview.qml").write_text(QML)
        env=dict(os.environ,QT_QPA_PLATFORM=os.environ.get("QT_QPA_PLATFORM","offscreen"),QT_QUICK_BACKEND=os.environ.get("QT_QUICK_BACKEND","software"))
        p=subprocess.run([runner,"-import",tmp,"-input",tmp],env=env,text=True,capture_output=True,timeout=30)
        output=p.stdout+p.stderr
        print(output)
        if p.returncode or any(x in output for x in ("during incubation","Binding loop detected","ReferenceError:","TypeError:","items in the process")):
            raise SystemExit(1)
if __name__=="__main__":
    main()
