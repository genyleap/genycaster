import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const wall=readFileSync(new URL('../../ui/components/feed/FeedMasonryView.qml',import.meta.url),'utf8');
const placeholder=wall.slice(wall.indexOf('id: visiblePlaceholder'),wall.indexOf('id: cardLoader'));
test('viewport crossings do not cancel or recreate an admitted skeleton',()=>{
    const body=placeholder.match(/function requestIfNeeded\(\) \{([\s\S]*?)\n                \}/);
    assert.ok(body);
    const invoke=new Function('state','renderingController','with(state){'+body[1]+'}');
    const state={needed:false,requested:false};
    invoke(state,undefined);assert.equal(state.requested,false);
    state.needed=true;invoke(state,{shuttingDown:true});assert.equal(state.requested,false);
    invoke(state,{shuttingDown:false});assert.equal(state.requested,true);
    for(const needed of [false,true,false]){
        state.needed=needed;invoke(state,undefined);assert.equal(state.requested,true);
    }
    assert.match(placeholder,/active: requested/);
    assert.doesNotMatch(placeholder,/active:[^\n]*item/);
    assert.match(placeholder,/visible: slot\.intersectsViewport && \(!slot\.cardReady \|\| opacity > 0\.01\)/);
    assert.match(placeholder,/animate: visiblePlaceholder\.visible && !view\.moving/);
});
