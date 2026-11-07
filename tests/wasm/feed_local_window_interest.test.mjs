import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { runInNewContext } from 'node:vm';

const qml = readFileSync(new URL('../../ui/components/feed/FeedSurface.qml', import.meta.url), 'utf8');
const first = qml.indexOf('    function scheduleLoadMore() {');
const last = qml.indexOf('    function sectionTitle(key) {', first);
assert.ok(first >= 0 && last > first);
const actual = qml.slice(first, last).trim();

function trySchedule({ cachedMore, offline, moving = false, sourceUnavailable = offline, errorText = offline ? 'offline' : '' }) {
  const called = [];
  const root = {
    presentationActive: true, cachedMore, offline, sourceUnavailable,
    errorText, busy: false, loadMorePending: false, hasMore: true,
    canAutomaticallyPage: () => true, needsFilteredPrefetch: () => false,
    isNearEnd: () => true,
  };
  const ctx = {
    root,
    readList: { moving },
    stableRows: { count: 64 },
    filterPrefetchDebounce: { stop: () => called.push('filter-stop'), restart: () => called.push('filter-restart') },
    loadMoreDebounce: { stop: () => called.push('load-stop'), restart: () => called.push('load-restart') },
  };
  const fn = runInNewContext('(' + actual + ')', ctx);
  fn();
  return called;
}

test('offline cached-feed continuation still requests the next bounded window', () => {
  assert.deepEqual(trySchedule({ cachedMore: true, offline: true }), ['load-restart']);
  assert.deepEqual(trySchedule({ cachedMore: false, offline: true }),
                   ['filter-stop', 'load-stop']);
});

test('fresh provider demand remains paused while unavailable and during motion', () => {
  assert.deepEqual(trySchedule({ cachedMore: false, offline: true }), ['filter-stop', 'load-stop']);
  assert.deepEqual(trySchedule({ cachedMore: false, offline: false }), ['load-restart']);
  assert.deepEqual(trySchedule({ cachedMore: true, offline: true, moving: true }), []);
});
