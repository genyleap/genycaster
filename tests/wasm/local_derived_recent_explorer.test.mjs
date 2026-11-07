import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');

test('locally observed Recent is bounded, sorted, excludes tombstones and has no provider cursor', () => {
  const sqlite = read('src/storage/read_store.cpp');
  const schema = read('src/storage/sqlite_boundary.cpp');
  const localPort = read('src/local_state/read.cppm');
  assert.match(schema, /idx_casts_local_recent/);
  assert.match(sqlite, /SqliteReadStore::readDerivedRecentWindow/);
  assert.match(sqlite, /ORDER BY casts\.created_at_ms DESC, casts\.id DESC/);
  assert.match(sqlite, /cast_local_tombstones/);
  assert.match(sqlite, /const bool hasMore = entries\.size\(\) > limit/);
  assert.match(sqlite, /page->next\.reset\(\)/);
  assert.match(sqlite, /SqliteReadStore::hasMaterializedRecentSnapshot/);
  assert.match(sqlite, /if \(!\*materialized\) return readDerivedRecentWindow\(offset, limit\)/);
  assert.match(sqlite, /\.isDerivedRecent = true/);
  assert.match(localPort, /isDerivedRecent/);
  assert.doesNotMatch(localPort, /hypersnap|snapchain|neynar/i);
});

test('network Recent is authoritative; local pagination and UI mark partial coverage', () => {
  const storage = read('src/storage/read_store.cpp');
  const model = read('src/application/read_model.cppm');
  const asyncService = read('src/application/read_async_service.cpp');
  const presenter = read('src/presentation/qt/presentation_bridge.hpp');
  const binding = read('ui/app/runtime/bindings.cpp');
  const shell = read('ui/components/shell/AppShell.qml');
  const feed = read('ui/components/feed/FeedSurface.qml');
  assert.match(storage, /if \(key\.kind == local::read::FeedKey::Kind::Recent && window->entries\.empty\(\)\)/);
  assert.match(storage, /if \(first->entries\.empty\(\)\)/);
  assert.match(storage, /if \(!\*materialized\) return readDerivedRecentWindow\(offset, limit\)/);
  assert.match(model, /bool localDerivedRecent\{false\}/);
  assert.match(asyncService, /state\.localDerivedRecent = window\.isDerivedRecent/);
  assert.match(binding, /bridge\.setLocalDerivedRecent\(state\.localDerivedRecent\)/);
  assert.match(presenter, /Q_PROPERTY\(bool localDerivedRecent READ localDerivedRecent NOTIFY stateChanged\)/);
  assert.match(shell, /localDerivedRecent: presentationBridge\.localDerivedRecent/);
  assert.match(feed, /root\.localDerivedRecent && root\.activeFeed === "recent"/);

  const tests = read('tests/unit/read_pipeline_tests.cpp');
  assert.match(tests, /locallyDerivedRecentIsBoundedAndNeverImpersonatesNetworkRecent/);
  assert.match(tests, /offlineExplorerRecentUsesDerivedRowsWithoutNetworkCursor/);
  for (const lang of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                       'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
    const ts = read('ui/i18n/qml_' + lang + '.ts');
    assert.match(ts, /<message id="genycaster\.feed\.recent_local_only_notice">[\s\S]*?<translation>[^<]+<\/translation>/);
  }
});
