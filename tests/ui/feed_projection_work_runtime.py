#!/usr/bin/env python3
"""Execute the production projection functions in Qt's actual QML JS engine."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FEED = ROOT / "ui/components/feed/FeedSurface.qml"
PROFILE = ROOT / "ui/components/profile/ProfileSurface.qml"

def section(source, start, end):
    first = source.index(start)
    return source[first:source.index(end, first)]

QML = """import QtQuick
import QtTest
Item {
    id: root
    width: 320; height: 240
    property var rows: []
    property string profileFeed: "user-popular"
    property string profileSortMode: "newest"
    property string popularMetric: "likes"
    readonly property var staticMediaFamilies: new Map()
    // PRODUCTION_FUNCTIONS
    QtObject {
        id: renderingController
        property bool stallDiagnosticsEnabled: false
        property int clockCalls: 0
        property var operations: []
        function diagnosticClockMs() { ++clockCalls; return 123 }
        function recordUiOperation(operation, startMs, rows) {
            operations.push([operation, startMs, rows])
        }
    }
    QtObject {
        id: mediaClassifier
        property int calls: 0
        function classify(embed, metadata) {
            ++calls
            return {family: metadata.contentType || metadata.providerKind ||
                            embed.contentType || embed.kind || "web"}
        }
    }
    QtObject {
        id: linkPreviewResolver
        property string observed: "image"
        function previewFor(url) { return {contentType: observed} }
    }
    TestCase {
        name: "FeedProjectionWorkBudget"
        when: windowShown
        function test_staticClassifierMemoization() {
            const embed = {kind:"url", url:"https://example.invalid/a"}
            compare(root.classifiedFamily(embed,{}),"url")
            compare(root.classifiedFamily({kind:"url",url:embed.url},{}),"url")
            compare(mediaClassifier.calls,1)
            compare(root.classifiedFamily(embed,{contentType:"video"}),"video")
            compare(root.classifiedFamily(embed),"image")
            linkPreviewResolver.observed="audio"
            compare(root.classifiedFamily(embed),"audio")
            compare(root.classifiedFamily(embed,{}),"url")
            compare(mediaClassifier.calls,4)
            compare(root.classifiedFamily({kind:"url",url:embed.url,contentType:"image"},{}),"image")
            compare(mediaClassifier.calls,5)
            for(let i=0;i<2100;++i)
                root.classifiedFamily({kind:"url",url:"https://example.invalid/"+i},{})
            compare(root.staticMediaFamilies.size,2048)
            const before=mediaClassifier.calls
            compare(root.classifiedFamily(embed,{}),"url")
            compare(mediaClassifier.calls,before+1)
            compare(root.staticMediaFamilies.size,2048)
            root.staticMediaFamilies.clear()
            for(let i=0;i<2100;++i)
                root.classifiedFamily({kind:"url",url:"https://example.invalid/scan/"+i},{},false)
            const firstScan=mediaClassifier.calls
            for(let i=0;i<2100;++i)
                root.classifiedFamily({kind:"url",url:"https://example.invalid/scan/"+i},{},false)
            compare(mediaClassifier.calls-firstScan,52)
            compare(root.staticMediaFamilies.size,2048)
        }
        function test_profileRanking() {
            root.rows = [{id:"a",likeCount:1,createdAtMs:10},
                         {id:"b",likeCount:2,createdAtMs:5},
                         {id:"c",likeCount:2,createdAtMs:20},
                         {id:"d",likeCount:2,createdAtMs:20}]
            root.profileFeed="user-popular"
            compare(root.sortedRows().map(r=>r.id).join(","),"c,d,b,a")
            root.profileFeed="user-casts"
            root.profileSortMode="oldest"
            compare(root.sortedRows().map(r=>r.id).join(","),"b,a,c,d")
            root.profileSortMode="newest"
            compare(root.sortedRows().map(r=>r.id).join(","),"c,d,a,b")
            compare(root.rows.map(r=>r.id).join(","),"a,b,c,d")
            compare(renderingController.clockCalls,0)
            compare(renderingController.operations.length,0)
            renderingController.stallDiagnosticsEnabled=true
            compare(root.sortedRows().map(r=>r.id).join(","),"c,d,a,b")
            compare(renderingController.clockCalls,1)
            compare(renderingController.operations.length,1)
            compare(renderingController.operations[0].join(","),"profile.sort,123,4")
            renderingController.stallDiagnosticsEnabled=false
        }
    }
}
"""

def main():
    runner=os.environ.get("QML_TEST_RUNNER") or shutil.which("qmltestrunner")
    if not runner:
        raise SystemExit("Set QML_TEST_RUNNER")
    feed=FEED.read_text()
    profile=PROFILE.read_text()
    functions=section(feed,"    function embedUrl(","    function isCollectibleMarketplaceUrl(")
    functions+=section(feed,"    function staticMediaFamilyKey(","    function looksLikeArtworkUrl(")
    functions+=section(profile,"    function rowTime(","    function rowKey(")
    with tempfile.TemporaryDirectory(prefix="genycaster-projection-") as tmp:
        (Path(tmp)/"tst_projection.qml").write_text(QML.replace("    // PRODUCTION_FUNCTIONS",functions))
        env=dict(os.environ,QT_QPA_PLATFORM="offscreen",QT_QUICK_BACKEND="software")
        p=subprocess.run([runner,"-input",tmp],env=env,text=True,capture_output=True,timeout=30)
        output=p.stdout+p.stderr
        print(output)
        if p.returncode or any(x in output for x in ("Binding loop detected","ReferenceError:","TypeError:","during incubation","items in the process")):
            raise SystemExit(1)
if __name__=="__main__":
    main()
