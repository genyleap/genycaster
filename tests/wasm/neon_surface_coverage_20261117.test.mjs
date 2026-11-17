import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const source = file => readFileSync(new URL('../../' + file, import.meta.url), 'utf8');

test('Stats KPI cards react to pointer proximity and retain a crisp superellipse rim', () => {
  const qml = source('ui/components/home/StatsKpi.qml');
  assert.match(qml, /^NeonSurface \{/m);
  assert.match(qml, /interactive: true/);
  assert.match(qml, /ambientNeon: true/);
  assert.match(qml, /neonColor: root\.pointerActive \? Theme\.accent/);
  assert.match(qml, /borderColor: root\.pointerActive \? Theme\.borderDefault/);
  assert.match(qml, /highQualityEdges: true/);
});

test('Floating New Cast CTA contains its animated label and has a saturated blue halo', () => {
  const qml = source('ui/components/navigation/FloatingComposeButton.qml');
  assert.match(qml, /contentItem: Item \{[\s\S]*?clip: true/);
  assert.match(qml, /RowLayout \{\s*id: contentRow[\s\S]*?anchors\.fill: parent/);
  assert.match(qml, /Layout\.minimumWidth: 0/);
  assert.match(qml, /elide: Text\.ElideRight/);
  assert.match(qml, /neonColor: Qt\.lighter\(Theme\.primaryFill, 1\.28\)/);
  assert.match(qml, /Theme\.primaryFill\.r, Theme\.primaryFill\.g/);
  assert.doesNotMatch(qml, /neonColor: Qt\.rgba\(1, 1, 1,/);
});

test('Audience selector is one retained superellipse with a matching animated selection surface', () => {
  const qml = source('ui/components/home/HomeFeedHeader.qml');
  const start = qml.indexOf('id: audienceSelector');
  const end = qml.indexOf('ContentTabs {', start);
  const selector = qml.slice(start, end);
  assert.match(qml, /NeonSurface \{\s*id: audienceSelector/);
  assert.match(selector, /interactive: true/);
  assert.match(selector, /highQualityEdges: true/);
  assert.match(selector, /NeonSurface \{\s*id: audienceSelectionPill/);
  assert.match(selector, /neonColor: Theme\.accent/);
  assert.match(selector, /root\.audience === "following"\) !== Theme\.rtl/);
  assert.match(selector, /Layout\.minimumWidth: 0/);
  assert.match(selector, /Layout\.maximumWidth: Layout\.preferredWidth/);
  assert.doesNotMatch(selector, /radius: 13|radius: 10|border\.width: Theme\.strokeStandard/);
});

test('Emoji mode and notification tabs use hue-correct selected Neon surfaces', () => {
  const emoji = source('ui/components/composer/EmojiPicker.qml');
  const notifications = source('ui/components/feed/NotificationSurface.qml');
  assert.match(emoji, /delegate: NeonSurface \{\s*id: modeButton/);
  assert.match(emoji, /ambientNeon: selected/);
  assert.match(emoji, /neonColor: selected \? Theme\.accent/);
  assert.match(emoji, /borderColor: selected \? Theme\.accent/);
  assert.match(notifications, /delegate: NeonSurface \{\s*id: filterTab/);
  assert.match(notifications, /ambientNeon: selected/);
  assert.match(notifications, /borderColor: selected \? Theme\.accent/);
  assert.match(notifications, /color: filterTab\.selected \? Theme\.ink/);
});

test('Notification cards keep color-aware hover rims without per-row permanent shaders', () => {
  const qml = source('ui/components/feed/NotificationSurface.qml');
  assert.match(qml, /id: notificationCardSurface[\s\S]*?deferredNeon: true/);
  assert.match(qml, /neonStrength: Theme\.neonCardStrength/);
  assert.match(qml, /neonColor: notificationRow\.latestActivity \? Theme\.accent/);
  assert.match(qml, /borderColor: notificationCardSurface\.pointerActive/);
  assert.match(qml, /highQualityEdges: true/);
});
