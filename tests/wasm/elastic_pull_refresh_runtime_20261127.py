from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'ui/components/feed/PullToRefresh.qml').read_text()
# Preserve actual production gesture behavior; isolate only decorative skin.
source=source[:source.index('    Superellipse {')]+'}\n'
source=source.replace('import Farcaster.Client\n','')
fixture="""import QtQuick
import QtTest
Item {
    id: root
    width: 320; height: 480
    property int refreshes: 0
    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: 1600
        boundsBehavior: Flickable.DragOverBounds
        boundsMovement: Flickable.FollowBoundsBehavior
        acceptedButtons: Qt.LeftButton
        Rectangle { width: 320; height: 1600; color: "white" }
    }
    PullToRefresh {
        id: refresh
        flickable: flick
        onRefreshRequested: root.refreshes += 1
    }
    TestCase {
        name: "ElasticPullRefresh"
        when: windowShown
        function test_shortDragDoesNotRefresh() {
            flick.contentY = flick.originY
            const before = root.refreshes
            mousePress(flick,150,40)
            mouseMove(flick,150,72,50,Qt.LeftButton)
            mouseRelease(flick,150,72)
            tryVerify(() => Math.abs(flick.contentY - flick.originY) < 0.5,2500)
            compare(root.refreshes,before)
        }
        function test_topDrag() {
            flick.contentY = flick.originY
            mousePress(flick, 150, 40)
            for (let y=70; y<=400; y+=30) {
                mouseMove(flick,150,y,25,Qt.LeftButton)
                verify(flick.contentY <= flick.originY)
            }
            verify(flick.contentY < flick.originY - refresh.activationDistance)
            verify(refresh.visible)
            verify(refresh.y >= 0 && refresh.y + refresh.height <= refresh.offset)
            verify(refresh.armed, "overshoot " + flick.verticalOvershoot + " distance " + refresh.pulledDistance)
            mouseRelease(flick,150,400)
            tryCompare(root,"refreshes",1)
            tryVerify(() => Math.abs(flick.contentY - flick.originY) < 0.5, 2500)
            compare(root.refreshes,1)
        }
    }
}
"""
with tempfile.TemporaryDirectory(prefix='genycaster-pull-test-') as d:
 p=Path(d);(p/'PullToRefresh.qml').write_text(source);(p/'tst_pull.qml').write_text(fixture)
 r=subprocess.run([os.environ.get('QML_TEST_RUNNER','/opt/genycaster/Qt/6.12.0/gcc_64/bin/qmltestrunner'),'-input',d],env=dict(os.environ,QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software'),capture_output=True,text=True,timeout=15)
 print(r.stdout+r.stderr)
 if r.returncode:raise SystemExit(r.returncode)
