import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');

test('checkpoint storage uses a source-neutral immediate SQLite transaction', () => {
  const store = read('src/storage/checkpoint_store.cpp');
  const schema = read('src/storage/sqlite_boundary.cpp');
  const port = read('src/sync/checkpoint_store.cppm');
  assert.match(store, /sqlite::Transaction transaction\{m_impl->connection\}/);
  assert.match(store, /transaction\.begin\(\)/);
  assert.match(store, /loadCheckpointLocked\(key\)/);
  assert.match(store, /checkpoint\.sequence < \(\*current\)->sequence/);
  assert.match(store, /checkpoint\.sequence == \(\*current\)->sequence/);
  assert.match(store, /ErrorCode::Conflict/);
  assert.match(store, /maximumSequence/);
  assert.match(store, /signedSequence < 0/);
  assert.match(schema, /CREATE TABLE IF NOT EXISTS sync_checkpoints/);
  assert.match(schema, /"BEGIN IMMEDIATE;"/);
  assert.doesNotMatch(port, /Hypersnap|Neynar|Snapchain|apiKey|providerCursor/);
});

test('native regression covers multi-connection races, idempotent resume and crash recovery', () => {
  const tests = read('tests/unit/storage_behavior.cpp');
  assert.match(tests, /std::barrier gate\{2\}/);
  assert.match(tests, /std::thread lower/);
  assert.match(tests, /std::thread higher/);
  assert.match(tests, /stale\.error\(\)\.code == foundation::error::ErrorCode::Conflict/);
  assert.match(tests, /sqliteMaximum \+ 1U/);
  assert.match(tests, /secondStore\.reset\(\)/);
  assert.match(tests, /const sync::model::SyncKey otherSource/);
});
