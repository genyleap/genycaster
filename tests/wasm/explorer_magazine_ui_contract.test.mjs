import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import vm from "node:vm";

const read = path => fs.readFileSync(path, "utf8");

test("Explorer Magazine has one compact responsive card size and keeps Home unchanged", () => {
    const surface = read("ui/components/feed/FeedSurface.qml");
    const shell = read("ui/components/shell/AppShell.qml");
    const masonry = read("ui/components/feed/FeedMasonryView.qml");
    assert.match(surface, /targetCardWidth: Theme\.scaleUi\(420\)/);
    assert.match(surface, /maximumColumns: 3/);
    assert.match(surface, /readonly property bool magazineEnabled: explorerLayoutAvailable && !compact\s+&& !socialMode/);
    assert.doesNotMatch(surface, /property int explorerCardWidth/);
    assert.doesNotMatch(shell, /explorerCardWidth|property alias cardWidth/);
    assert.match(shell, /explorerLayoutAvailable: root\.primaryDestination === "explorer"/);
    assert.match(shell, /explorerLayout: root\.explorerDisplayLayout/);
    assert.match(masonry, /readonly property int columns: Math\.max\(1, Math\.min\(maximumColumns/);
});

test("windowed Explorer fills the reading pane with two columns instead of a centered card", () => {
    const qml = read("ui/components/feed/FeedMasonryView.qml");
    // Execute the actual QML geometry expressions, not a duplicate formula
    // in the test. This catches regressions in cutoff and unused outside space.
    const minimum = qml.match(/readonly property real minimumCardWidth: ([^\n]+)/)?.[1];
    const columnFormula = qml.match(/readonly property int columns: ([\s\S]*?)\n    \/\/ Allocate/)?.[1];
    const widthFormula = qml.match(/readonly property real cardWidth: ([\s\S]*?)\n    readonly property real leadingInset:/)?.[1];
    const insetFormula = qml.match(/readonly property real leadingInset: ([^\n]+)/)?.[1];
    assert.ok(minimum && columnFormula && widthFormula && insetFormula);

    const evaluate = (width, targetCardWidth = 420) => {
        const values = {width, targetCardWidth, gutter: 12, maximumColumns: 3};
        const context = vm.createContext(values);
        for (const [property, expr] of [
            ["minimumCardWidth", minimum],
            ["columns", columnFormula],
            ["cardWidth", widthFormula],
            ["leadingInset", insetFormula]
        ]) {
            context[property] = vm.runInContext(expr, context);
        }
        return context;
    };

    for (const [viewportWidth, expectedColumns] of [
        [590, 1],
        [740, 2],
        [960, 2],
        [1040, 3],
        [1340, 3]
    ]) {
        const actual = evaluate(viewportWidth);
        assert.equal(actual.columns, expectedColumns, `${viewportWidth}dp column count`);
        assert.ok(actual.cardWidth > 0, "cards have a positive width");
        assert.ok(actual.cardWidth >= actual.minimumCardWidth || expectedColumns === 1);
        const occupied = 2 * actual.leadingInset
            + actual.columns * actual.cardWidth
            + (actual.columns - 1) * actual.gutter;
        assert.ok(Math.abs(viewportWidth - occupied) < 0.01,
            `No unused horizontal slabs at ${viewportWidth}dp`);
    }
    const scaled = evaluate(740, 357);
    assert.equal(scaled.columns, 2, "column count respects UI scale");
});

test("Magazine / Timeline view actions sit next to Everyone / Following, without size filter", () => {
    const shell = read("ui/components/shell/AppShell.qml");
    const header = read("ui/components/home/HomeFeedHeader.qml");
    const controls = read("ui/components/feed/ExplorerLayoutControls.qml");
    assert.match(controls, /^import QtQuick\.Layouts$/m);
    assert.match(controls, /RowLayout \{\s*id: choices/);
    assert.match(controls, /objectName: "explorerMagazineChoice"/);
    assert.match(controls, /objectName: "explorerTimelineChoice"/);
    assert.doesNotMatch(controls, /GenyComboBox|explorerCardSizeChoice|cardWidthRequested|small_cards|medium_cards|large_cards/);
    const audience = header.indexOf('id: audienceSelector');
    const choices = header.indexOf("ExplorerLayoutControls {");
    assert.ok(choices >= 0 && choices < audience, "View choices precede audience selector in same row");
    assert.match(header, /signal layoutRequested\(string mode\)/);
    assert.match(header, /visible: !root\.compact/);
    assert.match(shell, /explorerLayout: root\.explorerDisplayLayout/);
    assert.match(shell, /onLayoutRequested: mode => root\.explorerDisplayLayout = mode/);
    assert.equal((shell.match(/ExplorerLayoutControls \{/g) || []).length, 0,
        "No separate view-controls toolbar above the Explorer header");
});

test("Masonry viewport admission has no Loader.active geometry binding or loop", () => {
    const source = read("ui/components/feed/FeedMasonryView.qml");
    const match = source.match(/    function updateAdmissions\(\) \{([\s\S]*?)\n    \}\n\n    function forceLayout\(\)/);
    assert.ok(match, "Viewport admission logic must be present");
    assert.match(source, /active: slot\.admitted/);
    assert.doesNotMatch(source, /active: view\.visible && view\.positioned && slot\.placed/);
    assert.match(source, /onContentYChanged: scheduleAdmission\(\)/);
    assert.match(source, /onVisibleChanged: scheduleAdmission\(\)/);
    assert.match(source, /function scheduleAdmission\(\)/);
    assert.match(source, /onTriggered: view\.updateAdmissions\(\)/);
    assert.match(source, /readonly property real retentionBuffer: cacheBuffer \+/);
    const elements = [
        {y: 0, height: 140, admitted: false, placed: true},
        {y: 320, height: 140, admitted: false, placed: true},
        {y: 800, height: 140, admitted: false, placed: true}
    ];
    const vars = {
        visible: true, positioned: true, height: 400, contentY: 0,
        cacheBuffer: 100, retentionBuffer: 240,
        slots: {count: elements.length, itemAt: i => elements[i]}
    };
    const context = vm.createContext(vars);
    vm.runInContext("function updateAdmissions() {" + match[1] + "\n}", context);
    const advance = (y, visible = true) => {
        context.contentY = y;
        context.visible = visible;
        vm.runInContext("updateAdmissions()", context);
        return elements.map(slot => slot.admitted);
    };
    assert.deepEqual(advance(0), [true, true, false], "Only near-viewport cards are admitted");
    assert.deepEqual(advance(650), [false, true, true], "Retention buffer prevents thrashing at boundary");
    assert.deepEqual(advance(650, false), [false, false, false], "Hidden Explorer releases its cards");
    assert.deepEqual(advance(0), [true, true, false], "Magazine activates again when reopened");
});

test("Magazine avoids partial asynchronous cast incubation without disabling timeline virtualization", () => {
    const surface = read("ui/components/feed/FeedSurface.qml");
    const cast = read("ui/components/feed/CastItem.qml");
    const wall = read("ui/components/feed/FeedMasonryView.qml");
    assert.match(surface, /asynchronous: !sectionRow && !magazineRow/);
    assert.equal((cast.match(/asynchronous: !root\.magazineCard/g) || []).length, 4);
    assert.match(wall, /asynchronous: false\s*\n\s*sourceComponent: view\.delegate/);
    assert.match(wall, /function updateAdmissions\(\)/);
    assert.doesNotMatch(wall, /active: .*slot\.height/);
});
