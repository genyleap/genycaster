import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');
const feed = read('ui/components/feed/FeedSurface.qml');
const sprite = read('ui/components/media/SpriteEmoji.qml');
const animated = read('ui/components/media/AnimatedEmoji.qml');
const tabs = read('ui/components/controls/GenyTabStrip.qml');
const native = read('ui/app/social/feed_rows.cpp');

function timerBody(source, id) {
    const start = source.indexOf('id: ' + id);
    assert.ok(start >= 0, id);
    const handler = source.indexOf('onTriggered: {', start);
    assert.ok(handler > start);
    const opening = source.indexOf('{', handler);
    let depth = 0;
    for (let i = opening; i < source.length; ++i) {
        if (source[i] === '{') ++depth;
        else if (source[i] === '}' && --depth === 0)
            return source.slice(opening + 1, i);
    }
    throw new Error('Unbalanced timer ' + id);
}

test('cold insertion, exact revision ranges and count notification contracts', () => {
    assert.match(native, /if \(m_rows\.isEmpty\(\)\) \{[\s\S]*?beginInsertRows\(\{\}, 0,/);
    assert.match(native, /if \(countWillChange\) notifyCount\(\)/);
    assert.match(native, /notifyRoles\(firstRevision, last, revisionOnly\)/);
    for (const phase of ['resetMs','removeMs','insertMs','moveMs','rolesMs','countMs'])
        assert.ok(native.includes('"' + phase + '"'));
    assert.match(feed, /stableRows\.indexForKey\(root\.returnCastKey, true\)/);
    assert.match(feed, /stableRows\.indexForKey\(key\)/);
    assert.doesNotMatch(feed, /stableRows\.get\(i\)\.stableKey/);
});

test('magazine has one placeholder and pooled rows do not settle scroll geometry', () => {
    assert.match(feed, /active: !rowDelegate\.magazineRow && !rowDelegate\.sectionRow/);
    assert.match(feed, /target: !rowDelegate\.magazineRow && rowDelegate\.visible \? readList : null/);
});

test('sprite metadata publication rejects changed identity, hidden artwork and shutdown', () => {
    const commit = new Function('root','retryDelay',timerBody(sprite,'spriteCommitTimer'));
    const root = {unicodeSequence:'B',artworkWanted:true,shuttingDown:false,spriteData:null};
    const timer = {stop(){}};
    for (const pending of [
        {pendingSpriteSequence:'A',artworkWanted:true,shuttingDown:false},
        {pendingSpriteSequence:'B',artworkWanted:false,shuttingDown:false},
        {pendingSpriteSequence:'B',artworkWanted:true,shuttingDown:true},
    ]) {
        Object.assign(root,pending,{pendingSpriteMetadata:{source:'stale'}});
        commit(root,timer);
        assert.equal(root.spriteData,null);
        assert.equal(root.pendingSpriteMetadata,null);
        assert.equal(root.pendingSpriteSequence,'');
    }
    Object.assign(root,{pendingSpriteSequence:'B',pendingSpriteMetadata:{source:'current'},
        artworkWanted:true,shuttingDown:false});
    commit(root,timer);
    assert.equal(root.spriteData.source,'current');
    assert.doesNotMatch(sprite,/Qt\.callLater/);
});

test('vector replay restores paused artwork but never targets a recycled renderer', () => {
    const restore = new Function('root','lottie',timerBody(animated,'replayRestoreTimer'));
    const target = {source:''};
    const root = {replayTarget:target,replaySequence:'A',unicodeSequence:'A',
        assetSource:'asset-A',shuttingDown:false,motionActive:false};
    restore(root,{item:target});
    assert.equal(target.source,'asset-A');
    assert.equal(root.replayTarget,null);
    const replacement = {source:'new'};
    Object.assign(root,{replayTarget:target,replaySequence:'A',assetSource:'wrong'});
    restore(root,{item:replacement});
    assert.equal(target.source,'asset-A');
    assert.equal(replacement.source,'new');
    Object.assign(root,{replayTarget:target,replaySequence:'B'});
    restore(root,{item:target});
    assert.equal(target.source,'asset-A');
    assert.doesNotMatch(animated,/Qt\.callLater/);
    assert.doesNotMatch(tabs,/Qt\.callLater/);
    assert.match(tabs,/id: revealTimer/);
});
