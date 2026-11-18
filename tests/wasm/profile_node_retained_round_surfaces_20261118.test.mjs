import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const read = path => readFileSync(path, 'utf8');

test('profile link chip and icon tile use retained semantic surfaces', () => {
  const qml = read('ui/components/profile/ProfileLinkRow.qml');
  assert.doesNotMatch(qml, /\bradius\s*:/);
  assert.match(qml, /background: Superellipse\s*\{/);
  assert.match(qml, /Behavior on fillColor\s*\{\s*ColorAnimation/);
  assert.match(qml, /borderColor: root\.hovered \? Theme\.borderMuted : "transparent"/);
  assert.match(qml, /fillColor: Theme\.primarySurface/);
});

test('node health progress retains animated width, colors and non-native rounding', () => {
  const qml = read('ui/components/settings/NodeManagementPanel.qml');
  assert.doesNotMatch(qml, /\bradius\s*:/);
  assert.match(qml, /Superellipse\s*\{[\s\S]*?curveDepth: height \/ 2/);
  assert.match(qml, /curveDepth: parent\.curveDepth/);
  assert.match(qml, /fillColor: Theme\.positive/);
  assert.match(qml, /Behavior on width\s*\{/);
  assert.match(qml, /Behavior on fillColor\s*\{/);
});
