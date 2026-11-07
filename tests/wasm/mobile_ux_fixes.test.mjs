import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');

test('search palette can always close and channel selection routes to reading surface', () => {
  const palette = read('ui/components/navigation/SearchPalette.qml');
  const shell = read('ui/components/shell/AppShell.qml');
  const header = read('ui/components/feed/FeedHeader.qml');
  assert.match(palette, /objectName: "closeSearchPalette"/);
  assert.match(palette, /onClicked: root\.close\(\)/);
  assert.match(palette, /signal channelRequested\(string channelId, var channelInfo\)/);
  assert.match(palette, /root\.channelRequested\(channelId, modelData\)/);
  assert.match(shell, /onChannelRequested: \(channelId, channelInfo\) => \{/);
  assert.match(shell, /root\.primaryDestination = "explorer"\s+root\.shellRoute = ""\s+presentationBridge\.openChannel/);
  assert.match(header, /property var channelInfo/);
  assert.match(header, /root\.channelInfo\.description/);
});
test('reply actions use the Cast menu pattern and Delete keeps a danger accent', () => {
  const reply = read('ui/components/feed/ThreadReplyItem.qml');
  const button = read('ui/components/controls/ActionButton.qml');
  const shell = read('ui/components/shell/AppShell.qml');
  assert.match(reply, /objectName: "replyMoreButton"/);
  assert.match(reply, /replyMenuLoader/);
  assert.match(reply, /root\.removeCastRequested\(root\.reply\)/);
  assert.doesNotMatch(reply, /objectName: "deleteOwnedReply"/);
  assert.match(button, /foregroundColor: destructiveFilled \? Theme\.dangerInk/);
  assert.match(button, /: destructive \? Theme\.danger/);
  assert.match(shell, /destructiveFilled: true/);
});
test('WebAssembly market cards use bounded data, a compact-first layout and an explicit failure state', () => {
  const card = read('ui/components/content/TokenAddressPreview.qml');
  const gateway = read('services/media-gateway/gateway.mjs');
  assert.match(card, /property bool expanded: false/);
  assert.match(card, /objectName: "expandTokenDetails"/);
  assert.match(card, /requestBrowserToken\(\)/);
  assert.match(card, /tokenError = "Market data unavailable"/);
  assert.match(gateway, /requestUrl\.pathname === '\/v1\/browser\/tokens'/);
  assert.match(gateway, /publicTokenSnapshot\(chain, address/);
});
test('browser scroll optimizations do not replace Qt default scene graph backend', () => {
  const backdrop = read('ui/components/visual/DrawerBackdrop.qml');
  const feed = read('ui/components/feed/FeedSurface.qml');
  assert.match(backdrop, /browserRenderer: Qt\.platform\.os === "wasm"/);
  assert.match(backdrop, /!root\.browserRenderer/);
  assert.match(feed, /cacheBuffer: root\.compact \?/);
  assert.doesNotMatch(backdrop, /QSG_RHI_BACKEND/);
});
test('recovery does not re-prompt an already authorized signer or pretend a stored phrase exists', () => {
  const settings = read('ui/components/settings/SettingsPopup.qml');
  assert.match(settings, /visible: presentationBridge\.isAuthenticated && !presentationBridge\.writeReady/);
  assert.match(settings, /genycaster.setup.recovery_phrase_unavailable/);
  assert.match(settings, /objectName: "settingsRecoveryExplanation"/);
});
