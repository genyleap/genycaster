import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('feed and profile headers keep channel after author name without a third mobile row', () => {
  const feed = read('ui/components/CastItem.qml');
  const identity = feed.slice(feed.indexOf('id: castAuthorIdentity'), feed.indexOf('id: moreButton'));
  assert.ok(identity.includes('id: castNameAndChannel'));
  assert.ok(identity.includes('id: castInlineChannel'));
  assert.ok(identity.includes('text: root.authorName'));
  assert.ok(identity.includes('sourceUrl: root.channelImageUrl'));
  assert.ok(identity.includes('text: root.channelDisplayName'));
  assert.ok(identity.includes('text: root.ltrHandle'));
  assert.ok(identity.includes('objectName: "compactCastAuthorMetadata"'));
  assert.ok(identity.indexOf('text: root.authorName') < identity.indexOf('id: castInlineChannel'));
  assert.ok(identity.indexOf('id: castInlineChannel') < identity.indexOf('text: root.ltrHandle'));
  assert.ok(identity.indexOf('text: root.ltrHandle') < identity.indexOf('compactCastAuthorMetadata'));
  assert.doesNotMatch(feed, /Keep channel attribution separate from the two-line identity/);
});

test('cast detail uses same first-line channel layout and second-line handle', () => {
  const detail = read('ui/components/DetailSurface.qml');
  const header = detail.slice(detail.indexOf('id: detailNameAndChannel'), detail.indexOf('// Timestamp belongs with the author identity'));
  assert.ok(header.includes('text: root.resolvedChannelName'));
  assert.ok(header.includes('text: root.ltrHandle(root.detail.username)'));
  assert.ok(header.indexOf('text: root.resolvedChannelName') < header.indexOf('text: root.ltrHandle(root.detail.username)'));
});

test('sparse search casts receive exactly one bounded provider-local bulk hydration, keeping signed text intact', () => {
  const code = read('src/providers/hypersnap/async_gateway.cpp');
  const merge = code.slice(code.indexOf('void mergeHydratedCastPresentation'),code.indexOf('void mergeHydratedReplyMetrics'));
  assert.match(merge, /canonicalId\(cast\.id\.value\)/);
  assert.match(merge, /cast\.text == full\.text && full\.displayText/);
  assert.match(merge, /cast\.displayText = full\.displayText/);
  assert.doesNotMatch(merge, /cast\.text\s*=(?!=)/);
  assert.match(code, /spec\.path == "\/v2\/farcaster\/cast\/search"/);
  assert.match(code, /joinedCastHashes\(page\.casts, 25U\)/);
  assert.match(code, /joinedMissingMentionHashes\(page\.casts\)/);
  assert.match(code, /\/v2\/farcaster\/cast\/bulk/);
  assert.match(code, /std::chrono::milliseconds\{1'800\}/);
  assert.match(code, /finishPage\(std::move\(page\)\)/);
});

test('real emoji-X cast sample proves the hydrated zero-width mention renders in preserved cast text', () => {
  const fixture = read('tests/providers/hypersnap/contract_tests.cpp');
  assert.match(fixture, /2113262913767317648/);
  assert.match(fixture, /"start":12,"end":12/);
  assert.match(fixture, /"fid":280,"username":"vrypan.eth"/);
  assert.match(fixture, /Right! 💯 @vrypan\.eth/);
  assert.match(read('ui/components/CastItem.qml'), /\[@#\]\[a-z0-9_\]/);
  assert.match(read('ui/components/AppShell.qml'), /presentationBridge\.openProfileUsername\(target\.substring\(1\)\)/);
});
