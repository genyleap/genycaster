import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = name => readFileSync(new URL('../../'+name, import.meta.url),'utf8');

test('SQLite v26 migrates existing caches without overwriting signed CastAdd text',()=>{
 const schema=read('src/storage/sqlite_boundary.cpp');
 assert.match(schema, /display_text TEXT/);
 assert.match(schema, /tableHasColumn\(connection, "casts", "display_text"\)/);
 assert.match(schema, /ALTER TABLE casts ADD COLUMN display_text TEXT/);
 assert.match(schema, /recordMigration\(connection, 26, appliedAt\)/);
});
test('provider-rendered @mentions survive entity-store upserts and roundtrip through feed/cache',()=>{
 const store=read('src/storage/entity_store.cpp');
 assert.match(store, /INSERT INTO casts\([^)]*display_text[^)]*\)/);
 assert.match(store, /display_text = COALESCE\(excluded\.display_text, casts\.display_text\)/);
 assert.match(store, /bindOptionalText\(castStatement\.native\(\), 10, cast\.displayText\)/);
 assert.match(store, /SELECT c\.author_fid, c\.text,[\s\S]*?c\.display_text/);
 assert.match(store, /cast\.displayText = sqlite::optionalColumnText\(statement\.native\(\), 8\)/);
 const contract=read('tests/unit/storage_behavior.cpp');
 assert.match(contract, /Right! 💯 @vrypan\.eth/);
 assert.match(contract, /sparse\.displayText\.reset\(\)/);
 assert.match(contract, /\(\*warmed\)->displayText == cast\.displayText/);
 const readService=read('src/application/read_async_service.cpp');
 assert.match(readService, /auto materialized = readLocalPresentationWindow/);
 const presentation=read('ui/app/runtime_ui_bindings.cpp');
 assert.match(presentation, /cast\.displayText\.value_or\(cast\.text\)/);
});
test('ENS-like profile handles use exact-match search fallback on the currently configured provider',()=>{
 const identity=read('src/providers/hypersnap/async_gateway.cpp');
 assert.match(identity, /\/v2\/farcaster\/user\/by-username/);
 assert.match(identity, /\/v2\/farcaster\/user\/search/);
 assert.match(identity, /actual\.size\(\) == requested\.size\(\)/);
 assert.match(identity, /std::ranges::find_if\(\*profiles, sameName\)/);
 assert.match(read('ui/components/AppShell.qml'), /presentationBridge\.openProfileUsername/);
});
