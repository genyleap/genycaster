import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

const qml = readFileSync(
    new URL('../../ui/components/feed/FeedSurface.qml', import.meta.url), 'utf8');
const source = qml.match(
    /    function synchronizeStableModel\(\) \{([\s\S]*?)\n    \}\n\n    function isNearEnd\(/);
assert.ok(source, 'The live ListModel reconciliation must be executable in isolation');

function fixture() {
    let current = [];
    const stableRows = {
        items: [],
        get count() { return this.items.length; },
        clear() { this.items.length = 0; },
        get(index) { return this.items[index]; },
        append(item) { this.items.push(item); },
        insert(index, item) { this.items.splice(index, 0, item); },
        remove(index) { this.items.splice(index, 1); },
        move(from, to, count) {
            this.items.splice(to, 0, ...this.items.splice(from, count));
        },
        setProperty(index, property, value) { this.items[index][property] = value; },
    };
    const root = {
        stablePayloads: {},
        sourceRows: () => current.map(payload => ({
            payload, presentationKind: 'conversation',
        })),
        rowKey: payload => 'cast:' + payload.castId,
        rowRevision: payload => String(payload.revision || 0),
        mediaLayoutKind: () => 'other',
        rebuildStableModel(list) {
            stableRows.clear();
            const payloads = {};
            for (const entry of list) {
                const key = this.rowKey(entry.payload);
                payloads[key] = entry.payload;
                stableRows.append({
                    stableKey: key,
                    revisionKey: this.rowRevision(entry.payload),
                    presentationKind: entry.presentationKind,
                    mediaLayoutKind: this.mediaLayoutKind(entry.payload),
                });
            }
            this.stablePayloads = payloads;
        },
    };
    const synchronize = new Function(
        'root', 'stableRows',
        'return function synchronizeStableModel() {' + source[1] + '\n};',
    )(root, stableRows);
    return {
        root, stableRows, synchronize,
        update(next) { current = next; synchronize(); },
    };
}

test('incremental feed updates release departed cast payloads, retaining ordered rows', () => {
    const view = fixture();
    const item = (id, revision = 0) => ({castId: id, revision});
    view.update([item('a'), item('b'), item('c')]);
    assert.equal(Object.keys(view.root.stablePayloads).length, 3);
    view.update([item('a'), item('b', 2), item('c'), item('d')]);
    assert.deepEqual(view.stableRows.items.map(row => row.stableKey),
        ['cast:a', 'cast:b', 'cast:c', 'cast:d']);
    assert.equal(view.stableRows.get(1).revisionKey, '2');

    // This retains most of the viewport and must take the incremental path,
    // not the full-model clear/rebuild branch.
    view.update([item('b', 2), item('c'), item('d')]);
    assert.equal(view.root.stablePayloads['cast:a'], undefined);
    assert.equal(Object.keys(view.root.stablePayloads).length, 3);
    assert.deepEqual(view.stableRows.items.map(row => row.stableKey),
        ['cast:b', 'cast:c', 'cast:d']);

    view.update([item('other'), item('new')]);
    assert.equal(view.root.stablePayloads['cast:b'], undefined);
    assert.equal(Object.keys(view.root.stablePayloads).length, 2);
});

test('rotating hundreds of feed pages does not retain unbounded historical media', () => {
    const view = fixture();
    for (let index = 0; index < 350; ++index) {
        const window = Array.from({length: 8}, (_, slot) => ({
            castId: 'post-' + (index + slot),
            embeds: [{url: 'https://example.invalid/image-' + index + '-' + slot}],
        }));
        view.update(window);
        assert.equal(view.stableRows.count, 8);
        assert.equal(Object.keys(view.root.stablePayloads).length, 8);
    }
    assert.equal(view.root.stablePayloads['cast:post-0'], undefined);
    assert.ok(view.root.stablePayloads['cast:post-356']);
});
