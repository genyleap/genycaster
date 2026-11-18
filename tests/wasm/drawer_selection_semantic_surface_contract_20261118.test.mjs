import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const read = (path) => readFileSync(path, 'utf8');

test('shared drawers preserve interruptible edge gestures and semantic reduced-motion timing', () => {
  const drawer = read('ui/components/controls/GenyDrawer.qml');
  assert.match(drawer, /readonly property int sheetMotionDuration:\s*Theme\.reduceMotion \? 0 : Theme\.motionStandard \* 1\.8/);
  assert.match(drawer, /edge: bottomSheet \? Qt\.BottomEdge/);
  assert.equal((drawer.match(/property: "position"/g) || []).length, 2);
  assert.doesNotMatch(drawer, /readonly property int sheetMotionDuration:[^\n]*:\s*275/);
});

test('selection handle uses the scene-graph circle instead of a second legacy rounded rectangle', () => {
  const textarea = read('ui/components/controls/BidiTextArea.qml');
  assert.match(textarea, /Superellipse\s*\{\s*anchors\.centerIn: parent\s*width: 14\s*height: 14\s*curveDepth: width \/ 2\s*exponent: 2\s*fillColor: Theme\.nativeSelectionAccent\s*borderWidth: 2\s*borderColor: Theme\.editorSurface/);
  assert.doesNotMatch(textarea, /\bradius\s*:/);
  assert.match(textarea, /selectionHandleDragging = true/);
  assert.match(textarea, /onCanceled: root\.selectionHandleDragging = false/);
});
