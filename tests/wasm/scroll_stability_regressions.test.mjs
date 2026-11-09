import { componentPath } from './ui_component_paths.mjs';
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { runInNewContext } from 'node:vm';

const read = name => readFileSync(new URL('../../ui/components/' + componentPath(name) + '.qml', import.meta.url), 'utf8');
const feed = read('FeedSurface');
const detail = read('DetailSurface');
function method(source, name, context) {
    const start = source.indexOf('    function ' + name + '(');
    const end = source.indexOf('\n    }', start) + 6;
    assert.ok(start >= 0 && end > start, name);
    return runInNewContext('(' + source.slice(start, end).trim() + ')', context);
}
function timer() {
    return { running: false, restart() { this.running = true; }, stop() { this.running = false; } };
}
function feedHarness() {
    const item = { y: 1000 };
    const root = {
        presentationActive: true, applyingGeometryAnchor: false, applyingViewportRestore: false,
        pendingViewportScope: '', pendingViewportKey: '', pendingViewportSecondPass: false,
        geometryAnchorKey: 'a', geometryAnchorOffset: -40,
        activeFeed: 'trending', presentationMode: 'for-you', socialMode: false,
    };
    const readList = {
        moving: false, contentY: 1040, originY: -200, contentHeight: 4000, height: 800,
        itemAtIndex: () => item, forceLayout() {}, positionCalls: 0,
        positionViewAtIndex() { ++this.positionCalls; this.contentY = item.y; },
    };
    const context = {
        root, readList, ListView: { Beginning: 0 },
        stableRows: { count: 1, get: () => ({ stableKey: 'a' }) },
        geometryAnchorDebounce: timer(), geometryRestoreTimer: timer(),
        stableViewportRestoreTimer: timer(), positionAtTopTimer: timer(),
    };
    for (const name of ['cancelViewportCorrections', 'stabilizeGeometryAnchor', 'restoreViewportAfterRebuild'])
        root[name] = method(feed, name, context);
    const start = feed.indexOf('        function onContentYChanged() {');
    const end = feed.indexOf('\n        function onHeightChanged()', start);
    assert.ok(start >= 0 && end > start, 'shared readList scroll handler exists');
    const handler = feed.slice(start, end).trim().replace('function onContentYChanged()', 'function changed()');
    const contentYChanged = runInNewContext('(' + handler + ')', context);
    return { ...context, item, contentYChanged };
}

test('wheel/browser movement cancels an already queued geometry AND rebuild correction', () => {
    const h = feedHarness();
    h.geometryRestoreTimer.restart();
    h.stableViewportRestoreTimer.restart();
    h.positionAtTopTimer.restart();
    h.root.pendingViewportScope = 'trending:for-you:false';
    h.root.pendingViewportKey = 'a';
    h.readList.contentY += 120;
    h.contentYChanged();
    assert.equal(h.geometryRestoreTimer.running, false);
    assert.equal(h.stableViewportRestoreTimer.running, false);
    assert.equal(h.positionAtTopTimer.running, false);
    assert.equal(h.root.pendingViewportScope, '');
    h.root.stabilizeGeometryAnchor();
    assert.equal(h.readList.contentY, 1160, 'no pull back to the old offset');
});

test('actual height settlement preserves the visible cast despite a bad contentHeight estimate', () => {
    const h = feedHarness();
    h.item.y += 950;
    h.readList.contentHeight = 1600;
    h.root.stabilizeGeometryAnchor();
    assert.equal(h.item.y - h.readList.contentY, -40);
    assert.equal(h.root.applyingGeometryAnchor, false);
});

test('a retained rebuild anchor restores directly without a one-frame top snap', () => {
    const h = feedHarness();
    h.root.pendingViewportScope = 'trending:for-you:false';
    h.root.pendingViewportKey = 'a';
    h.root.pendingViewportOffset = -40;
    h.root.pendingViewportY = 1040;
    h.item.y = 1400;
    h.root.restoreViewportAfterRebuild();
    assert.equal(h.readList.contentY, 1440);
    assert.equal(h.readList.positionCalls, 0);
    assert.equal(h.root.pendingViewportScope, '');
});

test('hidden or moving feeds never apply a late geometry correction', () => {
    for (const property of ['moving', 'hidden']) {
        const h = feedHarness();
        h.item.y += 200;
        if (property === 'moving') h.readList.moving = true;
        else h.root.presentationActive = false;
        h.root.stabilizeGeometryAnchor();
        assert.equal(h.readList.contentY, 1040);
    }
});

