import { readFileSync } from 'node:fs';
import { strict as assert } from 'node:assert';
import { test } from 'node:test';
const root = new URL('../../', import.meta.url);
const read = (path) => readFileSync(new URL(path, root), 'utf8');

test('cast share is the right-most footer action and has native and web targets', () => {
  const bar = read('ui/components/EngagementBar.qml');
  assert.ok(bar.indexOf('id: castShareButton') > bar.indexOf('id: bookmarkButton'));
  assert.match(bar, /presentationBridge\.shareUrl\(presentationBridge\.castShareUrl/);
  assert.match(bar, /presentationBridge\.farcasterWebCastUrl/);
  assert.match(bar, /presentationBridge\.farcasterCastUri/);
  assert.match(bar, /presentationBridge\.openQuorumCast/);
  assert.match(bar, /presentationBridge\.quorumCastUri/);
  assert.match(bar, /id: castShareMenu[\s\S]*?active: false/);
});

test('profile sharing appears on the shared profile surface', () => {
  const profile = read('ui/components/ProfileSurface.qml');
  assert.match(profile, /id: profileShareButton/);
  assert.match(profile, /profileShareUrl/);
  assert.match(profile, /farcasterWebProfileUrl/);
  assert.match(profile, /farcasterProfileUri/);
});

test('unfinished moderation stays out of cast/profile menus and does not filter feed rows', () => {
  const cast = read('ui/components/CastItem.qml');
  const profile = read('ui/components/ProfileSurface.qml');
  for (const view of [cast, profile]) {
    assert.doesNotMatch(view, /authorMuted|authorBlocked|setAuthorMuted|setAuthorBlocked/);
    assert.doesNotMatch(view, /Copy report reference|Mute locally|Block locally|Mute author locally|Block author locally/);
  }
  assert.doesNotMatch(cast, /text: qsTr\("Bookmark cast"\)|qsTr\("Remove bookmark"\)/);
  assert.match(read('ui/components/EngagementBar.qml'), /id: bookmarkButton/);
  const bridge = read('src/presentation/qt/presentation_bridge.cpp');
  assert.match(bridge, /QVariantList PresentationBridge::rows\(\) const \{ return m_rows; \}/);
  assert.doesNotMatch(bridge, /moderation\/%1\/mute|moderation\/%1\/block/);
});

test('mobile footer has tightly grouped reactions and always-visible trailing bookmark/share', () => {
  const bar = read('ui/components/EngagementBar.qml');
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
  const qml = read('ui/components/EngagementBar.qml');
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
  const main = read('ui/mobile/main.cpp');
  assert.match(main, /--open-url=/);
  assert.match(main, /window\.location\.href/);
  assert.match(main, /presentationBridge\.openSharedLink\(incoming\)/);
  assert.match(main, /presentationBridge\.runtimeReady\(\)/);
});

test('app attribution requires node-returned provenance', () => {
  const codec = read('src/providers/hypersnap/codec.cpp');
  assert.match(codec, /findValue\(castObject, "app"\)/);
  assert.match(codec, /cast\.sourceClient = std::move\(\*name\)/);
  const feed = read('ui/mobile/runtime_ui_bindings.cpp');
  assert.match(feed, /"sourceClient"/);
  assert.match(read('ui/components/CastItem.qml'), /objectName: "castSourceClientBadge"/);
});
