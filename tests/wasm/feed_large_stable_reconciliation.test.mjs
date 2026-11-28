import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';

const qml = readFileSync(new URL('../../ui/components/feed/FeedSurface.qml', import.meta.url), 'utf8');
const cpp = readFileSync(new URL('../../ui/app/social/feed_rows.cpp', import.meta.url), 'utf8');
const start = qml.indexOf('    function synchronizeStableModel() {');
const end = qml.indexOf('\n    function isNearEnd()', start);
assert.ok(start >= 0 && end > start);
const sync = qml.slice(start, end).trim();

function fixture(initial, desired) {
    const source = desired.map(payload => ({payload, presentationKind: 'conversation'}));
    const calls = [];
    const stableRows = {
        count: initial.length,
        reconcile(batch) {
            calls.push(batch);
            this.count = batch.length;
        },
    };
    const root = {
        busy: false,
        stablePayloads: Object.fromEntries(initial.map(row => [row.castId, row])),
        sourceRows: () => source,
        rowKey: row => row.castId,
        rowRevision: row => row._revision,
        mediaLayoutKind: () => 'other',
    };
    runInNewContext('(' + sync + ')', {root, stableRows})();
    return {root, calls};
}
const cast = (id, revision = 'r1') => ({castId: 'cast:' + id, _revision: revision});

test('10k cached casts with one viewer revision crosses the native boundary once', () => {
    const source = Array.from({length: 10_000}, (_, i) => cast(i));
    const desired = source.slice();
    desired[4999] = cast(4999, 'liked');
    const result = fixture(source, desired);
    assert.equal(result.calls.length, 1);
    assert.equal(result.calls[0].length, 10_000);
    assert.equal(result.calls[0][4999].revisionKey, 'liked');
    assert.equal(Object.keys(result.root.stablePayloads).length, 10_000);
    assert.match(qml, /stableRows\.reconcile\(next\)/);
    assert.doesNotMatch(sync, /stableRows\.get\(/);
    assert.doesNotMatch(sync, /stableRows\.(?:move|insert|append|remove)\(/);
});

test('a reordered feed sends ordered stable IDs for native reconciliation', () => {
    const original = Array.from({length: 600}, (_, i) => cast(i));
    const desired = original.toReversed();
    const result = fixture(original, desired);
    assert.deepEqual(Array.from(result.calls[0], row => row.stableKey),
        desired.map(row => row.castId));
    assert.equal(result.calls.length, 1);
    assert.match(cpp, /bool FeedRowsModel::reconcile\(const QVariantList& rows\)/);
    assert.match(cpp, /beginMoveRows\(\{\}, found, found, \{\}, i\)/);
});

test('new page and departed IDs have one authoritative payload store', () => {
    const initial = Array.from({length: 120}, (_, i) => cast(i));
    const newer = [...initial.slice(1), cast(120), cast(121)];
    const result = fixture(initial, newer);
    assert.equal(result.calls[0].length, 121);
    assert.equal(result.root.stablePayloads['cast:0'], undefined);
    assert.ok(result.root.stablePayloads['cast:121']);
    assert.equal(Object.keys(result.root.stablePayloads).length, 121);
});

test('unrelated source changes use a bounded native model reset, not thousands of JS inserts', () => {
    const original = Array.from({length: 600}, (_, i) => cast(i));
    const fresh = Array.from({length: 600}, (_, i) => cast(10_000 + i));
    const result = fixture(original, fresh);
    assert.equal(result.calls.length, 1);
    assert.equal(result.calls[0].length, 600);
    assert.equal(Object.keys(result.root.stablePayloads).length, 600);
    assert.match(cpp, /retained \* 4 < std::min\(m_rows.size\(\), incoming.size\(\)\)/);
    assert.match(cpp, /beginResetModel\(\)/);
});
