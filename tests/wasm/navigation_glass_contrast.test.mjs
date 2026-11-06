import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const theme = readFileSync(new URL('../../ui/theme/Theme.qml', import.meta.url), 'utf8');
const dock = readFileSync(new URL('../../ui/components/BottomNavigation.qml', import.meta.url), 'utf8');
function token(name, dark) {
  const match = theme.match(new RegExp('property color ' + name + ': dark \\? "(#[A-Fa-f0-9]+)" : "(#[A-Fa-f0-9]+)"'));
  assert.ok(match, name);
  const hex = match[dark ? 1 : 2].slice(1);
  const rgba = hex.match(/../g).map(value => parseInt(value, 16) / 255);
  return hex.length === 8 ? [...rgba.slice(1), rgba[0]] : [...rgba, 1];
}
function composite(foreground, background) {
  return foreground.slice(0, 3).map((value, i) => value * foreground[3] + background[i] * (1 - foreground[3]));
}
function luminance(color) {
  return color.slice(0, 3).map(value => value <= 0.04045 ? value / 12.92 : ((value + 0.055) / 1.055) ** 2.4)
    .reduce((sum, value, i) => sum + value * [0.2126, 0.7152, 0.0722][i], 0);
}
function contrast(a, b) {
  const values = [luminance(a), luminance(b)].sort((x, y) => x - y);
  return (values[1] + 0.05) / (values[0] + 0.05);
}
for (const dark of [false, true]) {
  test(`${dark ? 'Dark' : 'Light'} navigation remains legible over arbitrary media`, () => {
    const tint = token('navigationGlassTint', dark);
    assert.ok(tint[3] < 0.8, 'material retains visible backdrop variation');
    const inks = ['navigationGlassInk', 'navigationGlassInkMuted'].map(name => token(name, dark));
    // The material is monotonic in each RGB channel. Include both extreme
    // luminances plus saturated colors, gradients and the selected wash.
    for (const r of [0, 0.25, 0.5, 0.75, 1])
      for (const g of [0, 0.25, 0.5, 0.75, 1])
        for (const b of [0, 0.25, 0.5, 0.75, 1]) {
          const surface = composite(tint, [r, g, b]);
          assert.ok(contrast(inks[1], surface) >= 4.5, `muted icon over ${r},${g},${b}`);
          const selected = composite(dark ? [1, 1, 1, 0x24 / 255] : [0x25 / 255, 0x2c / 255, 0x3c / 255, 0x1e / 255], surface);
          assert.ok(contrast(inks[0], selected) >= 4.5, `selected label over ${r},${g},${b}`);
        }
  });
}
test('dock pairs the material with its foregrounds and preserves smooth mask coverage', () => {
  assert.match(dock, /destination.active \? Theme.navigationGlassInk : Theme.navigationGlassInkMuted/);
  assert.match(dock, /color: Theme.navigationGlassInk\s/);
  assert.match(dock, /maskThresholdMin: 0\.5[\s\S]*?maskSpreadAtMin: 1\.0/);
  assert.match(dock, /objectName: "mobileCapsuleGlassTint"[\s\S]*?highQualityEdges: true/);
});