function repliesHarness() {
    const rows = [];
    const calls = { resets: 0, insertions: 0, top: 0 };
    const replyRows = {
        get count() { return rows.length; },
        get: i => rows[i],
        clear() { rows.length = 0; ++calls.resets; },
        insert(i, value) { rows.splice(i, 0, { ...value }); ++calls.insertions; },
        remove(i) { rows.splice(i, 1); },
        move(from, to) { rows.splice(to, 0, rows.splice(from, 1)[0]); },
        setProperty(i, key, value) { rows[i][key] = value; },
        reconcile(next) {
            let index = 0;
            for (const entry of next) {
                if (!rows[index] || rows[index].stableKey !== entry.stableKey) {
                    const found = rows.findIndex((row, at) =>
                        at > index && row.stableKey === entry.stableKey);
                    if (found >= 0)
                        this.move(found, index, 1);
                    else
                        this.insert(index, entry);
                }
                rows[index].revisionKey = entry.revisionKey;
                ++index;
            }
            rows.splice(index);
        },
    };
    const root = {
        moving: false, visible: true, busy: false, activeView: 'cast',
        detailScope: 'cast:parent', replyScope: 'cast:parent',
        prioritizedReplies: [], replyPayloads: {}, replyRevision: 0, replySyncPending: true,
        geometryAnchorKey: '', confirmedReplyKey: '', focusedReplyKey: '',
        rememberVisibleGeometry() {},
        positionViewAtBeginning() { ++calls.top; },
    };
    const context = { root, replyRows, geometryRestoreTimer: timer(), confirmedReplyFocusTimer: timer() };
    for (const name of ['normalizedCastHash', 'cancelViewportCorrections', 'focusConfirmedReply', 'replyContentRevision', 'synchronizeReplies'])
        root[name] = method(detail, name, context);
    return { ...context, rows, calls };
}
const reply = (id, text = '') => ({ castId: '0x' + id, text });

test('thread hydration, reactions and appending replies preserve row identities without a reset', () => {
    const h = repliesHarness();
    h.root.prioritizedReplies = [reply('a'), reply('b')];
    h.root.synchronizeReplies();
    const retained = h.rows[1];
    h.root.prioritizedReplies = [reply('a'), { ...reply('b', 'hydrated'), likedByMe: true }, reply('c')];
    h.root.synchronizeReplies();
    assert.equal(h.rows[1], retained);
    assert.equal(h.root.replyPayloads.b.text, 'hydrated');
    assert.equal(h.root.replyPayloads.b.likedByMe, true);
    assert.equal(h.calls.resets, 0);
    assert.equal(h.calls.top, 0);
    assert.equal(h.calls.insertions, 3);
});

test('unchanged replies retain the same revision while a reaction updates only its cast', () => {
    const h = repliesHarness();
    h.root.prioritizedReplies = [reply('a', 'first'), reply('b', 'second')];
    h.root.synchronizeReplies();
    const previousA = h.rows[0].revisionKey;
    const previousB = h.rows[1].revisionKey;
    h.root.prioritizedReplies = [reply('a', 'first'), {...reply('b', 'second'), likedByMe: true}];
    h.root.synchronizeReplies();
    assert.equal(h.rows[0].revisionKey, previousA);
    assert.notEqual(h.rows[1].revisionKey, previousB);
    assert.equal(h.calls.resets, 0);
    assert.equal(h.calls.insertions, 2);
});

test('reply reordering, duplicate IDs, deletion and cast navigation stay scoped', () => {
    const h = repliesHarness();
    h.root.prioritizedReplies = [reply('a'), reply('b'), reply('c')];
    h.root.synchronizeReplies();
    const retained = h.rows[2];
    h.root.prioritizedReplies = [reply('c'), reply('c'), reply('b'), reply('d')];
    h.root.synchronizeReplies();
    assert.deepEqual(h.rows.map(row => row.stableKey), ['c', 'b', 'd']);
    assert.equal(h.rows[0], retained);
    assert.equal(h.root.replyPayloads.a, undefined);
    assert.equal(h.calls.resets, 0);
    h.root.detailScope = 'cast:another';
    h.root.prioritizedReplies = [reply('e')];
    h.root.synchronizeReplies();
    assert.deepEqual(h.rows.map(row => row.stableKey), ['e']);
    assert.equal(h.calls.resets, 1);
    assert.equal(h.calls.top, 1);
});

