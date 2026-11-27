#!/usr/bin/env python3
"""Exercise the actual compact GridView viewport binding and shared media admission.
Qt Quick, without an application build or live account. Emulates WASM cache tuning.
"""
from pathlib import Path
import os
import subprocess
import tempfile

PROJECT = Path(__file__).resolve().parents[2]
apps = (PROJECT / "ui/components/apps/AppsSurface.qml").read_text()
start = apps.index("                                // Compact mode has one outer scroll owner.")
end = apps.index("                                // Two scannable desktop columns", start)
grid = apps[start:end].replace('Qt.platform.os === "wasm" ? 160 : Math.min(height, 480)', "160")
scope = (PROJECT / "ui/components/media/MediaResourceScope.qml").read_text().replace("import Farcaster.Client\n", "")
fixture = """import QtQuick
import QtTest
Item {
    id: root
    width: 390
    height: 720
    property bool compact: true
    property var visibleApps: Array.from({length: 500}, (_, i) => i)
    property int liveDelegates: 0
    property int peakDelegates: 0
    property int headerHeight: 330
    Flickable {
        id: appsScroller
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: pageColumn.height
        Item {
            id: pageColumn
            width: appsScroller.width
            height: root.headerHeight + contentViewport.height + 40
            Item {
                y: 30
                width: parent.width
                height: parent.height - 30
                Item {
                    id: contentViewport
                    y: root.headerHeight - 30
                    width: parent.width
                    height: Math.max(1, Math.ceil(root.visibleApps.length / appGrid.targetColumns)
                                      * appGrid.cellHeight)
                    GridView {
                        id: appGrid
""" + grid + """
                        readonly property int targetColumns: 1
                        cellWidth: width
                        cellHeight: 148
                        delegate: Item {
                            required property int index
                            width: appGrid.cellWidth
                            height: appGrid.cellHeight
                            Component.onCompleted: {
                                root.liveDelegates += 1
                                root.peakDelegates = Math.max(root.peakDelegates, root.liveDelegates)
                            }
                            Component.onDestruction: root.liveDelegates -= 1
                        }
                    }
                }
            }
        }
    }
    Flickable {
        id: rasterScroller
        x: 0; y: 0; width: 200; height: 200
        contentHeight: 1000
        Item {
            id: rasterParent
            y: 380; width: 100; height: 100
            Item {
                id: raster
                width: 100; height: 100
                visible: false // self-hidden until Ready
            }
        }
        MediaResourceScope {
            id: rasterScope
            owner: raster
            viewportMargin: 240
            requireOwnerVisible: false
        }
        MediaResourceScope {
            id: strictScope
            owner: raster
        }
    }
    TestCase {
        name: "WasmViewportAndRasterAdmission"
        when: windowShown
        function test_grid500Rows() {
            tryCompare(appGrid, "count", 500)
            verify(appGrid.height <= appsScroller.height)
            compare(contentViewport.height, 74000)
            for (const index of [0, 9, 120, 250, 490, 30, 499, 0]) {
                appsScroller.contentY = Math.min(appsScroller.contentHeight - appsScroller.height,
                                                 root.headerHeight + index * 148)
                wait(180)
                verify(Math.abs(appGrid.contentY - appGrid.originY - appGrid.y) < 1)
                const item = appGrid.itemAtIndex(index)
                verify(item !== null, "row missing: " + index)
                const mapped = item.mapToItem(appsScroller, 0, 0)
                verify(mapped.y < appsScroller.height && mapped.y + item.height > 0,
                       "row outside viewport: " + index + " at " + mapped.y)
                verify(root.liveDelegates <= 16, "too many live delegates: " + root.liveDelegates)
            }
            appsScroller.contentY = 330 + 100 * 148
            wait(180)
            root.visibleApps = root.visibleApps.concat([500,501,502])
            tryCompare(appGrid, "count", 503)
            wait(180)
            verify(appGrid.itemAtIndex(100) !== null)
            verify(root.liveDelegates <= 16)
            root.headerHeight += 100
            wait(180)
            verify(Math.abs(appGrid.contentY - appGrid.originY - appGrid.y) < 1)
            console.log("BOUNDED_GRID_PASS", root.liveDelegates, root.peakDelegates)
        }
        function test_rasterVisibilityAndMargin() {
            verify(rasterScope.eligible)
            verify(!strictScope.eligible)
            rasterScroller.contentY = 800
            tryCompare(rasterScope, "eligible", false)
            rasterScroller.contentY = 380
            tryCompare(rasterScope, "eligible", true)
            rasterParent.visible = false
            tryCompare(rasterScope, "eligible", false)
            rasterParent.visible = true
            tryCompare(rasterScope, "eligible", true)
            raster.visible = true
            tryCompare(strictScope, "eligible", true)
            rasterScroller.contentY = 0
            tryCompare(strictScope, "eligible", false)
            tryCompare(rasterScope, "eligible", true)
        }
    }
}
"""

def main():
    runner = os.environ.get("QML_TEST_RUNNER", "/opt/genycaster/Qt/6.12.0/gcc_64/bin/qmltestrunner")
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software")
    with tempfile.TemporaryDirectory(prefix="genycaster-memory-qml-") as tmp:
        path = Path(tmp)
        (path / "MediaResourceScope.qml").write_text(scope)
        (path / "tst_viewport.qml").write_text(fixture)
        run = subprocess.run([runner, "-input", tmp], env=env, cwd=PROJECT,
                             text=True, capture_output=True, timeout=25)
    output = run.stdout + run.stderr
    print(output)
    if run.returncode or "BOUNDED_GRID_PASS" not in output or "Binding loop detected" in output:
        raise SystemExit(1)

if __name__ == "__main__":
    main()
