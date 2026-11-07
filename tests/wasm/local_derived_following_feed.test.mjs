import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');

test('observed Following is account-scoped, deduplicated and local-only', () => {
  const storage = read('src/storage/read_store.cpp');
  const port = read('src/local_state/read.cppm');
  const model = read('src/application/read_model.cppm');
  const svc = read('src/application/read_async_service.cpp');
  assert.match(storage, /SqliteReadStore::canDeriveFollowing/);
  assert.match(storage, /SqliteReadStore::readDerivedFollowingWindow/);
  assert.match(storage, /links\.list_id = \? AND links\.source_fid = \?/);
  assert.match(storage, /links\.target_fid = casts\.author_fid/);
  assert.match(storage, /casts\.parent_id IS NULL/);
  assert.match(storage, /cast_local_tombstones/);
  assert.match(storage, /ORDER BY casts\.created_at_ms DESC, casts\.id DESC/);
  assert.match(storage, /const bool hasMore = entries\.size\(\) > limit/);
  assert.match(storage, /page->next\.reset\(\)/);
  assert.match(storage, /\.isDerivedFollowing = true/);
  assert.match(port, /isDerivedFollowing/);
  assert.match(model, /localDerivedFollowing/);
  assert.match(svc, /state\.localDerivedFollowing = window\.isDerivedFollowing/);
  assert.doesNotMatch(port, /Hypersnap|Neynar|Snapchain|apiKey/);
});

test('Qt bridge and all locales visibly mark incomplete local following', () => {
  const bridge = read('src/presentation/qt/presentation_bridge.hpp');
  const appShell = read('ui/components/shell/AppShell.qml');
  const feed = read('ui/components/feed/FeedSurface.qml');
  const bindings = read('ui/app/runtime/bindings.cpp');
  assert.match(bridge, /Q_PROPERTY\(bool localDerivedFollowing READ localDerivedFollowing/);
  assert.match(bindings, /bridge\.setLocalDerivedFollowing\(state\.localDerivedFollowing\)/);
  assert.match(appShell, /localDerivedFollowing: presentationBridge\.localDerivedFollowing/);
  assert.match(feed, /root\.localDerivedFollowing && root\.activeFeed === "following"/);
  assert.match(feed, /qsTrId\("genycaster\.feed\.following_local_only_notice"\)/);
  for (const language of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                           'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
    assert.match(read('ui/i18n/qml_' + language + '.ts'),
      /<message id="genycaster\.feed\.following_local_only_notice">[\s\S]*?<translation>[^<]+<\/translation>/);
  }
  assert.match(read('tests/unit/read_pipeline_tests.cpp'),
    /locallyDerivedFollowingIsScopedBoundedAndNeverClaimsLiveFeed\(\)/);
});
