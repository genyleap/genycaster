import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

const read = name => readFileSync(new URL('../../ui/components/' + name, import.meta.url), 'utf8');

test('profile sort/filter shares the existing tab row instead of reserving an empty row', () => {
    const profile = read('ProfileSurface.qml');
    const headerStart = profile.indexOf('header: ColumnLayout {');
    const delegateStart = profile.indexOf('delegate: Item {', headerStart);
    assert.ok(headerStart >= 0 && delegateStart > headerStart);
    const header = profile.slice(headerStart, delegateStart);

    assert.equal((header.match(/id: profileSortFilter/g) || []).length, 1);
    assert.equal((header.match(/id: profileTabs/g) || []).length, 1);
    assert.match(header, /RowLayout \{\s+anchors\.fill: parent[\s\S]*?GenyTabStrip \{\s+id: profileTabs[\s\S]*?FilterDropdown \{\s+id: profileSortFilter/);
    assert.doesNotMatch(header, /implicitHeight: 34 \+ Theme\.space1/);
    assert.match(header, /minimumTabWidth: root\.compact \? 52 : 68/);
    assert.match(header, /Layout\.fillWidth: true[\s\S]*?Layout\.minimumWidth: 0/);
    assert.match(header, /onActivated: tabId => root\.profileFeedRequested\(tabId\)/);
});

test('both chronological and popular metrics keep their original effective actions', () => {
    const profile = read('ProfileSurface.qml');
    assert.match(profile, /id: profileSortFilter[\s\S]*?model: root\.profileFeed === "user-popular"/);
    assert.match(profile, /root\.profileSortMode === "oldest" \? 1 : 0/);
    assert.match(profile, /root\.popularMetric = index === 1 \? "likes"/);
    assert.match(profile, /root\.profileSortMode = index === 1 \? "oldest" : "newest"/);
    assert.match(profile, /objectName: "profileSortFilter"/);
});

test('mobile floating dock preserves scroll clearance without a painted footer', () => {
    const shell = read('AppShell.qml');
    const footer = read('BottomNavigation.qml');
    assert.match(shell, /anchors\.bottomMargin: 0/);
    assert.match(read('FeedSurface.qml'), /bottomMargin: root\.compact \? 96 : 0/);
    assert.match(read('ProfileSurface.qml'), /bottomMargin: root\.compact \? 96 : 0/);
    assert.doesNotMatch(shell, /mobileBottomNavigation\.height \+ Theme\.space5/);
    assert.match(footer, /anchors\.topMargin: Qt\.platform\.os === "android" \? 4 : 8/);
    assert.match(footer, /objectName: "mobilePrimaryActionButton"/);
    assert.doesNotMatch(footer, /objectName: "mobileFooterBackground"/);
    assert.doesNotMatch(footer, /objectName: "mobileContentLowerEdge"/);
});
