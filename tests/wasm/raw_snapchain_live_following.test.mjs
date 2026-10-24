import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const root = new URL('../../', import.meta.url);
const load = file => readFileSync(new URL(file, root), 'utf8');

test('raw Following derives bounded protocol graph and merges casts without a hosted index', () => {
  const source = load('src/providers/snapchain/read.cpp');
  const module = load('src/providers/snapchain/read.cppm');
  assert.match(source, /boundedDerived\(capability::catalog::feedFollowingRead\(\)\)/);
  assert.match(source, /CapabilityOrigin::Derived/);
  assert.match(source, /CapabilityQuality::Derived/);
  assert.match(source, /\/v1\/linksByFid\?fid=/);
  assert.match(source, /graphPageSize = 100U/);
  assert.match(source, /maximumFollowees = 256U/);
  assert.match(source, /maximumAuthorPages = 8U/);
  assert.match(source, /\/v1\/castsByFid\?fid=/);
  assert.match(source, /&pageSize=100&reverse=1/);
  assert.match(source, /self->authors\.size\(\) > maximumFollowees \+ 1U/);
  assert.match(source, /self->candidates\.size\(\) <= self->limit/);
  assert.match(source, /self->lastAuthorTimestamp >= self->candidates\.back\(\)\.createdAt/);
  assert.match(source, /cast->parent/);
  assert.match(source, /raw-follow-v1:/);
  assert.match(source, /capability::catalog::feedFollowingRead\(\),\s*providerId/);
  assert.match(source, /std::binary_search\(/);
  assert.match(module, /rejects the request instead of publishing an incomplete live feed/);
  assert.doesNotMatch(module, /Hypersnap|Neynar|apiKey/);
});

test('native regression proves keyset paging, source affinity, graph breadth and cancellation', () => {
  const native = load('tests/providers/snapchain/read_contract_tests.cpp');
  assert.match(native, /testBoundedLiveFollowingFromRawNode\(\)/);
  for (const part of ['graphTooLarge','graphAtLimit','graphTwoPages','authorTwoPages',
                       'authorHistoryIncomplete','wrongFid',
                       'wrongLimit','wrongNode','pending->cancel\(\)']) {
    assert.match(native, new RegExp(part));
  }
  assert.match(load('tools/check_bounty_read_node.py'), /"link_type": "follow"/);
  assert.match(load('tools/check_bounty_read_node.py'), /"target_fid": fid/);
});
