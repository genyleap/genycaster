import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const source = readFileSync(new URL('../../ui/components/media/GenyVideoPlayer.qml',import.meta.url),'utf8');
function bodyAt(start) {
    const open=source.indexOf('{',start);
    let depth=0;
    for(let i=open;i<source.length;++i){
        if(source[i]==='{')++depth;
        if(source[i]==='}'&&--depth===0)return source.slice(open+1,i);
    }
    throw Error('unbalanced');
}
const method=(name,...args)=>new Function(...args,bodyAt(source.indexOf('function '+name+'(')));
const handler=(id,...args)=>new Function(...args,bodyAt(source.indexOf('onTriggered: {',source.indexOf('id: '+id))));
test('mute writes the exposed property, not an invokable setter',()=>{
    const session={muted:false};const root={coordinator:()=>session};
    const toggle=method('toggleMute','root');
    toggle(root);assert.equal(session.muted,true);
    toggle(root);assert.equal(session.muted,false);
    root.sessionMuted=true;session.muted=true;
    method('setVolume','value','audioOutput','root')(0.7,{volume:0},root);
    assert.equal(session.muted,false);
});
test('source replacement invalidates queued transport including same URL',()=>{
    const activation={transportEpoch:1};
    const timer=()=>({targetSource:'same',stopped:false,stop(){this.stopped=true}});
    const start=timer(),reload=timer(),restore=Object.assign(timer(),{seekPosition:42,pauseRequested:true});
    method('cancelDeferredTransport','activation','transportStart','transportReload','transportRestore')(activation,start,reload,restore);
    assert.equal(activation.transportEpoch,2);
    for(const t of [start,reload,restore]){assert.equal(t.stopped,true);assert.equal(t.targetSource,'')}
    assert.equal(restore.seekPosition,0);assert.equal(restore.pauseRequested,false);
    assert.match(source,/activation\.preparationId = 0\s+activation\.pendingUrl = String\(target.url\)/);
});
test('source assignment cannot rearm old play after synchronous replacement',()=>{
    const activation={transportEpoch:1,pendingQualityId:'original'};
    const root={prepareBackendForSource(){}};
    const player={get source(){return ''},set source(v){activation.transportEpoch++}};
    let armed=false;
    method('startPendingSource','target','root','activation','player','startupWatchdog','Qt','transportStart')('a',root,activation,player,{restart(){},stop(){}},{platform:{os:'macos'}},{restart(){armed=true}});
    assert.equal(armed,false);
});
test('no global deferred callbacks remain in player',()=>{
    assert.doesNotMatch(source,/Qt\.callLater/);
    for(const id of ['transportStart','transportReload','transportRestore']) {
        assert.match(source,new RegExp('if \\('+id+'\\) '+id+'\\.stop\\(\\)'));
        assert.ok(handler(id));
    }
});
