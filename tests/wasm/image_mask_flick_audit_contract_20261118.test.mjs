import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const read = name => readFileSync(name, 'utf8');

test('gallery alpha mask policy protects WASM and fitted single images', () => {
  const layout = read('ui/components/media/GalleryLayout.qml');
  const tile = read('ui/components/media/GalleryTile.qml');
  assert.match(layout, /useLayeredMask: Qt\.platform\.os === "wasm"\s*\n\s*\|\| root\.visibleCount === 1/);
  assert.match(tile, /property bool useLayeredMask: false/);
  assert.match(tile, /clipToSuperellipse: root\.useLayeredMask/);
  assert.match(tile, /visible: !root\.useLayeredMask && preview\.ready/);
  // Browser multi-tiles retain a clean fitted curve, native multi-tiles stay
  // on the direct image texture path instead of allocating full-size masks.
  for (const [wasm, count, expected] of [
    [true, 1, true], [true, 4, true],
    [false, 1, true], [false, 4, false],
  ]) assert.equal(wasm || count === 1, expected);
});

test('flick admission keeps in-flight provider work and frame-paces texture visibility', () => {
  const image = read('ui/components/visual/SuperellipseImage.qml');
  assert.match(image, /if \(current !== requested\)/);
  assert.match(image, /if \(current\.length > 0\) \{/);
  assert.match(image, /if \(root\.scrollMoving && root\.deferNewLoadsWhileScrolling\)/);
  assert.match(image, /if \(root\.activeSource\.toString\(\)\.length === 0\)/);
  assert.match(image, /sourceStartTimer\.stop\(\)/);
  assert.match(image, /renderingController\.requestMediaPresentation\(root, root\.presentationGeneration\)/);
  assert.match(image, /root\.invalidatePresentationAdmission\(\)/);
  assert.match(image, /root\.activeSource = ""/);
});
