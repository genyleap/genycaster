import { componentPath } from './ui_component_paths.mjs';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import test from 'node:test';

const read = file => readFileSync(new URL('../../ui/components/' + componentPath(file) + '.qml', import.meta.url), 'utf8');

test('Home hub replaces the duplicate cast timeline and prompt carousel', () => {
    const home = read('HomeDashboard');
    assert.match(home, /component HubWorkspace: ColumnLayout/);
    assert.match(home, /"homeShortcut_" \+ modelData.destination/);
    assert.match(home, /contentHeight: mobileColumn.implicitHeight \+ root.bottomOverlayInset \+ Theme.space4/);
    assert.doesNotMatch(home, /homeRecentCastPreview|homeContinuation|recentCasts|castIdeas/);
    assert.match(home, /onClicked: root.activateHubShortcut\(modelData.destination\)/);
    assert.doesNotMatch(home, /Math.max\(108, Theme.touchTarget/);
});

test('Wallet remains noninteractive and the home activity card performs no network requests', () => {
 const home=read('HomeDashboard');
 assert.match(home,/destination: "wallet"/);
 assert.match(home,/enabled: !shortcut.locked/);
 assert.match(home,/component HomeActivityCard: Superellipse/);
 assert.match(home,/root.notificationUnreadCount > 0/);
 assert.doesNotMatch(home,/fetch\(|XMLHttpRequest\(|publishCast\(/);
});

test('cast details use their own back toolbar instead of a duplicate global brand bar', () => {
    const shell = read('AppShell');
    const detail = read('DetailSurface');
    assert.match(shell, /visible: root.mobile && \(root.shellRoute.length > 0/);
    assert.match(detail, /genycaster.detail.conversation/);
    assert.match(detail, /genycaster.detail.open_channel/);
    assert.match(detail, /onClicked: root.channelRequested\(root.castChannel\)/);
    assert.match(detail, /onClicked: root.composeRequested\(\)/);
    assert.match(detail, /if \(root.interactionsAvailable\) root.replyRequested\(root.detail\)/);
    assert.match(detail, /genycaster.detail.back_back_to_feed/);
    assert.match(detail, /onClicked: root.backRequested\(\)/);
    assert.match(detail, /Layout\.topMargin: root\.compact \? 0 : Theme\.space2[\s\S]*?spacing: Theme\.space2/);
});
