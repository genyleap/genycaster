import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');

test('raw Snapchain Follow graph has typed native provider-neutral capability routes', () => {
  const mod = read('src/providers/snapchain/read.cppm');
  const source = read('src/providers/snapchain/read.cpp');
  assert.match(mod, /decodeFollowLinks\(/);
  assert.match(source, /supported\(capability::catalog::graphFollowingRead\(\)\)/);
  assert.match(source, /supported\(capability::catalog::graphFollowersRead\(\)\)/);
  assert.match(source, /\/v1\/linksByFid\?fid=/);
  assert.match(source, /\/v1\/linksByTargetFid\?target_fid=/);
  assert.match(source, /linkBody/);
  assert.match(source, /targetFid/);
  assert.match(source, /raw-social-v1:/);
  assert.match(source, /record->providerCursor\.starts_with\(scope\)/);
  assert.match(source, /boundedDerived\(capability::catalog::feedFollowingRead\(\)\)/);
  assert.doesNotMatch(mod, /Hypersnap|Neynar|apiKey/);
});

test('async native tests reject mismatched source/direction/FID and preserve pagination identity', () => {
  const tests = read('tests/providers/snapchain/read_contract_tests.cpp');
  assert.match(tests, /testRawFollowGraphCodec\(\)/);
  assert.match(tests, /testRawSocialRequestAndCursorAffinity\(\)/);
  assert.match(tests, /wrongDirection/);
  assert.match(tests, /wrongFid/);
  assert.match(tests, /wrongEndpoint/);
  assert.match(tests, /pageToken=link%2B%2F%3D/);
});
