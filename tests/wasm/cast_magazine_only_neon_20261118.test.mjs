import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const cast = readFileSync('ui/components/feed/CastItem.qml', 'utf8');

test('ordinary feed timeline rows remain transparent without per-row Neon GPU surface', () => {
  const start = cast.indexOf('id: magazineCardSurface');
  const end = cast.indexOf('ColumnLayout {\n                id: messageContent', start);
  assert.ok(start >= 0 && end > start, 'magazine surface must precede text');
  const chrome = cast.slice(start, end);
  assert.match(chrome, /visible: root\.magazineCard/);
  assert.match(chrome, /fillColor: root\.magazineCard \? Theme\.surfaceRaised : "transparent"/);
  assert.match(chrome, /borderWidth: root\.magazineCard \? Theme\.strokeHairline : 0/);
  assert.match(chrome, /interactive: root\.magazineCard && !root\.scrollMoving/);
  assert.match(chrome, /ambientNeon: root\.magazineCard/);
  assert.match(cast, /Item\s*\{\s*id: messageSurface/);
  assert.match(cast, /clip: root\.magazineCard/);
});

test('Magazine pointer tracking remains scroll-gated and RTL gutters unchanged', () => {
  assert.match(cast,/enabled: root\.magazineCard && !root\.scrollMoving && Theme\.neonEnabled/);
  assert.match(cast,/anchors\.leftMargin: root\.magazineCard \? Theme\.space4 : root\.compact \? Theme\.space2 : Theme\.space4/);
  assert.match(cast,/anchors\.rightMargin: root\.magazineCard \? Theme\.space4 : root\.compact \? Theme\.space2 : Theme\.space4/);
  assert.match(cast,/implicitHeight: messageContent\.implicitHeight \+ Theme\.space2 \* 2/);
  assert.match(cast,/Layout\.minimumWidth: 0/);
});
