import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { runInNewContext } from 'node:vm';

const qml = readFileSync(new URL('../../ui/components/FeedSurface.qml', import.meta.url), 'utf8');
const start = qml.indexOf('    function synchronizeStableModel() {');
const end = qml.indexOf('\n    function isNearEnd()', start);
assert.ok(start >= 0 && end > start, 'extract the actual FeedSurface reconciliation');
const reconcileSource = qml.slice(start, end).trim();

const cast = (i, revision = 'r1') => ({ castId: '0xrow-' + i, _revision: revision });
const entry = payload => ({ payload, presentationKind: 'conversation' });

function harness(initial, source) {
  let list = initial.map(row => ({
    stableKey: row.castId, revisionKey: row._revision,
    presentationKind: 'conversation', mediaLayoutKind: 'text',
  }));
  const metrics = { gets: 0, insertions: 0, moves: 0, removals: 0, updates: [] };
  const stableRows = {
    get count() { return list.length; },
    get(i) {
      metrics.gets++;
      assert.ok(i >= 0 && i < list.length, 'model index in range');
      return { ...list[i] };
    },
    move(from, to, count) {
      assert.equal(count, 1);
      list.splice(to, 0, list.splice(from, 1)[0]);
      metrics.moves++;
    },
    insert(index, value) { list.splice(index, 0, { ...value }); metrics.insertions++; },
    append(value) { list.push({ ...value }); metrics.insertions++; },
    remove(index) { list.splice(index, 1); metrics.removals++; },
    clear() { list = []; metrics.removals++; },
    setProperty(i, role, value) {
      assert.ok(i >= 0 && i < list.length);
      assert.notEqual(list[i][role], value, 'avoid an unchanged Qt role notification');
      list[i][role] = value;
      metrics.updates.push({ i, role });
    },
  };
  const root = {
    sourceRows: () => source.map(entry),
    stablePayloads: Object.fromEntries(initial.map(row => [row.castId, row])),
    rowKey: row => row.castId,
    rowRevision: row => row._revision,
    mediaLayoutKind: () => 'text',
    rebuildStableModel(items) {
      stableRows.clear();
      root.stablePayloads = {};
      for (const item of items) {
        stableRows.append({
          stableKey: item.payload.castId, revisionKey: item.payload._revision,
          presentationKind: item.presentationKind, mediaLayoutKind: 'text',
        });
        root.stablePayloads[item.payload.castId] = item.payload;
      }
    },
  };
  const synchronize = runInNewContext('(' + reconcileSource + ')', { root, stableRows });
  return { synchronize, root, metrics, get rows() { return list; } };
}

test('10k cached casts, one changed reaction role: no quadratic Qt model access', () => {
  const initial = Array.from({ length: 10_000 }, (_, i) => cast(i));
  const updated = initial.slice();
  updated[4999] = cast(4999, 'changed-like');
  const h = harness(initial, updated);
  h.synchronize();
  assert.equal(h.rows.length, initial.length);
  assert.equal(h.rows[4999].revisionKey, 'changed-like');
  assert.equal(h.metrics.moves, 0);
  assert.equal(h.metrics.insertions, 0);
  assert.equal(h.metrics.removals, 0);
  assert.deepEqual(h.metrics.updates.map(v => v.role), ['revisionKey']);
  assert.ok(h.metrics.gets <= 2 * initial.length + 5,
    'one index build plus one Qt get per unchanged row');
  assert.equal(Object.keys(h.root.stablePayloads).length, initial.length);
});

test('reordering 600 cached casts preserves IDs while avoiding repeated Qt linear scans', () => {
  const initial = Array.from({ length: 600 }, (_, i) => cast(i));
  const desired = initial.toReversed();
  const h = harness(initial, desired);
  h.synchronize();
  assert.deepEqual(h.rows.map(v => v.stableKey), desired.map(v => v.castId));
  assert.equal(h.metrics.insertions, 0);
  assert.equal(h.metrics.removals, 0);
  assert.ok(h.metrics.moves > 0);
  assert.ok(h.metrics.gets <= 2 * initial.length + 10,
    'no model.get inside a per-row forward search');
});

test('appending and removing cached rows keeps one authoritative stable-ID collection', () => {
  const initial = Array.from({ length: 120 }, (_, i) => cast(i));
  const appended = [...initial, cast(120), cast(121)];
  const h = harness(initial, appended);
  h.synchronize();
  assert.equal(h.metrics.insertions, 2);
  assert.equal(h.metrics.moves, 0);
  assert.deepEqual(h.rows.slice(-2).map(v => v.stableKey), ['0xrow-120', '0xrow-121']);
  assert.equal(Object.keys(h.root.stablePayloads).length, 122);
  // A second identical sync is inert for all roles and structures.
  h.metrics.gets = 0;
  h.metrics.insertions = 0;
  h.metrics.updates = [];
  h.synchronize();
  assert.equal(h.metrics.insertions, 0);
  assert.deepEqual(h.metrics.updates, []);
  assert.ok(h.metrics.gets <= 2 * 122 + 5);
  // Source changes via the same canonical root state in the UI.
  h.root.sourceRows = () => initial.slice(1).map(entry);
  h.synchronize();
  assert.equal(h.rows.length, 119);
  assert.equal(h.rows[0].stableKey, '0xrow-1');
  assert.equal(h.root.stablePayloads['0xrow-0'], undefined);
  assert.equal(h.root.stablePayloads['0xrow-121'], undefined);
});

test('large fresh replacement triggers existing bounded reset heuristic', () => {
  const initial = Array.from({ length: 600 }, (_, i) => cast(i));
  const fresh = Array.from({ length: 600 }, (_, i) => cast(10_000 + i));
  const h = harness(initial, fresh);
  h.synchronize();
  assert.deepEqual(h.rows.map(v => v.stableKey), fresh.map(v => v.castId));
  assert.equal(h.metrics.moves, 0);
  assert.equal(h.metrics.insertions, 600);
  assert.equal(Object.keys(h.root.stablePayloads).length, 600);
});
