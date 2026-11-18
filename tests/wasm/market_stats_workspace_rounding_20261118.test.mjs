import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const files = [
  'ui/components/home/MarketPanel.qml',
  'ui/components/home/StatsBreakdown.qml',
  'ui/components/home/StatsChart.qml',
  'ui/components/setup/InitialSetup.qml',
  'ui/components/shell/AppShell.qml',
];
const read = name => readFileSync(name, 'utf8');

for (const file of files) {
  test(`${file}: retains rounded UI on shared scene-graph surfaces`, () => {
    const source = read(file);
    assert.doesNotMatch(source, /\bradius\s*:/, 'no legacy Rectangle.radius');
    assert.match(source, /Superellipse\s*\{/);
    assert.match(source, /curveDepth:/);
    assert.match(source, /fillColor:/);
  });
}

test('RTL market dominance tracks still animate from the reading origin', () => {
  const source = read(files[0]);
  assert.match(source, /anchors\.left: Theme\.rtl \? undefined : parent\.left/);
  assert.match(source, /anchors\.right: Theme\.rtl \? parent\.right : undefined/);
  assert.match(source, /curveDepth: parent\.curveDepth/);
  assert.match(source, /Behavior on width/);
});

test('statistics dot markers keep circular geometry and inspected series data', () => {
  for (const path of files.slice(1, 3)) {
    const source = read(path);
    assert.match(source, /curveDepth: width \/ 2/);
    assert.match(source, /exponent: 2/);
    assert.match(source, /Accessible\./);
  }
  const breakdown = read(files[1]);
  assert.match(breakdown, /pointFor\(root\.inspectedIndex/);
  assert.match(breakdown, /borderWidth: 1\.5/);
});

test('onboarding logo and animated orbit preserve themed glyph and position', () => {
  const source = read(files[3]);
  assert.match(source, /phase: desktopBrandPane\.orbitYaw \* Math\.PI \/ 180/);
  assert.match(source, /fillColor: Theme\.setupHeroTitleInk/);
  assert.match(source, /fillColor: Theme\.platformAccentColor/);
  assert.match(source, /text: Theme\.platformIcon/);
});

test('busy activity strip still represents actual work and respects reduced motion', () => {
  const source = read(files[4]);
  assert.match(source, /visible: presentationBridge\.isBusy && !root\.initialSetupOpen/);
  assert.match(source, /running: workspaceActivity\.visible && !Theme\.reduceMotion/);
  assert.match(source, /SequentialAnimation on x/);
  assert.match(source, /fillColor: Theme\.accent/);
});
