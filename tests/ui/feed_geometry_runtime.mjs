// Runs the production FeedRow with a real Qt ListView/Loader and deterministic
// lightweight cast content. Uses an installed qmltestrunner; never builds C++.
import { readFileSync, writeFileSync, mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { spawnSync } from 'node:child_process';
import assert from 'node:assert/strict';

const runner = process.argv[2];
assert.ok(runner, 'Pass the installed Qt qmltestrunner path');
const source = readFileSync(new URL('../../ui/components/feed/FeedSurface.qml', import.meta.url), 'utf8');
const start = source.indexOf('    component FeedRow: Loader {');
const end = source.indexOf('\n\n    Component {\n        id: feedFooterComponent', start);
assert.ok(start >= 0 && end > start);
const feedRow = source.slice(start, end);
const directory = mkdtempSync(join(tmpdir(), 'genycaster-feed-geometry-'));
try {
    writeFileSync(join(directory, 'qmldir'), 'singleton Theme 1.0 Theme.qml\n');
    writeFileSync(join(directory, 'Theme.qml'), `pragma Singleton
import QtQuick
QtObject {
    property int space4: 16
    property int space3: 12
    property int mobileGutter: 16
    property color textPrimary: "black"
    property int typeTitle3Size: 18
    property int weightDemiBold: Font.DemiBold
}`);
    writeFileSync(join(directory, 'CastSkeleton.qml'), `import QtQuick
Item { property bool mediaPreview: false; property bool animate: false; implicitHeight: 120 }`);
    writeFileSync(join(directory, 'DirectionAwareLabel.qml'), 'import QtQuick\nText {}');
    writeFileSync(join(directory, 'ProfileListItem.qml'), `import QtQuick
Item { property var modelData; implicitHeight: 72; signal openRequested(real fid) }`);
    writeFileSync(join(directory, 'SemanticFeedItem.qml'), `import QtQuick
Item {
    property var modelData: ({})
    property int rowIndex: 0
    property string presentationKind: "conversation"
    property var sharedMediaGeometry: null
    property bool magazineCard: false
    property bool compact: false
    property bool interactionsAvailable: false
    property bool enrichmentActive: false
    property real accountFid: 0
    implicitHeight: Number(modelData.measuredHeight || 72)
    Rectangle { anchors.fill: parent; color: rowIndex % 2 ? "lightblue" : "beige" }
    signal openRequested(string castId)
    signal profileRequested(real fid)
    signal replyRequested(var cast)
    signal quoteRequested(var cast)
    signal reactionRequested(string kind, var cast, bool active)
    signal removeRequested(var cast)
    signal externalRequested(string url)
    signal mediaRequested(var mediaItems, int index)
    signal authenticationRequested()
}`);
    writeFileSync(join(directory, 'tst_FeedGeometry.qml'), `import QtQuick
import QtTest
Item {
    id: root
    width: 420; height: 600
    property bool compact: false
    property bool socialMode: false
    property bool presentationActive: true
    property bool interactionsAvailable: false
    property real accountFid: 0
    property string geometryAnchorKey: ""
    property var payloads: ({})
    function payloadForStableKey(key) { return payloads[key] || ({}) }
    function rowIncubationHeight() { return 600 }
    function rememberVisibleGeometry() {}
    function sectionTitle(key) { return key }
    Timer { id: geometryRestoreTimer }
    ListModel { id: stableRows }
    ListView {
        id: readList
        width: 420; height: 600
        model: stableRows
        reuseItems: true
        cacheBuffer: 100
        delegate: FeedRow {}
${feedRow}
    }
    TestCase {
        name: "ProductionFeedRowGeometry"
        when: windowShown
        function initTestCase() {
            for (let i = 0; i < 120; ++i) {
                root.payloads["cast:" + i] = { castId: String(i), measuredHeight: 90 + i % 4 * 30 }
                stableRows.append({stableKey: "cast:" + i, revisionKey: "initial",
                    presentationKind: "conversation", mediaLayoutKind: "other"})
            }
            tryCompare(readList, "count", 120)
            tryVerify(function() { const row = readList.itemAtIndex(0); return row && !row.incubating })
        }
        function cleanup() { readList.cancelFlick() }
        function test_measured_height_replaces_reservation_during_flick() {
            readList.positionViewAtIndex(40, ListView.Beginning)
            tryVerify(function() { const row = readList.itemAtIndex(40); return row && !row.incubating })
            const row = readList.itemAtIndex(40)
            readList.flick(0, -900)
            tryCompare(readList, "moving", true)
            const measured = row.resolvedHeight
            row.committedHeight = measured + 240
            row.synchronizeHeight()
            compare(row.height, measured, "ready content must release the oversized placeholder while moving")
        }
        function test_late_height_growth_is_not_clipped_or_overlapped() {
            readList.positionViewAtIndex(60, ListView.Beginning)
            tryVerify(function() { const row = readList.itemAtIndex(60); return row && !row.incubating })
            const row = readList.itemAtIndex(60)
            readList.flick(0, -500)
            tryCompare(readList, "moving", true)
            root.payloads["cast:60"] = {castId: "60", measuredHeight: 410}
            stableRows.setProperty(60, "revisionKey", "hydrated")
            tryCompare(row, "resolvedHeight", 410)
            compare(row.height, 410, "outer row and loaded content share one measured height")
        }
        function test_reused_rows_match_current_identity_and_geometry() {
            const observed = []
            let reused = false
            for (const index of [0, 8, 16, 24, 32, 40, 48, 32, 16, 0]) {
                readList.positionViewAtIndex(index, ListView.Beginning)
                tryVerify(function() { const row = readList.itemAtIndex(index); return row && !row.incubating })
                const row = readList.itemAtIndex(index)
                for (const previous of observed) {
                    if (previous.row === row && previous.key !== row.stableKey) reused = true
                }
                observed.push({row: row, key: row.stableKey})
                compare(row.stableKey, "cast:" + index)
                compare(row.item.modelData.castId, String(index))
                compare(row.visible, true)
                compare(row.height, row.resolvedHeight)
                verify(row.height > 0)
                wait(25)
            }
            verify(reused, "fixture must exercise the actual ListView reuse pool")
        }
    }
}`);
    const result = spawnSync(runner, ['-input', directory], {
        encoding: 'utf8', timeout: 30_000,
        env: { ...process.env, QT_QPA_PLATFORM: 'minimal', QT_QUICK_BACKEND: 'software',
            QT_QUICK_CONTROLS_STYLE: 'Basic', QT_FATAL_WARNINGS: '1', QML_DISABLE_DISK_CACHE: '1' },
    });
    process.stdout.write(result.stdout || '');
    process.stderr.write(result.stderr || '');
    if (result.error) throw result.error;
    assert.equal(result.status, 0, 'production row geometry runtime regressions (signal: ' + result.signal + ')');
} finally {
    rmSync(directory, { recursive: true, force: true });
}
