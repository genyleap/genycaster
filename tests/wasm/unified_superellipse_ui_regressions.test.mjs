import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('reaction actions have touch-safe hit targets without permanent card borders', () => {
    const button = read('ui/components/EngagementButton.qml');
    const bar = read('ui/components/EngagementBar.qml');
    assert.match(button, /implicitHeight: Theme.touchTarget/);
    assert.match(button, /fillColor: "transparent"/);
    assert.doesNotMatch(button, /fillColor: control.down \? Theme.surfacePressed/);
    assert.doesNotMatch(button, /control.active \? Qt.rgba/);
    assert.match(button, /: "transparent"/);
    assert.match(button, /borderWidth: Theme.keyboardFocusVisible\(control\) \? 2 : 0/);
    assert.match(button, /property color activeColor: Theme.accent/);
    assert.match(button, /curveDepth: Theme.curveControl/);
    assert.doesNotMatch(button, /containmentMask:\s*background/);
    for (const kind of ['castReplyButton','castQuoteButton','castViewCount'])
        assert.ok(bar.includes(kind),kind);
});

test('Home hub uses restrained Superellipse workspace cards and accessible composer', () => {
    const home=read('ui/components/HomeDashboard.qml');
    assert.match(home,/component HubWorkspace: ColumnLayout/);
    assert.equal((home.match(/HubWorkspace \{/g)||[]).length,2);
    assert.match(home,/"homeLocked_" \+ modelData.key/);
    assert.match(home,/key: "wallet"/);
    assert.match(home,/"homeShortcut_" \+ modelData.destination/);
    assert.match(home,/No MouseArea or TapHandler: a locked feature is not a button/);
    assert.match(home,/curveDepth: Theme.curveContainer/);
    assert.match(home,/ContextMenu.menu: EditorContextMenu \{ editor: mobileDraftEditor \}/);
});

test('replies and nested replies share real threaded connector and padded cards', () => {
    const thread = read('ui/components/ThreadReplyItem.qml');
    const rail = read('ui/components/GenyThreadRail.qml');
    const cmake = read('ui/CMakeLists.txt');
    assert.match(cmake, /components\/GenyThreadRail.qml/);
    assert.match(thread, /GenyThreadRail \{[\s\S]*?depth: root.depth/);
    assert.match(thread, /id: replyCard[\s\S]*?fillColor: root.highlighted \? Theme.surfaceSelected/);
    assert.match(thread, /anchors.margins: root.compact \? Theme.space3 : Theme.space4/);
    assert.doesNotMatch(thread, /source: Qt.resolvedUrl\("ThreadReplyItem.qml"\)/);
    assert.match(read('ui/components/DetailSurface.qml'), /reuseItems: true/);
    assert.match(read('ui/components/DetailSurface.qml'), /delegate: ThreadReplyItem/);
    assert.match(thread, /onQuoteRequested: root.quoteRequested\(root.reply\)/);
    assert.match(rail, /fillColor: root.highlighted \? Theme.accent : Theme.borderStrong/);
    assert.match(rail, /curveDepth: width \/ 2/);
    assert.match(rail, /exponent: 2.01/);
    assert.doesNotMatch(rail, /Rectangle \{/);
    assert.match(rail, /anchors.bottom: parent.bottom/);
});

test('casts notifications and explorer tabs reuse Superellipse surfaces and accent', () => {
    const feed = read('ui/components/CastItem.qml');
    const notifications = read('ui/components/NotificationSurface.qml');
    const tabs = read('ui/components/ContentTabs.qml');
    const bottom = read('ui/components/BottomNavButton.qml');
    assert.match(feed, /id: messageSurface[\s\S]*?Superellipse \{/);
    assert.match(feed, /curveDepth: Theme.curveContainer/);
    assert.match(notifications, /id: notificationRow[\s\S]*?Superellipse \{/);
    assert.match(notifications, /notificationRow.latestActivity[\s\S]*?Qt\.rgba\(Theme\.accent/);
    assert.match(tabs, /color: tabButton.selected \? Theme.accent/);
    assert.match(tabs, /Layout.preferredWidth: String\(modelData.id\) === "conversations" \? 1.65 : 1/);
    assert.doesNotMatch(tabs, /fillColor: tabButton.selected/);
    assert.match(bottom, /fillColor: control.selected[\s\S]*?Theme.surfaceSelected/);
    assert.match(bottom, /control.darkDock \? Qt.rgba\(1, 1, 1, 0.14\)/);
});

test('thread heading shows actual reply count, not an invented metric', () => {
    const detail = read('ui/components/DetailSurface.qml');
    assert.match(detail, /id: repliesContainer/);
    assert.match(detail, /id: replyCountLabel/);
    assert.match(detail, /LocaleFormat.formatInteger\(root.prioritizedReplies.length\)/);
});
