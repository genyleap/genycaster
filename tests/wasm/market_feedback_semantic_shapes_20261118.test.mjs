import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const source = path => readFileSync(path, 'utf8');
for (const file of [
  'ui/components/home/MarketSeasonMeter.qml',
  'ui/components/home/MarketFearGauge.qml',
  'ui/components/settings/FeedbackDialog.qml'
]) {
  test(`${file} keeps rounded indicator shapes in scene-graph surfaces`, () => {
    const qml = source(file);
    assert.doesNotMatch(qml, /\bradius\s*:/, 'no legacy Rectangle radius');
    assert.match(qml, /Superellipse\s*\{/);
    assert.match(qml, /curveDepth:/);
    assert.match(qml, /exponent:\s*2/);
  });
}

test('market index markers retain RTL logic and accessible readout', () => {
  const gauge = source('ui/components/home/MarketFearGauge.qml');
  const season = source('ui/components/home/MarketSeasonMeter.qml');
  assert.match(gauge, /xScale:\s*Theme\.rtl\s*\?\s*-1\s*:\s*1/);
  assert.match(season, /Theme\.rtl\s*\?\s*1-root\.progress\s*:\s*root\.progress/);
  assert.match(gauge, /Accessible\.role:\s*Accessible\.Indicator/);
  assert.match(season, /Accessible\.role:\s*Accessible\.Indicator/);
  assert.match(season, /borderWidth:\s*3/);
  assert.match(gauge, /borderWidth:\s*2\.5/);
});
