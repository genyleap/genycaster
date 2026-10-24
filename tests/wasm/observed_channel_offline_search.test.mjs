import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');

test('canonical observed casts teach the local channel index without provider coupling', () => {
  const store = read('src/storage/read_store.cpp');
  const port = read('src/local_state/read.cppm');
  const cast = read('src/domain/cast.cppm');
  assert.match(store, /SqliteReadStore::rememberChannelsFromCasts/);
  assert.match(store, /rememberChannelsFromCasts\(std::span\{&cast, 1U\}\)/);
  assert.match(store, /rememberChannelsFromCasts\(std::span<const domain::cast::Cast>\{page.casts\}\)/);
  assert.match(store, /putChannelSuggestions\(batch\)/);
  assert.match(store, /cast->channelId/);
  assert.match(store, /!cast->parent \? safeOptional\(cast->parentUrl\)/);
  assert.match(store, /cached_channel_suggestions.name != cached_channel_suggestions.id/);
  assert.match(port, /searchCachedChannels/);
  assert.match(cast, /std::optional<std::string> channelId/);
  assert.doesNotMatch(port, /Hypersnap|Snapchain|Neynar|apiKey/);
});

test('native regression exercises sparse names, reply channels, feed batching and restart', () => {
  const tests = read('tests/unit/read_pipeline_tests.cpp');
  assert.match(tests, /observedCastChannelsRemainDiscoverableOffline\(\)/);
  assert.match(tests, /channelId = "replytools"/);
  assert.match(tests, /channelId = "local"/);
  assert.match(tests, /for \(int i = 0; i < 35; \+\+i\)/);
  assert.match(tests, /searchCachedChannels\("batch_34", 10\)/);
  assert.match(tests, /const auto offline = \(\*reopened\)->searchCachedChannels\("local", 10\)/);
});
