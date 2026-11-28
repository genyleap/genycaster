import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');
const cast = read('ui/components/feed/CastItem.qml');
const native = read('src/presentation/qt/media_embed_equivalence.hpp');
const model = read('ui/app/social/feed_rows.cpp');

const start = cast.indexOf('    function synchronizeMediaContent() {');
const end = cast.indexOf('\n    onModelDataChanged:', start);
assert.ok(start >= 0 && end > start);
const synchronize = new Function('root', 'mediaClassifier',
    cast.slice(start, end) + '\nreturn synchronizeMediaContent;');

test('viewer-only changes retain media projection identity', () => {
    const original = {castId:'a',text:'caption',parentUrl:'',embeds:[
        {kind:'image',url:'https://example.invalid/a',metadata:{width:640,height:480}},
        {kind:'url',url:'https://example.invalid/b'}
    ]};
    const root = {modelData:original,mediaContent:null};
    // A test double for the native equality boundary, not a native Qt test.
    const classifier = {sameEmbedRecords:(a,b)=>JSON.stringify(a)===JSON.stringify(b)};
    const sync = synchronize(root,classifier);
    sync();
    const prior = root.mediaContent;
    for (const patch of [{likeCount:5,likedByMe:true},{recastCount:7},
                         {followingKnown:true,followingByMe:false},{viewCount:99}]) {
        root.modelData = {...original,...patch,embeds:structuredClone(original.embeds)};
        sync();
        assert.equal(root.mediaContent,prior);
    }
});

test('hydration, changed text, art context, parent, order and cast identity refresh media', () => {
    const original = {castId:'a',text:'caption',parentUrl:'',embeds:[
        {url:'https://example.invalid/a',metadata:{width:640}},
        {url:'https://example.invalid/b'}
    ]};
    const root = {modelData:original,mediaContent:null};
    const sync = synchronize(root,{sameEmbedRecords:(a,b)=>JSON.stringify(a)===JSON.stringify(b)});
    for (const patch of [
        {embeds:[{...original.embeds[0],metadata:{width:641}},original.embeds[1]]},
        {text:'different'}, {parentUrl:'chain://token'}, {_artChannelSource:true},
        {embeds:[...original.embeds].reverse()}, {castId:'b'}, {embeds:[]}
    ]) {
        root.modelData=original;sync();
        const previous=root.mediaContent;
        root.modelData={...original,...patch};sync();
        assert.notEqual(root.mediaContent,previous);
    }
});

test('missing native equality never suppresses a media refresh', () => {
    const root = {modelData:{castId:'a',embeds:[]},mediaContent:null};
    const sync=synchronize(root,undefined);
    sync();const previous=root.mediaContent;sync();
    assert.notEqual(root.mediaContent,previous);
});

test('classification uses snapshot fields and role timing records a precise mask', () => {
    const block=cast.slice(cast.indexOf('    function isVisualImage('),cast.indexOf('    readonly property var visualEmbeds:'));
    assert.doesNotMatch(block,/root\.modelData\.embeds|root\.modelData\.text|root\.modelData\._artChannelSource/);
    assert.match(cast,/onModelDataChanged: root\.synchronizeMediaContent\(\)/);
    assert.match(cast,/castData: root\.modelData/);
    assert.match(native,/left\.metaType\(\) != right\.metaType\(\)/);
    assert.match(native,/depth >= 32/);
    assert.match(model,/"slowRoleMask"/);
    assert.match(model,/roles\.contains\(MediaLayoutKindRole\) \? 4 : 0/);
});