test('thread refresh keeps cached rows while empty/busy and defers mutation during a flick', () => {
    const h = repliesHarness();
    h.root.prioritizedReplies = [reply('a')];
    h.root.synchronizeReplies();
    h.root.busy = true;
    h.root.prioritizedReplies = [];
    h.root.synchronizeReplies();
    assert.equal(h.rows.length, 1);
    h.root.busy = false;
    h.root.moving = true;
    h.root.prioritizedReplies = [reply('a'), reply('b')];
    h.root.replySyncPending = true;
    h.root.synchronizeReplies();
    assert.equal(h.rows.length, 1);
    assert.equal(h.root.replySyncPending, true);
    h.root.moving = false;
    h.root.synchronizeReplies();
    assert.equal(h.rows.length, 2);
});

test('cancelled confirmed-reply focus is not rearmed by hydration or media geometry', () => {
    const h = repliesHarness();
    h.root.confirmedReplyKey = 'cast:parent:myreply';
    h.root.focusConfirmedReply();
    assert.equal(h.confirmedReplyFocusTimer.running, true);
    h.root.cancelViewportCorrections();
    h.root.focusConfirmedReply();
    assert.equal(h.confirmedReplyFocusTimer.running, false);
    assert.doesNotMatch(detail, /onContentHeightChanged:.*focusConfirmedReply/);
});

test('desktop Timeline freezes image geometry while mobile single portraits settle to their actual ratio', () => {
    const panel = read('CastEmbedsPanel');
    const start = panel.indexOf('        dynamicAspectDiscovery:');
    const end = panel.indexOf('        onOpenRequested:', start);
    assert.ok(start >= 0 && end > start);
    const expression = panel.slice(start, end).split(':').slice(1).join(':').trim();
    const enabled = root => runInNewContext(expression, { root });
    assert.equal(enabled({magazineCard: false, mobileViewport: false, scrollHost: {}, visualCount: 1}), false);
    assert.equal(enabled({magazineCard: false, mobileViewport: false, scrollHost: null, visualCount: 1}), true);
    assert.equal(enabled({magazineCard: true, mobileViewport: false, scrollHost: {}, visualCount: 2}), true);
    assert.equal(enabled({magazineCard: false, mobileViewport: true, scrollHost: {moving: true}, visualCount: 1}), true,
        'mobile portrait decodes exact aspect but is applied after the flick');
    assert.equal(enabled({magazineCard: false, mobileViewport: true, scrollHost: {}, visualCount: 2}), false,
        'multi-image mobile grids retain their stable mosaic layout');
    const contentHeightHandler = feed.slice(feed.indexOf('        function onContentHeightChanged()'), feed.indexOf('        function onContentYChanged()'));
    assert.doesNotMatch(contentHeightHandler, /geometryRestoreTimer/, 'estimated extent cannot trigger correction');
});

test('late header/preceding-reply geometry preserves the same reply offset without estimated clamping', () => {
    const h = repliesHarness();
    h.root.prioritizedReplies = [reply('a'), reply('b')];
    h.root.synchronizeReplies();
    const item = { y: 1800 };
    Object.assign(h.root, {
        geometryAnchorKey: 'b', geometryAnchorOffset: -60, contentY: 1000,
        contentHeight: 1200, height: 800, itemAtIndex: i => i === 1 ? item : null,
        forceLayout() {},
    });
    const restore = method(detail, 'restoreVisibleGeometry', h);
    restore();
    assert.equal(h.root.contentY, 1860);
    assert.equal(h.root.geometryAnchorKey, '');
    assert.equal(h.root.applyingViewportChange, false);
    h.root.geometryAnchorKey = 'removed';
    restore();
    assert.equal(h.root.contentY, 1860, 'removed anchor never moves the viewport');
});

test('user scrolling cancels pending thread geometry correction', () => {
    const h = repliesHarness();
    Object.assign(h.root, {
        geometryAnchorKey: 'a', geometryAnchorOffset: 0, applyingViewportChange: false,
        contentY: 1200,
    });
    h.geometryRestoreTimer.restart();
    const start = detail.indexOf('    onContentYChanged: {');
    const end = detail.indexOf('\n    }', start) + 6;
    runInNewContext('(' + detail.slice(start, end).replace('onContentYChanged:', 'function changed()') + ')', h)();
    assert.equal(h.geometryRestoreTimer.running, false);
    assert.equal(h.root.geometryAnchorKey, '');
    assert.equal(h.root.contentY, 1200);
});
