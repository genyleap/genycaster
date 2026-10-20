import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { runInNewContext } from 'node:vm';

const source = readFileSync(new URL('../../ui/components/FeedSurface.qml', import.meta.url), 'utf8');
const start = source.indexOf('    function rowRevision(row) {');
const end = source.indexOf('\n    function payloadForStableKey(key) {', start);
assert.ok(start >= 0 && end > start, 'extract actual FeedSurface QML function');
const qmlFunction = source.slice(start, end).trim();

function revision(row, socialMode = false) {
  const context = { root: { socialMode } };
  const rowRevision = runInNewContext('(' + qmlFunction + ')', context);
  return rowRevision(row);
}

test('canonical cast revisions distinguish unknown, verified false and verified true reactions', () => {
  const bare = { castId: '0xcast', _revision: 'same-content', text: 'unchanged', likeCount: 3, recastCount: 7 };
  const unknown = revision(bare);
  const falseLike = revision({ ...bare, likedByMe: false });
  const trueLike = revision({ ...bare, likedByMe: true });
  assert.notEqual(unknown, falseLike);
  assert.notEqual(falseLike, trueLike);
  assert.notEqual(unknown, trueLike);
  assert.notEqual(unknown, revision({ ...bare, recastedByMe: false }));
  assert.notEqual(unknown, revision({ ...bare, recastedByMe: true }));
  assert.equal(unknown, revision({ ...bare }));
  assert.equal(falseLike, revision({ ...bare, likedByMe: false }));
  // A public count change is distinct, irrespective of viewer evidence.
  assert.notEqual(falseLike, revision({ ...bare, likeCount: 4, likedByMe: false }));
});

test('following and follows-me knowledge transition invalidates cast and profile delegates', () => {
  for (const socialMode of [false, true]) {
    const base = { castId: '0xabc', fid: 123, displayName: 'Author', _revision: 'same' };
    const unknown = revision(base, socialMode);
    const ignoredUnverified = revision({
      ...base, followingKnown: false, followingByMe: true,
      followsMeKnown: false, followsMe: true,
    }, socialMode);
    assert.equal(unknown, ignoredUnverified);
    const negative = revision({ ...base, followingKnown: true, followingByMe: false }, socialMode);
    const positive = revision({ ...base, followingKnown: true, followingByMe: true }, socialMode);
    assert.notEqual(unknown, negative);
    assert.notEqual(unknown, positive);
    assert.notEqual(negative, positive);
    assert.notEqual(unknown, revision({ ...base, followsMeKnown: true, followsMe: false }, socialMode));
    assert.notEqual(unknown, revision({ ...base, followsMeKnown: true, followsMe: true }, socialMode));
    // Account change scrubs just the viewer fields. Revision returns to unknown.
    assert.equal(unknown, revision({ ...base }, socialMode));
  }
});

test('section identity and contextual quote revision remain stable', () => {
  assert.equal(revision({ _genySection: 'today', followingKnown: true, followingByMe: true }),
               'section:today');
  const initial = { castId: '0xquote', _revision: 'text', _referencedRevision: '' };
  assert.notEqual(revision(initial),
                  revision({ ...initial, _referencedRevision: 'resolved source cast' }));
});
