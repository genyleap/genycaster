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
    const usableFormula = qml.match(/readonly property real usableWidth: ([^\n]+)/)?.[1];
    const columnFormula = qml.match(/readonly property int columns: ([\s\S]*?)\n    readonly property real cardWidth:/)?.[1];
    const widthFormula = qml.match(/readonly property real cardWidth: ([\s\S]*?)\n    readonly property real leadingInset:/)?.[1];
    const insetFormula = qml.match(/readonly property real leadingInset: ([^\n]+)/)?.[1];
    assert.ok(minimum && usableFormula && columnFormula && widthFormula && insetFormula);

    const evaluate = (width, targetCardWidth = 420) => {
        const values = {width, targetCardWidth, gutter: 12, outsideInset: 12, maximumColumns: 3};
        const context = vm.createContext(values);
        for (const [property, expr] of [
            ["minimumCardWidth", minimum],
            ["usableWidth", usableFormula],
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
    const match = source.match(/    function updateAdmissions\(\) \{([\s\S]*?)\n    \}\n    function scheduleRebalance\(\)/);
    assert.ok(match, "Viewport admission logic must be present");
    assert.match(source, /active: slot\.admitted/);
    assert.doesNotMatch(source, /active: view\.visible && view\.positioned && slot\.placed/);
    assert.match(source, /onContentYChanged: scheduleAdmission\(\)/);
    assert.match(source, /onVisibleChanged: \{[\s\S]*?layoutModel\.setViewport\(contentY, height, cacheBuffer, retentionBuffer, false, false\)/);
    assert.match(source, /function scheduleAdmission\(\)/);
    assert.match(source, /onTriggered: view\.updateAdmissions\(\)/);
    assert.match(source, /readonly property real retentionBuffer: cacheBuffer \+/);
    // Viewport admission is now a native QAbstractItemModel responsibility.
    // Execute the production QML handoff and verify inactive readers release
    // the window; native Qt runtime tests cover which slots are retained.
    const calls = [];
    const context = vm.createContext({
        contentY: 0, height: 400, cacheBuffer: 100, retentionBuffer: 240,
        view: {visible: true, resizeInProgress: false}, positioned: true,
        arranging: false, layoutTimer: {running: false},
        layoutModel: {setViewport(...args) {calls.push(args)}}
    });
    vm.runInContext("function updateAdmissions() {" + match[1] + "\n}", context);
    vm.runInContext("updateAdmissions()", context);
    assert.deepEqual([...calls.at(-1)], [0, 400, 100, 240, true, false]);
    context.contentY = 650;
    vm.runInContext("updateAdmissions()", context);
    assert.deepEqual([...calls.at(-1)], [650, 400, 100, 240, true, false]);
    context.view.visible = false;
    vm.runInContext("updateAdmissions()", context);
    assert.deepEqual([...calls.at(-1)], [650, 400, 100, 240, false, false]);
    context.view.visible = true;
    vm.runInContext("updateAdmissions()", context);
    assert.deepEqual([...calls.at(-1)], [650, 400, 100, 240, true, false]);
    context.view.resizeInProgress = true;
    vm.runInContext("updateAdmissions()", context);
    assert.deepEqual([...calls.at(-1)], [650, 400, 100, 240, true, true]);
    const before = calls.length;
    context.layoutTimer.running = true;
    vm.runInContext("updateAdmissions()", context);
    assert.equal(calls.length, before, "never admit against an invalidated layout");
    const native = read("ui/app/social/feed_rows.cpp");
    assert.match(native, /void MasonryWindowModel::synchronizeWindow\(\)/);
    assert.match(native, /std::lower_bound\(lane\.cbegin\(\), lane\.cend\(\), minimum/);
    assert.match(native, /\|\| retained\.contains\(slot\.key\)/);
});

test("Magazine incubates only admitted casts asynchronously without disabling timeline virtualization", () => {
    const surface = read("ui/components/feed/FeedSurface.qml");
    const cast = read("ui/components/feed/CastItem.qml");
    const wall = read("ui/components/feed/FeedMasonryView.qml");
    assert.match(surface, /asynchronous: !sectionRow && !magazineRow/);
    assert.equal((cast.match(/asynchronous: !root\.magazineCard/g) || []).length, 3);
    // Rich text builds synchronously inside the already virtualized row,
    // preventing Qt from abandoning nested text incubators on scroll.
    assert.match(cast, /id: castTextBody[\s\S]*?asynchronous: false/);
    assert.match(wall, /active: slot\.admitted[\s\S]*?opacity: slot\.cardReady \? 1 : 0[\s\S]*?asynchronous: true\s*sourceComponent: view\.delegate/);
    assert.match(wall, /function updateAdmissions\(\)/);
    assert.doesNotMatch(wall, /active: .*slot\.height/);
});


test("windowed Explorer keeps view actions at leading edge and collapses crowded labels", () => {
    const header = read("ui/components/home/HomeFeedHeader.qml");
    const controls = read("ui/components/feed/ExplorerLayoutControls.qml");
    const cast = read("ui/components/feed/CastItem.qml");
    const row = header.split("        RowLayout {\n            Layout.fillWidth: true\n            Layout.topMargin: Theme.space1")[1]
        .split("\n        RowLayout {")[0];
    assert.ok(row, "Feed toolbar row exists");
    assert.ok(row.indexOf("ExplorerLayoutControls {") < row.indexOf('text: root.compact ? qsTrId')
        && row.indexOf('text: root.compact ? qsTrId') < row.indexOf("id: audienceSelector"),
        "View buttons sit at the leading edge, and Everyone/Following remain at trailing edge");
    assert.match(header, /readonly property bool condensedFeedActions: !root\.compact && root\.width < 690/);
    assert.match(header, /Layout\.preferredWidth: segmentWidth \* 2 \+ segmentInset \* 2 \+ segmentGap/);
    assert.match(header, /Layout\.preferredHeight: root\.compact \? Math\.max\(44, Theme\.compactControlHeight\)\s*: Theme\.compactControlHeight/);
    assert.match(controls, /property bool condensed: false/);
    assert.equal((controls.match(/iconOnly: root\.condensed/g) || []).length, 3);
    assert.match(cast, /objectName: "magazineCastTime"/);
    assert.match(cast, /visible: root\.compact && !root\.magazineCard/);
    assert.match(cast, /visible: root\.magazineCard\s+&& Number\(root\.modelData\.createdAtMs/);
    assert.match(cast, /root\.magazineCard \? Math\.max\(76, root\.width \* 0\.53\)/);
    assert.match(cast, /!\s*root\.magazineCard \|\| root\.width >= 390/);
});

test("semantic tabs leave skeleton state after a bounded deadline and offer retry", () => {
    const surface = read("ui/components/feed/FeedSurface.qml");
    const empty = read("ui/components/controls/EmptyState.qml");
    assert.match(surface, /property bool semanticWaitExpired: false/);
    assert.match(surface, /readonly property bool waitingForFirstRows: root\.runtimeReady\s+&& !root\.semanticWaitExpired/);
    assert.match(surface, /id: semanticLoadDeadline\s+interval: 12000\s+repeat: false\s+onTriggered: root\.semanticWaitExpired = true/);
    assert.match(surface, /onPresentationModeChanged: \{\s*root\.restartSemanticDeadline\(\)/);
    assert.match(surface, /onActiveFeedChanged: \{\s*root\.restartSemanticDeadline\(\)/);
    assert.match(surface, /onRuntimeReadyChanged: \{\s*if \(root\.runtimeReady\) root\.restartSemanticDeadline\(\)/);
    assert.match(surface, /visible: stableRows\.count === 0 && root\.waitingForFirstRows/);
    assert.match(surface, /visible: stableRows\.count === 0 && !root\.waitingForFirstRows/);
    assert.match(surface, /timedOut: root\.semanticWaitExpired && stableRows\.count === 0/);
    assert.match(empty, /property bool timedOut: false/);
    assert.match(empty, /root\.sourceUnavailable \|\| root\.timedOut/);
    assert.match(empty, /onClicked: root\.retryRequested\(\)/);

    // Test the actual deadline predicate, not a duplicated hand-written rule.
    const excerpt = surface.match(/readonly property bool waitingForFirstRows: ([\s\S]*?)\n    Timer \{/);
    assert.ok(excerpt);
    const predicate = excerpt[1].trim();
    const calculate = (expired, loading = true) => {
        const ctx = vm.createContext({
            root: {
                runtimeReady: true, semanticWaitExpired: expired, busy: loading,
                supplementalBusy: false, selectionSettling: false,
                needsFilteredPrefetch: () => false
            },
            filterPrefetchDebounce: {running: false}
        });
        return vm.runInContext(predicate, ctx);
    };
    assert.equal(calculate(false), true, "live first-page loading uses placeholders");
    assert.equal(calculate(true), false, "expired request no longer hides empty/retry state");
    assert.equal(calculate(false, false), false, "settled empty feed shows empty state");
});

test("Magazine nested EmbedPreview and GalleryLayout never abandon incubation", () => {
    const cast = read("ui/components/feed/CastItem.qml");
    const panel = read("ui/components/media/CastEmbedsPanel.qml");
    const rich = read("ui/components/content/RichCastRenderer.qml");
    const preview = read("ui/components/media/EmbedPreview.qml");
    const gallery = read("ui/components/media/GalleryLayout.qml");
    assert.match(cast, /magazineCard: root\.magazineCard\s+sharedMediaGeometry: root\.sharedMediaGeometry\s+suppressedEmbedUrls:/);
    assert.match(panel, /asynchronousTiles: root\.asynchronousTiles && !root\.magazineCard/);
    assert.match(panel, /property bool asynchronousTiles: true/);
    assert.match(panel, /magazineCard: root\.magazineCard\s+sharedMediaGeometry: root\.sharedMediaGeometry\s+flatLinkCard:/);
    assert.match(rich, /magazineCard: root\.magazineCard/);
    assert.match(preview, /asynchronous: !root\.magazineCard/);
    assert.match(preview, /asynchronousTiles: !root\.magazineCard/);
    assert.match(gallery, /property bool asynchronousTiles: true/);
    assert.match(gallery, /asynchronous: root\.asynchronousTiles/);
});
