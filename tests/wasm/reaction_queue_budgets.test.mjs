import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

const qml = readFileSync(new URL('../../ui/components/CastReactions.qml', import.meta.url), 'utf8');
const body = qml.match(/^    function ensure\(castId, viewerFid\) \{([\s\S]*?)^    \}\n\n    function dispatch\(/m);
assert.ok(body, 'reaction admission must remain independently testable');

function fixture() {
    const Qt = {platform: {os: 'wasm'}};
    const root = {
        pending: {}, retryAfter: {}, positive: {}, local: {},
        busy: false, keyFor: (castId, fid) => String(fid) + ':' + castId,
    };
    const dispatchTimer = {restart() {}};
    const admission = new Function('Qt', 'root', 'dispatchTimer',
        'return function ensure(castId, viewerFid) {' + body[1] + '\n};');
    return {root, ensure: admission(Qt, root, dispatchTimer)};
}

test('WASM reaction admission never accumulates an unbounded offscreen request queue', () => {
    const {root, ensure} = fixture();
    for (let index = 0; index < 800; ++index) {
        ensure('cast-' + index, 42);
    }
    const queued = Object.keys(root.pending);
    assert.ok(queued.length <= 32);
    assert.ok(Object.keys(root.retryAfter).length <= 256);
    assert.ok(queued.includes('42:cast-799'));
    assert.ok(!queued.includes('42:cast-0'));
});

test('positive evidence is bounded without discarding local optimistic reactions', () => {
    const {root, ensure} = fixture();
    root.local['42:cast-0:like'] = true;
    for (let index = 0; index < 350; ++index) {
        ensure('cast-' + index, 42);
        // Simulate dispatch consuming the pending cast and storing a verified
        // positive result. The singleton's retention policy must remain bounded.
        root.pending = {};
        root.positive['42:cast-' + index + ':like'] = true;
    }
    assert.ok(Object.keys(root.retryAfter).length <= 256);
    assert.ok(Object.keys(root.positive).length <= 256);
    assert.equal(root.local['42:cast-0:like'], true);
    assert.equal(root.positive['42:cast-349:like'], true);
});
