import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const source=readFileSync(new URL('../../ui/components/feed/NotificationSurface.qml',import.meta.url),'utf8');
test('native notification publication has payloads before signals and prunes retired keys afterwards',()=>{
    const body=source.match(/function synchronizeStableModel\(\) \{([\s\S]*?)\n    \}/);
    assert.ok(body);
    const sync=new Function('root','list','notificationModel','renderingController',body[1]);
    const a={id:'a',revision:'1'},b={id:'b',revision:'2'},duplicate={id:'a',revision:'3'};
    const root={visibleRows:[a,b,duplicate],stablePayloads:{obsolete:{id:'obsolete'}},
        notificationKey:r=>r.id,notificationRevision:r=>r.revision,
        scheduleViewportRestore(key,offset,capture){this.restored=[key,offset,capture]}};
    const model={count:1,keyAt:()=> 'a',reconcile(roles){
        assert.deepEqual(roles,[{stableKey:'a',revisionKey:'1'},{stableKey:'b',revisionKey:'2'}]);
        assert.equal(root.stablePayloads.a,a);assert.equal(root.stablePayloads.b,b);
        assert.ok(root.stablePayloads.obsolete);return true;
    }};
    const list={contentY:10,width:300,indexAt:()=>0,itemAtIndex:()=>({y:15})};
    sync(root,list,model,undefined);
    assert.deepEqual(Object.keys(root.stablePayloads),['a','b']);
    assert.deepEqual(root.restored,['a',5,false]);
});
test('native model preserves primitive roles and final-filter change triggers synchronization',()=>{
    assert.match(source,/FeedRowsModel \{\s*id: notificationModel/);
    assert.match(source,/notificationModel\.indexForKey\(anchorKey\)/);
    assert.match(source,/onVisibleRowsChanged: stableModelSyncTimer.restart\(\)/);
    assert.doesNotMatch(source,/notificationModel\.(get|insert|remove|move|setProperty)\(/);
});
