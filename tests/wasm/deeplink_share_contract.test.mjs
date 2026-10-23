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

test('profile and cast moderation are explicitly local and report is not falsely submitted', () => {
  const cast = read('ui/components/CastItem.qml');
  assert.match(cast, /Mute author locally/);
  assert.match(cast, /Block author locally/);
  assert.match(cast, /Copy report reference/);
  assert.doesNotMatch(cast, /submitReport\(/);
  const bridge = read('src/presentation/qt/presentation_bridge.cpp');
  assert.match(bridge, /moderation\/%1\/mute/);
  assert.match(bridge, /moderation\/%1\/block/);
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
