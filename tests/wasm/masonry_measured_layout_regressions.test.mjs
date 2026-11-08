import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';

const read = p => fs.readFileSync(p, 'utf8');
const wall = read('ui/components/feed/FeedMasonryView.qml');

function section(start, end) {
    const at = wall.indexOf(start);
    const to = wall.indexOf(end, at);
    assert.ok(at >= 0 && to > at, 'QML method block exists: ' + start);
    return wall.slice(at, to);
}

test('a resolved magazine card replaces an oversized reservation with its actual height', () => {
    const code = section('            function recordHeight() {',
                         '\n            Loader {\n                id: visiblePlaceholder');
    const sample = [];
    const context = vm.createContext({
        cardLoader: {item: {height: 850, implicitHeight: 230}},
        measuredHeight: 850, stableKey: 'cast:1', revisionKey: 'r:1', width: 346,
        view: {retainHeight: (...args) => sample.push(args)}
    });
    vm.runInContext(code.trim(), context);
    vm.runInContext('recordHeight()', context);
    assert.equal(context.measuredHeight, 230,
        'Loader reservation cannot indefinitely keep a card 850px high after content resolves to 230px');
    assert.deepEqual(sample[0], ['cast:1', 'r:1', 346, 230]);
    context.cardLoader.item = {height: 250, implicitHeight: 0};
    vm.runInContext('recordHeight()', context);
    assert.equal(context.measuredHeight, 250, 'geometry-only component has a height fallback');
    context.cardLoader.item = null;
    vm.runInContext('recordHeight()', context);
    assert.equal(context.measuredHeight, 250, 'destroying a pooled delegate never resets its confirmed height');
    assert.match(wall, /function onImplicitHeightChanged\(\) \{ slot\.recordHeight\(\) \}/);
});

test('late image dimensions only move subsequent cards vertically in the same lane', () => {
    const code = section('    function forceLayout() {',
                         '\n    function positionViewAtBeginning()');
    const heights = [480, 260, 310, 180, 220, 400, 120, 200, 145];
    const elements = heights.map(height => ({
        height, assignedColumn: -1, assignmentColumns: 0, x: 0, y: 0, placed: false
    }));
    const root = {
        arranging: false, positioned: false, moving: false, flicking: false,
        relayoutPending: false, columns: 3, layoutDirection: 0, leadingInset: 12,
        cardWidth: 310, gutter: 12, width: 978, height: 680,
        contentY: 0, contentHeight: 2000, rowsBottom: 0,
        slots: {count: elements.length, itemAt: i => elements[i]},
        headerLoader: {height: 0},
        layoutTimer: {stop() {}},
        layoutStarted() {}, layoutFinished() {},
        scheduleAdmission() {}, scheduleLayout() {},
        indexAt() {return -1;},
        Qt: {RightToLeft: 1}
    };
    const context = vm.createContext(root);
    vm.runInContext(code.trim(), context);
    vm.runInContext('forceLayout()', context);
    const originalColumns = elements.map(slot => slot.assignedColumn);
    const originalX = elements.map(slot => slot.x);

    // A portrait whose placeholder was much taller is decoded. The new
    // height must close that hole, with no horizontal shuffle or overlap.
    elements[0].height = 185;
    elements[1].height = 380;
    vm.runInContext('forceLayout()', context);
    assert.deepEqual(elements.map(slot => slot.assignedColumn), originalColumns);
    assert.deepEqual(elements.map(slot => slot.x), originalX);
    const lastBottom = Array(3).fill(12);
    for (const slot of elements) {
        assert.equal(slot.y, lastBottom[slot.assignedColumn],
            'each slot begins exactly at previous card bottom plus gutter');
        lastBottom[slot.assignedColumn] += slot.height + 12;
    }
    assert.equal(context.rowsBottom, Math.max(...lastBottom));
    assert.match(wall, /clip: cardLoader\.item === null\s+onHeightChanged: view\.scheduleLayout\(\)/);
    assert.doesNotMatch(code, /const repack = !moving && !flicking/,
        'idle scroll must never repack cards sideways on media completion');
});

test('shared view/filter controls follow the compact rounded reference in Explorer and Profile', () => {
    const controls = read('ui/components/feed/ExplorerLayoutControls.qml');
    const explorer = read('ui/components/home/HomeFeedHeader.qml');
    const profile = read('ui/components/profile/ProfileSurface.qml');
    const translations = ['en_US', 'fa_IR', 'ar_SA', 'de_DE', 'es_ES',
        'fr_FR', 'hi_IN', 'ja_JP', 'ko_KR', 'pt_BR', 'ru_RU', 'zh_CN'];

    assert.match(controls, /Superellipse \{\s*id: segments/);
    assert.match(controls, /objectName: "magazineViewSegments"/);
    assert.match(controls, /customSelectedSurface: Theme\.surfaceRaised/);
    assert.match(controls, /objectName: "magazineToolbarFilter"/);
    assert.match(controls, /onClicked: root\.filterRequested\(\)/);
    assert.match(explorer, /filterAvailable: root\.filterAvailable/);
    assert.match(explorer, /onFilterRequested: root\.filterRequested\(\)/);
    assert.match(explorer, /visible: root\.compact && root\.filterAvailable/);
    assert.match(profile, /filterAvailable: true/);
    assert.match(profile, /onFilterRequested: profileFilterSheet\.open\(\)/);
    for (const language of translations) {
        const catalog = read('ui/i18n/qml_' + language + '.ts');
        assert.match(catalog, /id="genycaster\.filters\.explorer"/);
        assert.match(catalog, /id="genycaster\.filters\.profile"/);
    }
});

test('Profile Magazine estimates dimensions using its actual narrow in-card geometry', () => {
    const profile = read('ui/components/profile/ProfileSurface.qml');
    const fn = profile.slice(profile.indexOf('    function rowIncubationHeight('),
                             profile.indexOf('    // The profile reuses the same native stable-ID', profile.indexOf('    function rowIncubationHeight(')));
    assert.match(fn, /const compactCard = root\.compact \|\| root\.profileMagazineEnabled/);
    assert.match(fn, /Math\.max\(compactCard \? 260 : 480, Number\(availableWidth \|\| 0\)\)/);
    assert.match(fn, /const horizontalInset = compactCard \? Theme\.mobileGutter \* 2/);
    assert.doesNotMatch(fn, /const rowWidth = Math\.max\(root\.compact \? 320 : 480/);
});

test('magazine relative timestamp and the overflow action share one horizontal axis', () => {
    const cast = read('ui/components/feed/CastItem.qml');
    const authorEnd = cast.indexOf('id: castAuthorIdentity');
    const time = cast.indexOf('objectName: "magazineCastTime"', authorEnd);
    const more = cast.indexOf('id: moreButton', authorEnd);
    assert.ok(authorEnd >= 0 && time > authorEnd && more > time);
    assert.match(cast.slice(time, more), /Layout\.alignment: Qt\.AlignVCenter/);
    assert.match(cast.slice(more, more + 180),
        /Layout\.alignment: root\.magazineCard \? Qt\.AlignVCenter : Qt\.AlignTop/);
    assert.match(cast.slice(time, more), /Layout\.maximumWidth: Math\.min\(104, root\.width \* 0\.29\)/);
});
