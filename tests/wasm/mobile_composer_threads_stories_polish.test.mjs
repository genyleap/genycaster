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

test('mobile composer starts as a compact prompt and smoothly grows with editing and keyboard', () => {
 const home = qml('HomeDashboard');
 const shell = qml('AppShell');
 assert.match(home, /id: mobileComposerSurface[\s\S]*?Behavior on implicitHeight \{[\s\S]*?NumberAnimation \{ duration: Theme\.reduceMotion \? 0 : Theme\.motionEmphasis/);
 assert.match(home, /implicitHeight: root\.composerFocused \? 98 : 38/);
 assert.match(home, /id: mobileComposeActions\s+visible: root\.composerFocused/);
 assert.match(home, /contentHeight: mobileColumn\.implicitHeight \+ Theme\.space3/);
 assert.doesNotMatch(home, /Math\.max\(108, Theme\.touchTarget/);
 assert.match(home, /function onComposerFocusedChanged\(\) \{[\s\S]*?mobileFlick\.contentY = 0/);
 assert.match(shell, /readonly property bool homeComposerFocused:/);
 assert.match(shell, /height < preKeyboardViewportHeight - 140/);
});

test('thread reply rail uses solid rounded nodes and own replies have distinct cards', () => {
 const rail = qml('GenyThreadRail');
 const item = qml('ThreadReplyItem');
 const detail = qml('DetailSurface');
 assert.match(rail, /fillColor: root\.highlighted \? Theme\.accent : Theme\.borderStrong/);
 assert.match(rail, /width: root\.compact \? 9 : 10[\s\S]*?borderWidth: 0/);
 assert.match(item, /root\.ownReply \? Theme\.primarySurface : Theme\.surfaceRaised/);
 assert.match(item, /root\.ownReply \? Theme\.borderStrong : Theme\.borderSubtle/);
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

test('footer edge no longer masks a tall region of the scrolling feed', () => {
 const shell = qml('AppShell');
 const footer = qml('BottomNavigation');
 assert.match(shell, /spacing: root\.mobile \? 0 : Theme\.space3/);
 assert.match(footer, /y: -7\s+height: 14/);
});
