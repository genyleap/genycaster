import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import test from 'node:test';

const qml = name => readFileSync(new URL('../../ui/components/' + name + '.qml', import.meta.url), 'utf8');

test('dark mobile navigation uses neutral theme-independent selection instead of permanent blue', () => {
 const button = qml('BottomNavButton');
 assert.match(button, /control\.darkDock \? Qt\.rgba\(1, 1, 1, 0\.14\)/);
 assert.match(button, /control\.darkDock \? "#ffffff"/);
 assert.doesNotMatch(button, /#8bb2ff|0\.22, 0\.38, 0\.76, 0\.44/);
});

test('Home prompt opens a full-page mobile composer without an inline keyboard', () => {
 const home = qml('HomeDashboard');
 const composer = qml('ComposerDrawer');
 const shell = qml('AppShell');
 assert.match(home,/objectName: "homeComposerLauncher"/);
 assert.match(home,/Layout\.preferredHeight: root\.androidCompact \? 52 : 60/);
 assert.match(home,/verticalAlignment: Text\.AlignVCenter/);
 assert.match(home,/onClicked: root\.composeRequested\(root\.draftText\)/);
 assert.match(composer,/fullScreen: root\.compact/);
 assert.match(composer,/objectName: "mobileComposerNavigation"/);
 assert.match(composer,/objectName: "mobileComposerMediaBar"/);
 assert.match(shell,/onComposeRequested: text => root\.openComposer\(null, "", text\)/);
});

test('thread reply rail nodes align and incoming replies to my cast stand out', () => {
 const rail = qml('GenyThreadRail');
 const item = qml('ThreadReplyItem');
 const detail = qml('DetailSurface');
 assert.match(rail, /fillColor: root\.highlighted \? Theme\.accent : Theme\.borderStrong/);
 assert.match(rail, /width: root\.compact \? 8 : 9[\s\S]*?borderWidth: 0/);
 assert.match(item, /borderWidth: 0/);
 assert.match(item, /borderColor: "transparent"/);
 assert.match(detail, /id: replyCountLabel[\s\S]*?LocaleFormat\.formatInteger\(root\.prioritizedReplies\.length\)/);
 assert.match(detail, /curveDepth: height \/ 2/);
});

test('stories and their activity badges are upscaled together', () => {
 const item = qml('StoryItem');
 const strip = qml('StoryStrip');
 assert.match(item, /frameSize: root\.compact \? 62 : 66/);
 assert.match(item, /avatarSize: root\.compact \? 54 : 58/);
 assert.match(item, /implicitWidth: root\.compact \? 74 : 78/);
 assert.match(item, /width: 25\s+height: 25/);
 assert.match(item, /width: 24\s+height: 24/);
 assert.match(item, /preferredSize: 13/);
 assert.match(strip, /implicitHeight: visible \? \(root\.compact \? 94 : 98\)/);
});

test('floating dock keeps feed clearance without an opaque footer', () => {
 const shell = qml('AppShell');
 const footer = qml('BottomNavigation');
 assert.match(shell, /spacing: root\.mobile \? 0 : Theme\.space3/);
 assert.match(shell, /anchors\.bottomMargin: 0/);
 assert.match(qml('FeedSurface'), /bottomMargin: root\.compact \? 96 : 0/);
 assert.doesNotMatch(footer, /objectName: "mobileFooterBackground"/);
 assert.doesNotMatch(footer, /objectName: "mobileContentLowerEdge"/);
});
