import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const cast = readFileSync('ui/components/feed/CastItem.qml', 'utf8');

test('mixed media supporting cards have an independent bounded admission gate', () => {
  assert.match(cast, /property bool supportingAdmitted: false/);
  assert.match(cast, /id: supportingAdmissionTimer[\s\S]*?interval: 48/);
  assert.match(cast, /active: root\.mediaFirst && root\.supportingEmbeds\.length > 0\s*&& root\.supportingAdmitted/);
});

test('repeated metadata refreshes preserve an already-admitted card', () => {
  assert.match(cast, /function supportingIdentity\(\)/);
  assert.match(cast, /onSupportingEmbedsChanged:\s*\{\s*const identity = root\.supportingIdentity\(\)/);
  assert.match(cast, /if \(root\.admittedSupportingIdentity !== identity\)\s*\{\s*root\.admittedSupportingIdentity = identity\s*root\.supportingAdmitted = false/);
});

test('scroll motion defers new mixed cards without starving ordinary text-only links', () => {
  assert.match(cast, /if \(!root\.mediaFirst\)\s*\{\s*root\.supportingAdmitted = true/);
  assert.match(cast, /if \(!root\.supportingAdmitted && !root\.scrollMoving && root\.visible\)/);
  assert.match(cast, /onScrollMovingChanged: root\.scheduleSupportingAdmission\(\)/);
  assert.match(cast, /onVisibleChanged: root\.scheduleSupportingAdmission\(\)/);
});

test('rich supporting cards remain below the author caption and visual media', () => {
  assert.ok(cast.indexOf('objectName: "castTextBody"') <
            cast.indexOf('objectName: "castVisualMedia"'));
  assert.ok(cast.indexOf('objectName: "castVisualMedia"') <
            cast.indexOf('&& root.supportingAdmitted'));
});
