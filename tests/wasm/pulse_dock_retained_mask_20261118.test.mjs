import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(path, 'utf8');

test('Pulse tiles keep day-level hit testing, range border and partial-data semantics', () => {
  const pulse = read('ui/components/profile/GenyPulsePanel.qml');
  assert.doesNotMatch(pulse, /\bradius\s*:/);
  assert.match(pulse, /delegate: Superellipse\s*\{/);
  assert.match(pulse, /fillColor: !inYear \? "transparent" :/);
  assert.match(pulse, /Theme\.pulseUnobservedTile/);
  assert.match(pulse, /fillColor: inYear\s*\n\s*\? root\.githubTileColor/);
  assert.match(pulse, /borderWidth: root\.githubRange!=="1y" && inRange \? 1 : 0/);
  assert.match(pulse, /borderColor: Theme\.pulseGithubRangeBorder/);
  assert.match(pulse, /onClicked: root\.selectedDate = root\.dayString\(parent\.day\)/);
  assert.match(pulse, /onClicked: root\.selectedGithubDate=parent\.date/);
});

test('Pulse icon badges preserve provider art and themed fills', () => {
  const pulse = read('ui/components/profile/GenyPulsePanel.qml');
  assert.match(pulse, /fillColor: Theme\.pulseFarcasterIconSurface/);
  assert.equal((pulse.match(/fillColor: Theme\.pulseBrandIconBackground/g) || []).length, 2);
  assert.match(pulse, /source: root\.providerIconRoot \+ "github\.svg"/);
  assert.match(pulse, /source: root\.providerIconRoot \+ "ethereum-wallet\.svg"/);
});

test('dock glass mask keeps hidden alpha capture of the same capsule geometry', () => {
  const dock = read('ui/components/navigation/BottomNavigation.qml');
  assert.doesNotMatch(dock, /\bradius\s*:/);
  assert.match(dock, /id: capsuleMask/);
  assert.match(dock, /Superellipse\s*\{\s*x: navigationCapsule\.sampleLeft/);
  assert.match(dock, /curveDepth: height \/ 2\s*\n\s*exponent: 2/);
  assert.match(dock, /fillColor: Theme\.dockMaskWhite/);
  assert.match(dock, /sourceItem: capsuleMask/);
  assert.match(dock, /live: root\.useBackdropBlur/);
  assert.match(dock, /sourceRect: Qt\.rect\(0, 0, capsuleMask\.width, capsuleMask\.height\)/);
});
