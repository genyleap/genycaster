import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const files = [
  'ui/components/profile/ProfileLinkRow.qml',
  'ui/components/settings/UpdateNoticeDrawer.qml',
  'ui/components/apps/AndroidWebView.qml',
  'ui/app/miniapp/EmbeddedBrowser.qml',
  'ui/components/composer/ComposerDrawer.qml',
  'ui/components/settings/NodeManagementPanel.qml',
  'ui/components/content/RichInlineText.qml',
];

for (const path of files) {
  test(`shared rounded control migrated without legacy Rectangle radius: ${path}`, () => {
    const source = readFileSync(path, 'utf8');
    assert.doesNotMatch(source, /\bradius\s*:/);
    assert.match(source, /Superellipse\s*\{/);
    assert.match(source, /\bcurveDepth:/);
  });
}

test('profile link hover colors stay animated with semantic scene-graph fill', () => {
  const source = readFileSync(files[0], 'utf8');
  assert.match(source, /Behavior on fillColor\s*\{\s*ColorAnimation/);
  assert.match(source, /fillColor: root\.down \? Theme\.surfacePressed/);
});

test('node health transitions and text selection gestures remain active', () => {
  const nodes = readFileSync(files[5], 'utf8');
  const text = readFileSync(files[6], 'utf8');
  assert.match(nodes, /Behavior on width\s*\{/);
  assert.match(nodes, /Behavior on fillColor\s*\{/);
  assert.match(text, /selectionHandleDragging = true/);
  assert.match(text, /borderWidth: 2/);
});

test('native miniapp backgrounds retain privacy surface and selected frame borders', () => {
  const android = readFileSync(files[2], 'utf8');
  const desktop = readFileSync(files[3], 'utf8');
  assert.match(android, /curveDepth: root\.miniAppFullscreen \? 0 : Theme\.scaleUi\(18\)/);
  assert.match(desktop, /curveDepth: root\.edge === 1 && !root\.expanded/);
  assert.match(android, /borderWidth: root\.miniAppFullscreen \? 0 : 1/);
  assert.match(desktop, /borderColor: Theme\.borderMuted/);
});
