import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');

test('locally discovered channels have provider-neutral bounded indexed storage', () => {
  const port = read('src/local_state/read.cppm');
  const storage = read('src/storage/read_store.cpp');
  const schema = read('src/storage/sqlite_boundary.cpp');
  assert.match(port, /searchCachedChannels\(std::string_view query, std::size_t limit\)/);
  assert.match(port, /putChannelSuggestions\(/);
  assert.match(storage, /SqliteReadStore::searchCachedChannels/);
  assert.match(storage, /SqliteReadStore::putChannelSuggestions/);
  assert.match(storage, /sqlite::Transaction transaction\{m_metadata\}/);
  assert.match(storage, /cached_channel_suggestions\.parent_url/);
  assert.match(storage, /prefix\.push_back\('!'\)/);
  assert.match(schema, /CREATE TABLE IF NOT EXISTS cached_channel_suggestions/);
  assert.match(schema, /idx_cached_channels_name_prefix/);
  assert.doesNotMatch(port, /Hypersnap|Neynar|Snapchain|apiKey/);
});

test('both search pipelines preserve offline channels and cache network discoveries', () => {
  const asyncService = read('src/application/read_async_service.cpp');
  const syncService = read('src/application/read_service.cpp');
  const a = asyncService.slice(asyncService.indexOf('ReadService::openSearch('),
    asyncService.indexOf('ReadService::openNotifications('));
  const b = syncService.slice(syncService.indexOf('ReadService::openSearch('),
    syncService.indexOf('ReadService::openNotifications('));
  for (const code of [a, b]) {
    assert.match(code, /m_store\.searchCachedChannels\(query\.text, query\.limit\)/);
    assert.match(code, /putChannelSuggestions\(/);
    assert.match(code, /\.page\.channels/);
    assert.match(code, /failedMetadata\(/);
    assert.match(code, /ReadPhase::Ready/);
  }
  const tests = read('tests/unit/read_pipeline_tests.cpp');
  assert.match(tests, /cachedChannelSearchPersistsLiteralNamesAndSparseUpdates\(\)/);
  assert.match(tests, /asyncSearchRetainsOfflineChannelsAndCachesOnlineDiscovery\(\)/);
  assert.match(tests, /searchCachedChannels\("dev%", 8\)/);
  assert.match(tests, /searchCachedChannels\("dev_", 8\)/);
  assert.match(tests, /gateway\.unavailable = true/);
});

test('UI continues displaying the existing provider-neutral SearchPage channels', () => {
  const ui = read('ui/mobile/runtime_ui_bindings.cpp');
  assert.match(ui, /searchPresentation\(const application::read::model::SearchState& state\)/);
  assert.match(ui, /state\.page\.channels/);
  assert.match(ui, /applySearchSuggestions\(/);
});
