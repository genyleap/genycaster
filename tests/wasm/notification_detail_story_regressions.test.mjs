import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { test } from 'node:test';

const root = resolve(import.meta.dirname, '../..');
const read = path => readFileSync(resolve(root, path), 'utf8');

test('first Following story is a clean, untruncated Add action', () => {
    const item = read('ui/components/StoryItem.qml');
    assert.match(item, /visible: !root\.addStory\s+anchors\.centerIn: parent/);
    assert.match(item, /text: root\.addStory \? qsTrId\("genycaster\.home\.add_short"\) : root\.displayName/);
    assert.match(item, /elide: root\.addStory \? Text\.ElideNone : Text\.ElideRight/);
    assert.match(item, /visible: root\.addStory\s+anchors\.centerIn: parent\s+text: Theme\.iconPlus/);
    assert.doesNotMatch(item, /visible: false\s+width: 20/);
});

test('notification list uses flat rows, real category icons and no circular badges', () => {
    const ui = read('ui/components/NotificationSurface.qml');
    assert.match(ui, /delegate: Item \{\s+id: notificationRow/);
    assert.match(ui, /height: 1\s+color: Theme\.divider/);
    assert.doesNotMatch(ui, /id: kindBadge/);
    assert.match(ui, /text: notificationRow\.kindGlyph\s+color: notificationRow\.kindColor/);
    assert.match(ui, /text: String\(notificationRow\.modelData\.castText \|\| ""\)/);
    assert.doesNotMatch(ui, /maximumLineCount: 3/);
});

test('notification activity filters affect the real stable list and never claim unsupported channels', () => {
    const ui = read('ui/components/NotificationSurface.qml');
    assert.match(ui, /readonly property var visibleRows: root\.filterKind === "all"/);
    assert.match(ui, /const source = root\.visibleRows \|\| \[\]/);
    assert.match(ui, /onFilterKindChanged: Qt\.callLater\(root\.requestStableModelSync\)/);
    assert.match(ui, /kind: "mention", title: qsTrId\("genycaster\.notifications\.filter_mentions"\)/);
    assert.match(ui, /kind: "reactions", title: qsTrId\("genycaster\.notifications\.filter_reactions"\)/);
    assert.doesNotMatch(ui, /kind: "channels"/);
});

test('notification cast and quoted-parent media reach the UI without losing navigation', () => {
    const bridge = read('ui/mobile/main.cpp');
    const ui = read('ui/components/NotificationSurface.qml');
    assert.match(bridge, /row\.insert\(QStringLiteral\("castEmbedUrl"\)/);
    assert.match(bridge, /row\.insert\(QStringLiteral\("parentEmbedUrl"\)/);
    assert.match(ui, /notificationImageUrl\(\s*notificationRow\.modelData\.castEmbedUrl\)/);
    assert.match(ui, /notificationImageUrl\(\s*notificationRow\.modelData\.parentEmbedUrl\)/);
    assert.match(ui, /onTapped: root\.castRequested\(String\(notificationRow\.modelData\.parentCastId/);
    assert.match(ui, /if \(castId\.length > 0\) root\.castRequested\(castId\)/);
});

test('short Add and notification category strings are translated for every supported language', () => {
    for (const locale of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                           'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
        const ts = read('ui/i18n/qml_' + locale + '.ts');
        for (const key of ['genycaster.home.add_short',
                           'genycaster.notifications.filter_activity',
                           'genycaster.notifications.filter_replies',
                           'genycaster.notifications.filter_mentions',
                           'genycaster.notifications.filter_reactions',
                           'genycaster.notifications.filtered_empty',
                           'genycaster.notifications.filtered_description',
                           'genycaster.notifications.load_more']) {
            assert.equal(ts.split('id="' + key + '"').length - 1, 1, locale + ':' + key);
        }
    }
});
