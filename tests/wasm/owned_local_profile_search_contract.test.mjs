import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');

test('local profile search belongs to Genycaster SQLite cache, not an indexed vendor', () => {
    const api = read('src/local_state/read.cppm');
    const implementation = read('src/storage/read_store.cpp');
    const schema = read('src/storage/sqlite_boundary.cpp');
    assert.match(api, /searchCachedProfiles\(std::string_view query, std::size_t limit\)/);
    assert.match(implementation, /SqliteReadStore::searchCachedProfiles/);
    assert.match(implementation, /sqlite::bindText\(statement.native\(\), index, value\)/);
    assert.match(implementation, /pattern\.push_back\('!'\)/);
    assert.match(schema, /idx_profiles_username_prefix/);
    assert.match(schema, /idx_profiles_display_name_prefix/);
    assert.doesNotMatch(api, /hypersnap|neynar|snapchain/i);
    assert.doesNotMatch(read('src/domain/profile.cppm'), /searchCachedProfiles|providerCursor/);
});

test('interactive search publishes local matches and preserves them on two failure modes', () => {
    const service = read('src/application/read_async_service.cpp');
    const section = service.slice(service.indexOf('ReadService::openSearch('), service.indexOf('ReadService::openNotifications('));
    assert.match(section, /m_store\.searchCachedProfiles\(query.text, query.limit\)/);
    assert.match(section, /refreshingMetadata\(hasCache\)/);
    assert.match(section, /m_gateway\.search/);
    assert.match(section, /store->putProfileSummary\(profile\)/);
    assert.match(section, /m_cacheExecutor\.post/);
    assert.match(section, /state\.page\.profiles = std::move\(cached\)/);
    assert.match(section, /failedMetadata\(std::move\(fresh.error\(\)\), hasCache\)/);
    assert.match(section, /failedMetadata\(std::move\(request.error\(\)\), hasCache\)/);
    const tests = read('tests/unit/read_pipeline_tests.cpp');
    assert.match(tests, /cachedProfileIndexSurvivesRestartAndKeepsLiteralQueries/);
    assert.match(tests, /asyncSearchKeepsCachedIdentitiesWhenIndexIsUnavailable/);
    assert.match(tests, /searchCachedProfiles\("dev_", 8\)/);
    assert.match(tests, /searchCachedProfiles\("dev%", 8\)/);
});
