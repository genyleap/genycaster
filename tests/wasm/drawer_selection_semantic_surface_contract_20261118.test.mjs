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

test('editable text delegates selection to platform owners while cast grips retain semantic geometry', () => {
  const textarea = read('ui/components/controls/BidiTextArea.qml');
  const editor = read('ui/components/composer/EditorContextMenu.qml');
  const cast = read('ui/components/content/RichInlineText.qml');
  assert.match(textarea, /EditorContextMenu \{ id: editorMenu; editor: root \}/);
  assert.match(textarea, /if \(Qt\.platform\.os === "wasm"\) editorMenu\.requestSystemEditor\(\)\s*else forceActiveFocus\(\)/);
  assert.match(textarea, /persistentSelection: true/);
  assert.match(editor, /readonly property bool browser: Qt\.platform\.os === "wasm"/);
  assert.doesNotMatch(textarea, /\bselectionHandleDragging\b/);
  assert.match(cast, /Superellipse\s*\{\s*anchors\.centerIn: parent\s*width: 13\s*height: 13\s*curveDepth: width \/ 2\s*exponent: 2\s*fillColor: root\.usePlatformSelection \? Theme\.accent\s*: Theme\.nativeSelectionAccent\s*borderWidth: 2\s*borderColor: Theme\.surfaceRaised/);
  assert.match(cast, /selectionHandleDragging = true/);
  assert.match(cast, /onCanceled: \{\s*root\.selectionHandleDragging = false/);
});

test('profile holder and social proof marks use theme-rendered circles and themed hover chrome', () => {
  const profile = read('ui/components/profile/ProfileSurface.qml');
  assert.match(profile, /Superellipse\s*\{\s*id: genyHolderBadge/);
  assert.match(profile, /visible: root\.genyHolderAttested/);
  assert.match(profile, /fillColor: socialBadge\.liveVerified \? Theme\.success : Theme\.accent/);
  assert.match(profile, /curveDepth: implicitHeight \/ 2\s*exponent: 2/);
  assert.doesNotMatch(profile, /\bradius\s*:/);
  assert.doesNotMatch(profile, /ToolTip\./);
  assert.match(profile, /visible: genyHolderBadgeHover\.hovered && genyHolderBadge\.visible/);
  assert.match(profile, /visible: identityPrimaryLink\.hovered/);
  assert.match(profile, /background: PopupSurface\s*\{\s*curveDepth: Theme\.curveCompact/);
});
