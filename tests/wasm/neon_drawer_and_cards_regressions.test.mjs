import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const drawer=read('ui/components/controls/GenyDrawer.qml');
const bookmarks=read('ui/components/profile/BookmarksSurface.qml');
const apps=read('ui/components/apps/AppsSurface.qml');
const notices=read('ui/components/feed/NotificationSurface.qml');
const casts=read('ui/components/feed/CastItem.qml');
const material=read('ui/components/visual/NeonSurface.qml');

test('edge-mounted drawers restore the neon rim while preserving square attached corners',()=>{
  assert.match(drawer,/neonAmbient:\s*!control\.fullScreen/);
  assert.match(drawer,/curveTopLeft: !control\.fullScreen && \(control\.bottomSheet \|\| control\.edge !== Qt\.LeftEdge\)/);
  assert.match(drawer,/curveTopRight: !control\.fullScreen && \(control\.bottomSheet \|\| control\.edge !== Qt\.RightEdge\)/);
  assert.match(drawer,/Math\.max\(26, parent\.curveDepth \+ Theme\.space1\)/);
  assert.match(drawer,/gradient: Gradient \{[\s\S]*?GradientStop/);
  assert.match(drawer,/control\.edge === Qt\.RightEdge[\s\S]*?"transparent"/);
  assert.doesNotMatch(drawer,/neonAmbient:\s*false/);
});

test('bookmarks recover stationary rim only close to viewport to avoid offscreen shaders',()=>{
  assert.match(bookmarks,/ambientNeon: root\.visible && !root\.moving/);
  assert.match(bookmarks,/bookmarkCard\.y \+ bookmarkCard\.height >= root\.contentY/);
  assert.match(bookmarks,/bookmarkCard\.y <= root\.contentY \+ root\.height/);
  assert.match(bookmarks,/deferredNeon: true/);
});

test('selected app categories and latest notifications display Neon rather than blank flat cards',()=>{
  assert.match(apps,/ambientNeon: destinationCard\.selected/);
  assert.match(notices,/ambientNeon: notificationRow\.visible && !list\.moving/);
  assert.match(notices,/notificationRow\.y \+ notificationRow\.height >= list\.contentY/);
  assert.match(casts,/ambientNeon: root\.magazineCard/);
  assert.match(material,/material\.visible && Theme\.neonEnabled/);
});
