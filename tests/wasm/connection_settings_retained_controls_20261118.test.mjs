import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(path, 'utf8');

test('connections verification and provider cards use semantic retained surfaces', () => {
  const qml = read('ui/components/profile/ConnectionsPopup.qml');
  assert.doesNotMatch(qml, /\bradius\s*:/);
  assert.match(qml, /Superellipse\s*\{[\s\S]*?fillColor: Theme\.surfaceMuted/);
  assert.match(qml, /fillColor: Theme\.successSurface/);
  assert.match(qml, /fillColor: Theme\.connectionIconBackground/);
  assert.match(qml, /borderColor: manualLinkedInField\.activeFocus/);
  assert.match(qml, /curveDepth: Theme\.curveContainer/);
  assert.match(qml, /RowLayout\s*\{\s*layoutDirection: Theme\.uiDirection/);
});

test('settings neon navigation keeps RTL selected stripe and animated color choices', () => {
  const qml = read('ui/components/settings/SettingsPopup.qml');
  assert.doesNotMatch(qml, /\bradius\s*:/);
  assert.match(qml, /visible: navTab\.selected/);
  assert.match(qml, /x: Theme\.rtl \? parent\.width - width : 0/);
  assert.match(qml, /fillColor: actionColorChoice\.isSelected \? Theme\.surfaceSelected/);
  assert.match(qml, /Behavior on fillColor\s*\{/);
  assert.match(qml, /borderColor: actionColorChoice\.isSelected \? Theme\.accent : Theme\.borderSubtle/);
});

test('theme swatches keep circular selection rings and contrast-aware icon', () => {
  const qml = read('ui/components/settings/SettingsPopup.qml');
  assert.match(qml, /curveDepth: width \/ 2\s*\n\s*exponent: 2/);
  assert.match(qml, /borderWidth: themePreset\.selected/);
  assert.match(qml, /borderColor: themePreset\.activeFocus/);
  assert.match(qml, /fillColor: themePreset\.presetSwatch/);
  assert.match(qml, /Theme\.contrastInk\(themePreset\.presetSwatch\)/);
});
