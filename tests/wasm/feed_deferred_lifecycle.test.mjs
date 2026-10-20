import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { runInNewContext } from 'node:vm';

const qml = readFileSync(new URL('../../ui/components/FeedSurface.qml', import.meta.url), 'utf8');
const start = qml.indexOf('    function deferFeedWork(syncRows, primeArt, checkPage) {');
const end = qml.indexOf('\n    onRowsChanged:', start);
assert.ok(start >= 0 && end > start, 'extract actual FeedSurface schedule function');
const deferredFunction = qml.slice(start, end).trim();
const timerStart = qml.indexOf('    Timer {\n        id: feedWorkTimer');
const triggeredAt = qml.indexOf('        onTriggered: {', timerStart);
const nextTimer = qml.indexOf('\n    Timer {\n        id: positionAtTopTimer', triggeredAt);
assert.ok(timerStart >= 0 && triggeredAt > timerStart && nextTimer > triggeredAt,
  'extract object-owned zero-delay timer');
const block = qml.slice(triggeredAt, nextTimer);
const callbackStart = block.indexOf('{') + 1;
const callbackEnd = block.lastIndexOf('\n        }');
const triggerBody = block.slice(callbackStart, callbackEnd);
assert.ok(triggerBody.length > 60);

function fixture(active = true, visuals = false) {
  const calls = [];
  const root = {
    presentationActive: active,
    presentationMode: visuals ? 'visuals' : 'for-you',
    deferredFeedSync: false, deferredArtPrime: false, deferredPageCheck: false,
    stableModelSyncPending: false,
    primeArtPreviews: () => calls.push('prime'),
    requestStableModelSync: () => calls.push('sync'),
    scheduleLoadMore: () => calls.push('page'),
  };
  const context = {
    root, feedWorkTimer: { restart: () => calls.push('restart') },
  };
  const defer = runInNewContext('(' + deferredFunction + ')', context);
  const trigger = runInNewContext('(function() {' + triggerBody + '\n})', context);
  return { root, calls, defer, trigger };
}

test('many row, art and pagination updates coalesce into a single owned refresh', () => {
  const { root, calls, defer, trigger } = fixture(true, true);
  defer(true, false, true);
  defer(false, true, false);
  defer(true, false, true);
  assert.equal(root.deferredFeedSync, true);
  assert.equal(root.deferredArtPrime, true);
  assert.equal(root.deferredPageCheck, true);
  trigger();
  assert.deepEqual(calls, ['restart', 'restart', 'restart', 'prime', 'sync', 'page']);
  assert.equal(root.deferredFeedSync, false);
  assert.equal(root.deferredArtPrime, false);
  assert.equal(root.deferredPageCheck, false);
  trigger(); // no pending work: must not start requests or rescans
  assert.equal(calls.length, 6);
});

test('inactive surfaces defer visible reconciliation and never admit network pagination', () => {
  const { root, calls, defer, trigger } = fixture(false, true);
  defer(true, true, true);
  trigger();
  assert.deepEqual(calls, ['restart']);
  assert.equal(root.stableModelSyncPending, true);
  root.presentationActive = true;
  defer(true, true, true);
  trigger();
  assert.deepEqual(calls, ['restart', 'restart', 'prime', 'sync', 'page']);
});

test('optional visual metadata priming respects current mode', () => {
  const { calls, defer, trigger } = fixture(true, false);
  defer(false, true, false);
  trigger();
  assert.deepEqual(calls, ['restart']);
  defer(true, false, false);
  trigger();
  assert.deepEqual(calls, ['restart', 'restart', 'sync']);
});

test('FeedSurface owns its deferred work and does not queue orphan callLater closures', () => {
  assert.doesNotMatch(qml, /Qt\.callLater\s*\(/);
  assert.match(qml, /id: feedWorkTimer/);
  assert.match(qml, /feedWorkTimer\.stop\(\)/);
  assert.match(qml, /id: positionAtTopTimer/);
});
