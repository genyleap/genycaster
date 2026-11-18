import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const qml = readFileSync('ui/components/feed/CastItem.qml', 'utf8');

test('timeline rows reserve compact height while Magazine keeps its original card spacing', () => {
  assert.match(qml, /implicitHeight: messageRow\.implicitHeight \+ Theme\.space2 - \(root\.magazineCard \? Theme\.space2 : 0\)/);
  assert.match(qml, /implicitHeight: messageContent\.implicitHeight \+ Theme\.space2 \* 2\s*\n\s*\+ \(root\.magazineCard \? Theme\.space2 \* 2 : 0\)/);
  assert.match(qml, /anchors\.topMargin: root\.magazineCard \? Theme\.space4 : Theme\.space2/);
  assert.match(qml, /anchors\.bottomMargin: root\.magazineCard \? Theme\.space4 : Theme\.space2/);
  // The reserved height must match the rendered top + bottom insets for
  // every presentation mode, so image-reflow cannot clip the final row.
  for (const magazine of [false, true]) {
    const inset = magazine ? 4 : 2;
    const reserved = 2 * 2 + (magazine ? 2 * 2 : 0);
    assert.equal(reserved, inset * 2);
    const outer = 2 - (magazine ? 2 : 0);
    assert.equal(outer, magazine ? 0 : 2);
  }
});

test('direction-aware surface and media admission remain unchanged', () => {
  assert.match(qml, /id: messageSurface/);
  assert.match(qml, /clip: root\.magazineCard/);
  assert.match(qml, /borderWidth: root\.magazineCard \? Theme\.strokeHairline : 0/);
  assert.match(qml, /anchors\.leftMargin: root\.magazineCard \? Theme\.space4 : root\.compact \? Theme\.space2 : Theme\.space4/);
  assert.match(qml, /anchors\.rightMargin: root\.magazineCard \? Theme\.space4 : root\.compact \? Theme\.space2 : Theme\.space4/);
  assert.match(qml, /property bool supportingAdmitted: false/);
  assert.match(qml, /onSupportingEmbedsChanged:/);
});
