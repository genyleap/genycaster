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
                         '\n            Component.onCompleted: offerEstimate()');
    const sample = [];
    const context = vm.createContext({
        cardLoader: {item: {height: 850, implicitHeight: 230}},
        measuredHeight: 850, cardHeight: 850, revisionSettling: false,
        stableKey: 'cast:1', revisionKey: 'r:1', width: 346,
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

test('the magazine uses a viewport-bounded native layout model instead of one QML Item per cast', () => {
    const code = section('    function forceLayout() {',
                         '    function positionViewAtBeginning()');
    const native = read('ui/app/social/feed_rows.cpp');
    const header = read('ui/app/social/feed_rows.hpp');
    assert.match(wall, /MasonryWindowModel \{/);
    assert.match(wall, /Repeater \{[\s\S]*?model: layoutModel/);
    assert.match(wall, /layoutModel\.setViewport\(contentY, height, cacheBuffer, retentionBuffer/);
    assert.doesNotMatch(wall, /model: view\.model\s*\n\s*delegate:/,
        'never build an offscreen QQuickItem for every source row');
    assert.match(code, /layoutModel\.relayout\(/);
    assert.match(code, /layoutModel\.geometryForKey\(anchor\.stableKey\)/);
    assert.doesNotMatch(code, /slots\.count/, 'per-frame layout does not walk visual delegates');
    assert.match(native, /std::lower_bound\(lane\.cbegin\(\), lane\.cend\(\)/);
    assert.match(native, /beginInsertRows\(\{\}, index, index\)/);
    assert.match(native, /beginRemoveRows\(\{\}, index, m_window\.size\(\) - 1\)/);
    assert.match(header, /class MasonryWindowModel : public QAbstractListModel/);
    // The real C++ layout geometry and model signal invariants are exercised
    // by ui.feed_rows_model; this source guard covers QML integration.
});

test('shared view/filter controls follow the compact rounded reference in Explorer and Profile', () => {
    const controls = read('ui/components/feed/ExplorerLayoutControls.qml');
    const explorer = read('ui/components/home/HomeFeedHeader.qml');
    const profile = read('ui/components/profile/ProfileSurface.qml');
    const translations = ['en_US', 'fa_IR', 'ar_SA', 'de_DE', 'es_ES',
        'fr_FR', 'hi_IN', 'ja_JP', 'ko_KR', 'pt_BR', 'ru_RU', 'zh_CN'];

    assert.match(controls, /Item \{\s*id: segments/);
    assert.match(controls, /objectName: "magazineViewSegments"/);
    assert.match(controls, /GenyTabButton \{/);
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
