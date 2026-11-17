import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = file => readFileSync(new URL('../../' + file, import.meta.url), 'utf8');

test('profile rail cards and pulse snapshot receive pointer-aware Neon', () => {
  const rail = read('ui/components/navigation/SocialSidebar.qml');
  const pulse = read('ui/components/feed/FeedPulseCard.qml');
  assert.equal((rail.match(/interactive: true/g) || []).length, 3,
    'search, profile and suggested cards all receive passive pointer input');
  assert.match(rail, /FeedPulseCard \{[\s\S]*?reflectionSource: root\.reflectionSource/);
  assert.match(pulse, /^NeonSurface \{/m);
  assert.match(pulse, /interactive: true/);
  assert.match(pulse, /borderWidth: Theme\.strokeHairline/);
  assert.match(pulse, /borderColor: root\.pointerActive \? Theme\.borderDefault/);
  assert.match(rail, /spacing: Theme\.space3/);
});

test('profile activity tiles use one antialiased Neon material per visible card', () => {
  const panel = read('ui/components/profile/GenyPulsePanel.qml');
  for (const content of ['farcasterContent', 'githubContent', 'walletContent']) {
    const pattern = new RegExp('NeonSurface \\{[\\s\\S]{0,250}?implicitHeight: ' + content + '\\.implicitHeight');
    assert.match(panel, pattern, content);
  }
  assert.equal((panel.match(/ambientNeon: true/g) || []).length, 3);
  assert.equal((panel.match(/borderWidth: Theme\.strokeHairline/g) || []).length, 3);
});

test('Magazine casts retain a real GPU-rim stroke and 16px gaps on desktop', () => {
  const cast = read('ui/components/feed/CastItem.qml');
  const profile = read('ui/components/profile/ProfileSurface.qml');
  const wall = read('ui/components/feed/FeedMasonryView.qml');
  assert.match(cast, /borderWidth: root\.magazineCard \? Theme\.strokeHairline : 0/);
  assert.match(cast, /borderColor: root\.magazineCard[\s\S]*?magazineCardSurface\.pointerActive/);
  assert.match(profile, /readonly property int profileMargin: root\.compact \? Theme\.mobileGutter : Theme\.space4/);
  assert.match(profile, /id: profileMagazineView[\s\S]{0,600}?gutter: Theme\.space4/);
  assert.match(wall, /layoutModel\.relayout\(columns, cardWidth, gutter,/);
  assert.match(wall, /sharedNeonPointerY: masonryPointer\.point\.position\.y[\s\S]*?\+ view\.contentY - slot\.y/);
});

test('magazine and timeline selectors are accessible text-only tabs', () => {
  const tabs = read('ui/components/feed/ExplorerLayoutControls.qml');
  const button = read('ui/components/controls/GenyTabButton.qml');
  assert.equal((tabs.match(/GenyTabButton \{/g) || []).length, 2);
  assert.match(tabs, /showSelectionIndicator: false/g);
  assert.doesNotMatch(tabs, /Superellipse \{|customSelectedSurface:/);
  assert.match(tabs, /iconOnly: root\.condensed/g);
  assert.match(button, /visible: !control\.iconOnly/);
  assert.match(button, /selected: false/);
});
