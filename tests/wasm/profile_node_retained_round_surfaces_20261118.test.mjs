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

test('compact node health rows retain semantic rounding, progress counts and keyboard focus', () => {
  const qml = read('ui/components/settings/NodeManagementPanel.qml');
  assert.doesNotMatch(qml, /\bradius\s*:/);
  assert.match(qml, /qsTrId\("genycaster\.nodes\.health_progress"\)\.arg\(root\.checkedNodes\)\.arg\(root\.eligibleNodes\)/);
  assert.match(qml, /qsTrId\("genycaster\.nodes\.health_passed"\)\.arg\(root\.healthyNodes\)/);
  assert.match(qml, /background: Superellipse \{[\s\S]*?borderColor: nodeRow\.modelData\.active \? Theme\.positive : Theme\.accent[\s\S]*?curveDepth: Theme\.curveControl/);
  assert.match(qml, /Theme\.keyboardFocusVisible\(nodeToggle\)/);
  assert.match(qml, /Accessible\.selected: !!nodeRow\.modelData\.active/);
});
