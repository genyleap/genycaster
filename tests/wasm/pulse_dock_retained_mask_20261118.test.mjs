import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(path, 'utf8');

test('Pulse batched calendar tiles keep day hit testing, range border and partial-data semantics', () => {
  const pulse = read('ui/components/profile/GenyPulsePanel.qml');
  assert.ok(!pulse.includes('radius:'), 'Do not restore legacy Rectangle.radius');
  // Two bounded painted calendars replace hundreds of scene-graph delegates,
  // retaining per-day hit testing, partial-data status and range highlighting.
  for (const expression of [
    'objectName: "farcasterActivityBatchedCanvas"',
    'objectName: "githubContributionBatchedCanvas"',
    'function drawHeatmapTile(ctx, x, y, size, corner)',
    'ctx.bezierCurveTo(',
    'Theme.pulseUnobservedTile',
    'ctx.fillStyle = String(root.githubTileColor',
    'ctx.strokeStyle = String(Theme.pulseGithubRangeBorder)',
    'if (root.githubRange !== "1y" && inRange)',
    'mouse.x - week*step >= root.githubTileSize',
    'mouse.y - dow*step >= root.githubTileSize',
    'root.selectedDate = root.dayString(day)',
    'root.selectedGithubDate = root.dayString(day)',
    'onGithubContributionDaysChanged() { githubHeatmapCanvas.requestPaint() }'
  ]) assert.ok(pulse.includes(expression), 'Missing batched calendar contract: '+expression);
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
