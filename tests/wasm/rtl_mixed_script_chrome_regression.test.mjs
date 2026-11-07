import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = name => readFileSync(new URL('../../' + name, import.meta.url), 'utf8');
test('direction-aware labels separate interface alignment from content script direction', () => {
  const label = read('ui/components/controls/BidiLabel.qml');
  assert.match(label, /property bool alignToUi: false/);
  assert.match(label, /horizontalAlignment: alignToUi \? Theme\.textStartAlignment/);
  assert.match(label, /localizationController\.isRightToLeftText\(analyzedText\)/);
});
test('bookmark author layout mirrors, while saved cast body preserves natural text direction', () => {
  const q = read('ui/components/profile/BookmarksSurface.qml');
  assert.match(q, /CastItem \{/);
  assert.match(q, /modelData: bookmarkCard.modelData/);
  const cast = read('ui/components/feed/CastItem.qml');
  assert.match(cast, /readonly property bool contentRtl: localizationController.isRightToLeftText/);
  assert.match(cast, /RichCastRenderer \{/);
});
test('provider and node names follow UI edge regardless of Latin content', () => {
  const connections = read('ui/components/profile/ConnectionsPopup.qml');
  const nodes = read('ui/components/settings/NodeManagementPanel.qml');
  const composer = read('ui/components/composer/ComposerDrawer.qml');
  assert.match(connections, /text: picker\.providerTitle\(providerRow\.modelData\)\s*alignToUi: true/);
  assert.match(connections, /copy_saved_link_1"\)\.arg\("\\u2066" \+ nick \+ "\\u2069"\)/);
  assert.match(nodes, /text: String\(modelData\.name\)\s*alignToUi: true/);
  assert.match(nodes, /text: String\(modelData\.label\)\s*alignToUi: true/);
  assert.match(composer, /text: channelRow\.isHome[\s\S]*?alignToUi: true/);
});
test('homepage promotions and greetings preserve mixed-script chrome', () => {
  const home = read('ui/components/home/HomeDashboard.qml');
  const profile = read('ui/components/profile/ProfileSurface.qml');
  const fa = read('ui/i18n/qml_fa_IR.ts');
  assert.match(home, /genycaster\.home\.geny_token_title/);
  assert.match(home, /"\\u2066" \+ root\.welcomeName \+ "\\u2069"/);
  assert.match(profile, /objectName: "profileProfessionalCategory"[\s\S]*?layoutDirection: Theme\.uiDirection/);
  assert.match(fa, /<translation>یک کست یا پیام جدید بنویسید…<\/translation>/);
  assert.match(fa, /<translation>حساب‌های متصل خود را در جنی‌کستر/);
});