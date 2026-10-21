import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import test from 'node:test';

const read = file => readFileSync(new URL('../../ui/components/' + file + '.qml', import.meta.url), 'utf8');

test('Home fills the lower workspace with real, actionable feed previews', () => {
    const home = read('HomeDashboard');
    assert.match(home, /id: homeContinuation/);
    assert.match(home, /visible: !root.composerFocused/);
    assert.match(home, /model: root.recentCasts/);
    assert.match(home, /objectName: "homeRecentCastPreview"/);
    assert.match(home, /onClicked: root.castRequested\(String\(modelData.castId\)\)/);
    assert.match(home, /visible: root.recentCasts.length === 0[\s\S]*?onTapped: root.exploreRequested\(\)/);
    assert.match(home, /contentHeight: mobileColumn.implicitHeight \+ Theme.space3/);
    assert.doesNotMatch(home, /Math.max\(108, Theme.touchTarget/);
});

test('Home preview model ignores missing and repeated cast IDs, never inventing activity', () => {
    const home = read('HomeDashboard');
    const start = home.indexOf('readonly property var recentCasts: {');
    const end = home.indexOf('property int localHour:', start);
    assert.ok(start >= 0 && end > start);
    const body = home.slice(start, end);
    const expression = body.replace('readonly property var recentCasts:', '').trim();
    const calculate = rows => vm.runInNewContext('(() => ' + expression + ')()', {root: {rows}});
    const rows = [
        {castId: '', text: 'not a cast'},
        {castId: 'c1', displayName: 'Alice', text: 'First\ncast'},
        {castId: 'c1', displayName: 'Duplicated'},
        {castId: 'c2', author: 'Bob', text: 'Second'},
        {castId: 'c3', author: 'Charlie', text: 'Third'}
    ];
    const previews = calculate(rows);
    assert.equal(previews.length, 2);
    assert.equal(previews[0].castId, 'c1');
    assert.equal(previews[0].name, 'Alice');
    assert.equal(previews[0].text, 'First cast');
    assert.equal(previews[1].castId, 'c2');
    assert.equal(calculate([]).length, 0);
});

test('cast details use their own back toolbar instead of a duplicate global brand bar', () => {
    const shell = read('AppShell');
    const detail = read('DetailSurface');
    assert.match(shell, /AppHeader \{\s+\/\/ Cast details[\s\S]*?visible: root.mobile && \(root.shellRoute.length > 0[\s\S]*?presentationBridge.activeView !== "cast"\)/);
    assert.match(detail, /genycaster.detail.back_back_to_feed/);
    assert.match(detail, /onClicked: root.backRequested\(\)/);
    assert.match(detail, /Layout.topMargin: Theme.space1[\s\S]*?spacing: Theme.space2/);
});
