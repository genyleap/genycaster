import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

const read = name => readFileSync(new URL('../../ui/components/' + name, import.meta.url), 'utf8');

test('all Cast reactions have uniform unboxed touch targets and semantic active hues', () => {
    const button = read('EngagementButton.qml');
    const bar = read('EngagementBar.qml');
    assert.match(button, /implicitHeight: Theme\.touchTarget/);
    assert.match(button, /: "transparent"/);
    assert.match(button, /borderWidth: Theme\.keyboardFocusVisible\(control\) \? 2 : 0/);
    assert.match(button, /shadowEnabled: false/);
    assert.match(button, /property color activeColor: Theme\.accent/);
    assert.match(bar, /iconGlyph: root\.displayedLikeIcon[\s\S]*?iconFilled: root\.effectiveLikedByMe[\s\S]*?activeColor: Theme\.negative/);
    assert.match(bar, /iconGlyph: Theme\.iconRepost[\s\S]*?activeColor: Theme\.positive/);
    assert.match(bar, /displayedLikeIcon: Theme\.iconHeart/);
    assert.doesNotMatch(bar, /contextualLikeIcon/);
});

test('mobile Home uses viewport-aware dimensions and preserves composer clearance', () => {
    const home = read('HomeDashboard.qml');
    const shell = read('AppShell.qml');
    assert.match(home, /readonly property bool densePhone: root\.compact && root\.height < 840/);
    assert.match(home, /implicitHeight: root\.compact \? \(root\.densePhone \? 148 : 170\)/);
    assert.match(home, /avatarSize: root\.densePhone \? 56 : 76/);
    assert.match(home, /implicitHeight: root\.densePhone \? 62 : 92/);
    assert.match(home, /Math\.max\(108, Theme\.touchTarget \+ Theme\.space6 \* 2\)/);
    assert.match(home, /visible: !root\.composerFocused/);
    assert.match(shell, /anchors\.bottomMargin: root\.mobile && !root\.homeComposerFocused/);
    assert.match(shell, /\? mobileBottomNavigation\.height : 0/);
    assert.doesNotMatch(shell, /mobileBottomNavigation\.height \+ Theme\.space5/);
});

test('thread rail has rounded capsule ends and softened accent nodes', () => {
    const rail = read('GenyThreadRail.qml');
    assert.match(rail, /Superellipse \{[\s\S]*?anchors\.bottom: parent\.bottom/);
    assert.match(rail, /curveDepth: width \/ 2/);
    assert.match(rail, /exponent: 2\.01/);
    assert.match(rail, /opacity: root\.highlighted \? 0\.7 : 0\.32/);
    assert.doesNotMatch(rail, /Rectangle \{/);
});

test('real token 24h movement colors both price and trend and Copy supplies feedback', () => {
    const token = read('TokenAddressPreview.qml');
    assert.match(token, /color: root\.changeKnown && root\.priceKnown && root\.change24h > 0\s+\? Theme\.positive/);
    assert.match(token, /root\.changeKnown && root\.priceKnown && root\.change24h < 0\s+\? Theme\.negative : Theme\.textPrimary/);
    assert.match(token, /color: root\.change24h >= 0 \? Theme\.positive : Theme\.negative/);
    assert.match(token, /property bool addressCopied: false/);
    assert.match(token, /text: qsTrId\("genycaster\.feedback\.copy"\)/);
    assert.match(token, /root\.addressCopied = true/);
    assert.match(token, /presentationBridge\.copyText\(root\.address\)/);
    assert.match(token, /Layout\.preferredWidth: Theme\.touchTarget/);
});
