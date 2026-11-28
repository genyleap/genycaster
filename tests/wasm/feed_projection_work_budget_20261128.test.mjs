import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';

const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const profile=read('ui/components/profile/ProfileSurface.qml');
const feed=read('ui/components/feed/FeedSurface.qml');
function method(source,name,next,context) {
    const start=source.indexOf('    function '+name+'(');
    const end=source.indexOf(next,start);
    assert.ok(start>=0 && end>start,name);
    return runInNewContext('('+source.slice(start,end).trim()+')',context);
}
test('Profile ranking reads each score/time exactly once; preserves order and ties',()=>{
    const rows=Array.from({length:5000},(_,i)=>({
        id:i,createdAtMs:(i*73)%1234,likeCount:i%17,recastCount:i%7,replyCount:i%5
    }));
    rows.push({id:5000,time:'invalid',likeCount:0},{id:5001,createdAtMs:0});
    const root={rows};
    const context={root};
    const time=method(profile,'rowTime','\n    function popularityScore',context);
    const score=method(profile,'popularityScore','\n    function sortedRows',context);
    const sorted=method(profile,'sortedRows','\n    function rowKey',context);
    for(const profileFeed of ['user-casts','user-popular']) {
        root.profileFeed=profileFeed;
        for(const profileSortMode of ['oldest','newest']) {
            root.profileSortMode=profileSortMode;
            for(const popularMetric of ['likes','recasts','replies','all']) {
                root.popularMetric=popularMetric;
                let timeCalls=0,scoreCalls=0;
                root.rowTime=row=>{++timeCalls;return time(row);};
                root.popularityScore=row=>{++scoreCalls;return score(row);};
                const actual=Array.from(sorted());
                assert.equal(timeCalls,rows.length);
                assert.equal(scoreCalls,profileFeed==='user-popular'?rows.length:0);
                const expected=[...rows].sort((a,b)=>{
                    if(profileFeed==='user-popular') {
                        const delta=score(b)-score(a);
                        return delta!==0?delta:time(b)-time(a);
                    }
                    return (time(a)-time(b))*(profileSortMode==='oldest'?1:-1);
                });
                assert.deepEqual(actual,expected);
            }
        }
    }
    assert.equal(root.rows,rows);
    assert.match(profile,/const structureChanged = profileRowsModel\.reconcile\(next\)/);
    const sync=profile.slice(profile.indexOf('    function synchronizeStableModel() {'),
                             profile.indexOf('    function isNearEnd()'));
    assert.doesNotMatch(sync,/profileRowsModel\.get\(index\)/);
});

test('Feed height reservation skips unused classification and caps supporting work',()=>{
    let classifications=0;
    const root={
        compact:false,magazineEnabled:false,socialMode:false,
        classifiedFamily:embed=>{++classifications;return embed.kind;}
    };
    const Theme={typeTitle3Size:16,typeBodyLineHeight:1.4,space4:16,mobileGutter:12,
        space3:12,typeBodySize:14,space2:8,space5:20,space7:28};
    const estimate=method(feed,'rowIncubationHeight','\n    function rowRevision',{root,Theme});
    const links=Array.from({length:50},()=>({kind:'web'}));
    const row={text:'A\nB\nC',embeds:links};
    const other=estimate(row,'conversation','other',600);
    assert.equal(classifications,0);
    assert.equal(other,112+3*20+230);
    const mixed={text:row.text,embeds:[{kind:'image',width:800,height:600},...links]};
    const height=estimate(mixed,'conversation','single-image',600);
    assert.equal(classifications,3);
    assert.equal(height,Math.ceil(112+3*20+(600-78)/(4/3)+8+2*152));
    classifications=0;
    assert.equal(estimate({...mixed,embeds:mixed.embeds.slice(0,3)},'conversation','single-image',600),height);
    assert.equal(classifications,3);
});

test('Static classification caches exact inputs only; metadata, malformed and oversized inputs bypass',()=>{
    let calls=0;
    const root={staticMediaFamilies:new Map(),embedUrl:e=>e.url||e.value||''};
    const mediaClassifier={classify(e,m){++calls;return {family:m.contentType||m.providerKind||e.contentType||e.mediaType||e.kind||'web'};}};
    const context={root,mediaClassifier,linkPreviewResolver:{previewFor:()=>({contentType:'resolver'})}};
    root.staticMediaFamilyKey=method(feed,'staticMediaFamilyKey','\n    function classifiedFamily',context);
    const classify=method(feed,'classifiedFamily','\n    function looksLikeArtworkUrl',context);
    const embed={kind:'url',url:'https://example.invalid/asset'};
    assert.equal(classify(embed,{}),'url');
    assert.equal(classify({...embed},{}),'url');
    assert.equal(calls,1);
    for(const field of ['kind','url','value','mediaType','contentType','providerKind']) {
        classify({...embed,[field]:'different'}, {});
    }
    assert.equal(calls,7);
    assert.equal(classify(embed,{contentType:'image'}),'image');
    assert.equal(classify(embed,{providerKind:'video'}),'video');
    assert.equal(classify(embed),'resolver');
    assert.equal(calls,10);
    assert.equal(classify(embed,{}),'url');
    assert.equal(calls,10);
    for(const e of [{...embed,url:42},{...embed,url:'x'.repeat(3000)}]) {
        classify(e,{});classify(e,{});
    }
    assert.equal(calls,14);
    for(let i=0;i<3000;++i)
        classify({...embed,url:'https://example.invalid/'+i},{});
    assert.equal(root.staticMediaFamilies.size,2048);
    const before=calls;
    classify(embed,{});
    assert.equal(calls,before+1,'old entry evicted');
    assert.equal(root.staticMediaFamilies.size,2048);

    // Sequential scans larger than capacity must not wipe the entire
    // warm cache. Viewport-specific lookups can still replace cold entries.
    root.staticMediaFamilies.clear();
    for(let i=0;i<3000;++i)
        classify({...embed,url:'https://example.invalid/scan/'+i},{},false);
    const firstScan=calls;
    for(let i=0;i<3000;++i)
        classify({...embed,url:'https://example.invalid/scan/'+i},{},false);
    assert.equal(calls-firstScan,3000-2048);
    assert.equal(root.staticMediaFamilies.size,2048);
});
