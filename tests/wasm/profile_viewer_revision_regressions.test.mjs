import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { runInNewContext } from 'node:vm';

const qml = readFileSync(new URL('../../ui/components/ProfileSurface.qml', import.meta.url), 'utf8');
const start = qml.indexOf('    function rowRevision(row) {');
const end = qml.indexOf('\n    function rowIncubationHeight(', start);
assert.ok(start >= 0 && end > start, 'real ProfileSurface rowRevision is available');
const rowRevision = runInNewContext('(' + qml.slice(start, end).trim() + ')', {});

test('profile cached viewer reactions distinguish unknown, verified no and yes', () => {
  const cast = { castId: '0xprofile', _revision: 'cast-1', likeCount: 4, recastCount: 2 };
  const unknown = rowRevision(cast);
  const likeNo = rowRevision({ ...cast, likedByMe: false });
  const likeYes = rowRevision({ ...cast, likedByMe: true });
  const recastNo = rowRevision({ ...cast, recastedByMe: false });
  const recastYes = rowRevision({ ...cast, recastedByMe: true });
  assert.equal(unknown, rowRevision({ ...cast }));
  for (const value of [likeNo, likeYes, recastNo, recastYes]) assert.notEqual(unknown, value);
  assert.notEqual(likeNo, likeYes);
  assert.notEqual(recastNo, recastYes);
  assert.equal(unknown, rowRevision({ ...cast, likedByMe: null }));
});

test('profile feed Follow and Follows-me hydrate and invalidate on account switch', () => {
  const cast = { castId: '0xprofile', _revision: 'cast-1' };
  const unknown = rowRevision(cast);
  assert.equal(unknown, rowRevision({
    ...cast, followingKnown: false, followingByMe: true, followsMeKnown: false, followsMe: true,
  }));
  const noFollow = rowRevision({ ...cast, followingKnown: true, followingByMe: false });
  const yesFollow = rowRevision({ ...cast, followingKnown: true, followingByMe: true });
  const noFollowsMe = rowRevision({ ...cast, followsMeKnown: true, followsMe: false });
  const yesFollowsMe = rowRevision({ ...cast, followsMeKnown: true, followsMe: true });
  for (const rev of [noFollow, yesFollow, noFollowsMe, yesFollowsMe]) assert.notEqual(unknown, rev);
  assert.notEqual(noFollow, yesFollow);
  assert.notEqual(noFollowsMe, yesFollowsMe);
  assert.equal(unknown, rowRevision({ ...cast })); // viewer scrubbed
});

test('profile quote hydration, public counts and write states remain independently revisioned', () => {
  const cast = { castId: '0xprofile', _revision: 'text', likeCount: 3 };
  const initial = rowRevision(cast);
  assert.notEqual(initial, rowRevision({ ...cast, _referencedRevision: 'source-confirmed' }));
  assert.notEqual(initial, rowRevision({ ...cast, likeCount: 4 }));
  assert.notEqual(initial, rowRevision({ ...cast, writeState: 'queued' }));
  assert.notEqual(initial, rowRevision({ ...cast, writeState: 'confirmed' }));
  assert.equal(initial, rowRevision({ ...cast, unrelatedOptionalToken: 'never-structural' }));
});
