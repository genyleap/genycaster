import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const profile=readFileSync(new URL('../../ui/components/profile/ProfileSurface.qml',import.meta.url),'utf8');
test('cold entry constructs only selected profile header; visited headers remain mounted',()=>{
    const body=profile.match(/function retainSelectedHeader\(\) \{([\s\S]*?)\n    \}/);
    assert.ok(body);
    const retain=new Function('root',body[1]);
    const headerExpressions=[...profile.matchAll(/header: ([^\n]+)\n\s*\? profileHeaderComponent : null/g)];
    assert.equal(headerExpressions.length,2);
    const enabled=headerExpressions.map(match=>new Function('root','return Boolean('+match[1]+')'));
    for(const magazine of [false,true]){
        const root={profileMagazineEnabled:magazine,timelineHeaderRetained:false,magazineHeaderRetained:false};
        assert.deepEqual(enabled.map(f=>f(root)),[!magazine,magazine]);
        retain(root);
        root.profileMagazineEnabled=!magazine;
        assert.deepEqual(enabled.map(f=>f(root)),[true,true]);
        retain(root);
        root.profileMagazineEnabled=magazine;
        assert.deepEqual(enabled.map(f=>f(root)),[true,true]);
    }
    assert.match(profile,/root\.profileComponentReady = true\s+root\.retainSelectedHeader\(\)/);
    assert.match(profile,/root\.profileHeaderConstructed\s*&&/);
    assert.match(profile,/if \(!root\.profileRowsReleased\)/);
});
