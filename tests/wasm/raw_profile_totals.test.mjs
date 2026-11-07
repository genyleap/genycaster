import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../',import.meta.url);
const read = name => readFileSync(new URL(name,root),'utf8');

test('raw UserData never fabricates graph totals and indexed totals require both fields',()=>{
  const raw=read('src/providers/snapchain/read.cpp');
  const indexed=read('src/providers/hypersnap/codec.cpp');
  assert.match(raw,/profile\.socialCountsKnown = false/);
  assert.match(indexed,/socialCountsKnown = followers\.has_value\(\) && following\.has_value\(\)/);
});

test('social-count availability remains canonical through local cache and UI projection',()=>{
  const store=read('src/storage/entity_store.cpp');
  const sqlite=read('src/storage/sqlite_boundary.cpp');
  const projection=read('ui/app/runtime/bindings.cpp');
  assert.match(store,/social_counts_known/);
  assert.match(store,/sqlite3_bind_int\(statement\.native\(\), 15, profile\.socialCountsKnown/);
  assert.match(store,/profile\.socialCountsKnown = sqlite3_column_int\(statement\.native\(\), 13\)/);
  assert.match(store,/profile\.socialCountsKnown = sqlite3_column_int\(statement\.native\(\), 14\)/);
  assert.match(sqlite,/recordMigration\(connection, 29, appliedAt\)/);
  assert.match(projection,/QStringLiteral\("socialCountsKnown"\)/);
});

test('profile, social rail, and compact account card show unknown instead of zero',()=>{
  const profile=read('ui/components/profile/ProfileSurface.qml');
  const rail=read('ui/components/navigation/SocialSidebar.qml');
  const side=read('ui/components/navigation/SideNavigation.qml');
  const card=read('ui/components/profile/ProfileTrustCard.qml');
  assert.match(profile,/activeDetail\.socialCountsKnown === false/);
  assert.match(rail,/shownProfile\.socialCountsKnown === false/);
  assert.match(read('ui/components/navigation/SearchPalette.qml'),/modelData\.socialCountsKnown === false/);
  assert.match(side,/profileDetail\.socialCountsKnown !== false/);
  assert.match(card,/root\.followers >= 0 \? LocaleFormat/);
  assert.match(card,/root\.following >= 0 \? LocaleFormat/);
  assert.match(read('tests/unit/storage_behavior.cpp'),/!\(\*unknown\)->socialCountsKnown/);
});
