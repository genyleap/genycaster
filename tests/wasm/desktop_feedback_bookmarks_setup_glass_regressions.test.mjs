import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../'+path, import.meta.url), 'utf8');

test('feedback controls consume clicks and use a smiling face entry icon', () => {
  const dialog = read('ui/components/FeedbackDialog.qml');
  const theme = read('ui/theme/Theme.qml');
  const header = read('ui/components/AppHeader.qml');
  assert.ok(theme.includes('iconFeedback: "social/smile"'));
  assert.ok(header.includes('iconGlyph: Theme.iconFeedback'));
  assert.equal((dialog.match(/delegate: Button \{/g) || []).length, 3);
  assert.ok(!dialog.includes('TapHandler {'));
  assert.ok(dialog.includes('onClicked: root.sentiment = moodButton.modelData.value'));
  assert.ok(dialog.includes('onClicked: root.category = categoryCard.modelData.id'));
});

test('bookmark cards have even horizontal and vertical inset', () => {
  const bookmarks = read('ui/components/BookmarksSurface.qml');
  assert.ok(bookmarks.includes('- Theme.space3 * 2'));
  assert.ok(bookmarks.includes('Layout.margins: Theme.space3'));
});

test('desktop sidebar stays opaque while the mobile dock retains its blur', () => {
  const nav = read('ui/components/SideNavigation.qml');
  const dock = read('ui/components/BottomNavigation.qml');
  assert.ok(nav.includes('fillColor: Theme.surfacePrimary'));
  assert.ok(!nav.includes('desktopNavigationGlassBlur'));
  assert.ok(dock.includes('mobileCapsuleBackdropCapture'));
  assert.ok(dock.includes('blurSourceItem'));
});

test('returning authenticated users clear the temporary post-logout setup gate', () => {
  const bridge = read('src/presentation/qt/presentation_bridge.cpp');
  const unit = read('tests/unit/presentation_icon_tests.cpp');
  assert.ok(bridge.includes('m_isAuthenticated && m_initialSetupCompleted && m_showSetupAfterSignOut'));
  assert.ok(bridge.includes('m_settings->remove(QStringLiteral("setup/showAfterSignOut"))'));
  assert.ok(unit.includes('A returning verified account clears the temporary post-logout Setup'));
  assert.ok(unit.includes('assert(!afterSignOut.initialSetupRequired())'));
});
