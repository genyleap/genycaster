import { readFileSync } from 'node:fs';
import { strict as assert } from 'node:assert';
import { test } from 'node:test';
const root = new URL('../../', import.meta.url);
const read = (path) => readFileSync(new URL(path, root), 'utf8');

test('cast share is the right-most footer action and has native and web targets', () => {
  const bar = read('ui/components/feed/EngagementBar.qml');
  assert.ok(bar.indexOf('id: castShareButton') > bar.indexOf('id: bookmarkButton'));
  assert.match(bar, /presentationBridge\.shareUrl\(presentationBridge\.castShareUrl/);
  assert.match(bar, /presentationBridge\.farcasterWebCastUrl/);
  assert.match(bar, /presentationBridge\.openFarcasterCast/);
  assert.match(bar, /presentationBridge\.openQuorumCast/);
  assert.doesNotMatch(bar, /presentationBridge\.quorumCastUri/);
  assert.match(bar, /id: castShareMenu[\s\S]*?active: false/);
});

test('cast context menu copies the cast permalink, not an attached image or website', () => {
  const cast = read('ui/components/feed/CastItem.qml');
  assert.ok(cast.includes('readonly property string castPermalink: presentationBridge.castShareUrl('));
  assert.ok(cast.includes('CastViews.normalizeHash(root.modelData.castId || "")'));
  const menu = read('ui/components/feed/CastActionsMenu.qml');
  assert.ok(cast.includes('CastActionsMenu {'));
  assert.ok(menu.includes('CastViews.normalizeHash(root.castData.castId || "")'));
  const menuStart = menu.indexOf('text: qsTrId("genycaster.cast.copy_link")');
  assert.ok(menuStart > 0);
  const menuAction = menu.slice(menuStart, menuStart + 250);
  assert.ok(menuAction.includes('presentationBridge.copyText(root.castPermalink)'));
  assert.ok(menu.includes('visible: root.castPermalink.length > 0'));
  assert.ok(!menu.includes('presentationBridge.copyText(root.firstExternalUrl)'));
});

test('profile sharing appears on the shared profile surface', () => {
  const profile = read('ui/components/profile/ProfileSurface.qml');
  assert.match(profile, /id: profileShareButton/);
  assert.match(profile, /profileShareUrl/);
  assert.match(profile, /farcasterWebProfileUrl/);
  assert.match(profile, /openFarcasterProfile/);
});

test('profile moderation uses signed Block while Mute remains client-account scoped', () => {
  const cast = read('ui/components/feed/CastItem.qml');
  const profile = read('ui/components/profile/ProfileSurface.qml');
  for (const view of [cast, profile]) {
    assert.doesNotMatch(view, /authorMuted|authorBlocked|setAuthorMuted|setAuthorBlocked/);
    assert.doesNotMatch(view, /Copy report reference|Mute locally|Block locally|Mute author locally|Block author locally/);
  }
  assert.doesNotMatch(cast, /text: qsTr\("Bookmark cast"\)|qsTr\("Remove bookmark"\)/);
  assert.match(read('ui/components/feed/EngagementBar.qml'), /id: bookmarkButton/);
  const bridge = read('src/presentation/qt/presentation_bridge.cpp');
  assert.match(bridge, /QVariantList PresentationBridge::rows\(\) const \{ return m_rows; \}/);
  assert.doesNotMatch(bridge, /moderation\/%1\/mute|moderation\/%1\/block/);
  assert.match(profile,/onClicked: \{[\s\S]*root\.blockingMutationRequested\(/);
  assert.match(read('ui/components/shell/AppShell.qml'),/onBlockingMutationRequested/);
});

test('mobile footer has tightly grouped reactions and always-visible trailing bookmark/share', () => {
  const bar = read('ui/components/feed/EngagementBar.qml');
  assert.match(bar, /id: reactionActions/);
  assert.match(bar, /id: utilityActions/);
  assert.match(bar, /id: bookmarkButton[\s\S]*?Layout\.minimumWidth: Theme\.touchTarget/);
  assert.doesNotMatch(bar, /visible: !root\.compact/);
  assert.match(bar, /enabled: root\.normalizedCastId\.length > 0/);
  assert.match(bar, /normalizedCastId: CastViews\.normalizeHash\(root\.viewDescriptor\.castId \|\| root\.castId\)/);
  assert.doesNotMatch(bar, /castShareUrl\(root\.castId\)/);
  assert.match(bar, /castShareUrl\(root\.normalizedCastId\)/);
  assert.match(bar, /openQuorumCast\(root\.normalizedCastId\)/);
  assert.match(bar, /Item \{ Layout\.fillWidth: true; Layout\.minimumWidth: 0 \}/);
});

test('public Genycaster links use runtime hosting origin or native configuration', () => {
  const bridge = read('src/presentation/qt/presentation_bridge.cpp');
  const qml = read('ui/components/feed/EngagementBar.qml');
  const cmake = read('CMakeLists.txt');
  assert.ok(bridge.includes('window.location.origin'));
  assert.ok(bridge.includes('GENYCASTER_PUBLIC_BASE_URL'));
  assert.ok(bridge.includes('sharing/publicBaseUrl'));
  assert.ok(cmake.includes('GENYCASTER_DEFAULT_PUBLIC_BASE_URL'));
  assert.ok(bridge.includes('publicShareBaseUrl()'));
  assert.ok(bridge.includes('base.host()'));
  assert.ok(!bridge.includes('caster.genyleap.com'));
  assert.ok(qml.includes('castShareUrl(root.normalizedCastId)'));
});

test('incoming browser and command-line URLs route after runtime is ready', () => {
  const main = read('ui/app/main.cpp');
  assert.match(main, /--open-url=/);
  assert.match(main, /window\.location\.href/);
  assert.match(main, /presentationBridge\.openSharedLink\(incoming\)/);
  assert.match(main, /presentationBridge\.runtimeReady\(\)/);
});

test('app attribution requires node-returned provenance', () => {
  const codec = read('src/providers/hypersnap/codec.cpp');
  assert.match(codec, /findValue\(castObject, "app"\)/);
  assert.match(codec, /cast\.sourceClient = std::move\(\*name\)/);
  const feed = read('ui/app/runtime/bindings.cpp');
  assert.match(feed, /"sourceClient"/);
  assert.match(read('ui/components/feed/CastItem.qml'), /objectName: "castSourceClientBadge"/);
});
