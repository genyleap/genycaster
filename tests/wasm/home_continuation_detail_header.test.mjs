import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import test from 'node:test';

const read = file => readFileSync(new URL('../../ui/components/' + file + '.qml', import.meta.url), 'utf8');

test('Home hub replaces the duplicate cast timeline and prompt carousel', () => {
    const home = read('HomeDashboard');
    assert.match(home, /component HubWorkspace: ColumnLayout/);
    assert.match(home, /"homeShortcut_" \+ modelData.destination/);
    assert.match(home, /contentHeight: mobileColumn.implicitHeight \+ Theme.space3/);
    assert.doesNotMatch(home, /homeRecentCastPreview|homeContinuation|recentCasts|castIdeas/);
    assert.match(home, /onClicked: root.activateHubShortcut\(modelData.destination\)/);
    assert.doesNotMatch(home, /Math.max\(108, Theme.touchTarget/);
});

test('Wallet is noninteractive and the GENY banner makes no network requests', () => {
 const home=read('HomeDashboard');
 assert.match(home,/destination: "wallet"/);
 assert.match(home,/enabled: !shortcut.locked/);
 assert.match(home,/geny_token_slide_one/);
 assert.doesNotMatch(home,/fetch\(|XMLHttpRequest\(|publishCast\(/);
});

test('cast details use their own back toolbar instead of a duplicate global brand bar', () => {
    const shell = read('AppShell');
    const detail = read('DetailSurface');
    assert.match(shell, /AppHeader \{\s+\/\/ Cast details[\s\S]*?visible: root.mobile && \(root.shellRoute.length > 0[\s\S]*?presentationBridge.activeView !== "cast"\)/);
    assert.match(detail, /genycaster.detail.back_back_to_feed/);
    assert.match(detail, /onClicked: root.backRequested\(\)/);
    assert.match(detail, /Layout.topMargin: Theme.space1[\s\S]*?spacing: Theme.space2/);
});
