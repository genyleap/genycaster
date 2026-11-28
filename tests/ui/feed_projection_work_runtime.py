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
    property bool socialMode: false
    property bool topicFilterActive: false
    property string presentationMode: "visuals"
    property string activeFeed: "following"
    property string signerFilterMode: "all"
    property string appFilterMode: "all"
    property string selectedSignerKey: ""
    property string selectedAppFid: ""
    property bool presentationActive: true
    property bool stableModelSyncPending: false
    property bool previewSyncPending: false
    property int projectionSyncs: 0
    function synchronizeStableModel() { ++projectionSyncs }
    QtObject { id: readList; property bool moving: false }
    Timer { id: previewSyncDebounce; interval: 10000 }
    property var nativeRowSnapshot: null
    property var nativeRowOrderProvider: null
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
        property int calls: 0
        function previewFor(url) {
            ++calls
            return {contentType: observed}
        }
    }
    TestCase {
        name: "FeedProjectionWorkBudget"
        when: windowShown
        function test_staticClassifierMemoization() {
            mediaClassifier.calls = 0
            root.staticMediaFamilies.clear()
            linkPreviewResolver.observed = "image"
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
        function test_fullProjectionConsumesPendingMetadataSync() {
            root.presentationActive = true
            root.stableModelSyncPending = false
            root.previewSyncPending = true
            root.projectionSyncs = 0
            readList.moving = true
            previewSyncDebounce.start()
            root.requestStableModelSync()
            compare(root.projectionSyncs,0)
            verify(root.stableModelSyncPending)
            verify(root.previewSyncPending)
            verify(previewSyncDebounce.running)
            readList.moving = false
            root.presentationActive = false
            root.requestStableModelSync()
            compare(root.projectionSyncs,0)
            verify(root.previewSyncPending)
            root.presentationActive = true
            root.requestStableModelSync()
            compare(root.projectionSyncs,1)
            verify(!root.stableModelSyncPending)
            verify(!root.previewSyncPending)
            verify(!previewSyncDebounce.running)
        }
        function test_imageEvidenceSkipsUnneededNativeClassification() {
            mediaClassifier.calls = 0
            root.staticMediaFamilies.clear()
            linkPreviewResolver.observed = "web"
            verify(root.rowHasImage({embeds:[{kind:"image",url:"https://example.invalid/a"}]}))
            verify(root.rowHasImage({embeds:[{kind:"url",url:"https://example.invalid/b.jpg"}]}))
            verify(root.rowHasImage({embeds:[{kind:"url",url:"https://example.invalid/c",previewImage:"poster"}]}))
            verify(root.rowHasImage({embeds:[{kind:"url",url:"https://imagedelivery.net/hash/id/public"}]}))
            compare(mediaClassifier.calls,0)
            verify(!root.rowHasImage({embeds:[{kind:"url",url:"https://example.invalid/d"}]}))
            compare(mediaClassifier.calls,1)
            linkPreviewResolver.observed = "image"
            verify(root.rowHasImage({embeds:[{kind:"url",url:"https://example.invalid/d"}]}))
            compare(mediaClassifier.calls,2)
        }
        function test_collectibleProjectionReadsEachUrlOnce() {
            root.presentationMode = "visuals"
            root.activeFeed = "trending"
            root.topicFilterActive = false
            const a = "https://example.invalid/piece-a"
            const b = "https://example.invalid/piece-b"
            root.rows = [
                {castId:"one",text:"My new painting",embeds:[{kind:"url",url:a}]},
                {castId:"two",text:"Original art",embeds:[{kind:"url",url:a}]},
                {castId:"three",text:"Original art",embeds:[{kind:"url",url:b}]},
                {castId:"four",text:"Ordinary post",embeds:[{kind:"url",url:b}]}
            ]
            linkPreviewResolver.observed = "image"
            linkPreviewResolver.calls = 0
            compare(root.sourceRows().map(r=>r.payload.castId).join(","),"one,two,three")
            compare(linkPreviewResolver.calls,2)
            // Resolver updates must invalidate the projection-local snapshot.
            linkPreviewResolver.observed = "web"
            compare(root.sourceRows().length,0)
            compare(linkPreviewResolver.calls,4)
            root.presentationMode = "for-you"
            compare(root.sourceRows().length,4)
            compare(linkPreviewResolver.calls,4)
            root.presentationMode = "visuals"
        }
        function test_profileRankingLargeTies() {
            const source=[]
            for(let i=0;i<5000;++i) source.push({id:i,createdAtMs:i%47,likeCount:i%3})
            root.rows=source
            root.profileFeed="user-popular"
            root.popularMetric="likes"
            const sorted=root.sortedRows()
            compare(sorted.length,source.length)
            for(let i=1;i<sorted.length;++i) {
                const a=sorted[i-1], b=sorted[i]
                verify(a.likeCount>b.likeCount || a.likeCount===b.likeCount
                    && (a.createdAtMs>b.createdAtMs || a.createdAtMs===b.createdAtMs && a.id<b.id))
            }
        }
        function test_profileNativeSnapshotFallback() {
            root.rows=[{id:"fallback"}]
            root.nativeRowSnapshot={token:"a",rows:[{id:"a",createdAtMs:1},{id:"b",createdAtMs:2}]}
            root.nativeRowOrderProvider=function(token,feed,mode,metric) {
                compare(token,"a")
                return [1,0]
            }
            compare(root.sortedRows().map(r=>r.id).join(","),"b,a")
            root.nativeRowOrderProvider=function(){return []}
            root.profileFeed="user-casts"
            root.profileSortMode="oldest"
            compare(root.sortedRows().map(r=>r.id).join(","),"a,b")
            root.nativeRowSnapshot=null
            root.nativeRowOrderProvider=null
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
    functions=section(feed,"    function embedUrl(","    function incrementFilterPrefetchPages(")
    functions+=section(feed,"    function rowMatchesAppFilter(","    function rowKey(")
    functions+=section(feed,"    function requestStableModelSync(","    function cancelViewportCorrections(")
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
