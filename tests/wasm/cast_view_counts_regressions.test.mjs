import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../../'+p,import.meta.url),'utf8');

test('one shared QML singleton batches at most 16 visible cast view queries across recycled delegates',()=>{
  const cmake=read('ui/CMakeLists.txt');
  const provider=read('ui/components/CastViews.qml');
  assert.match(cmake,/components\/CastViews.qml/);
  assert.match(cmake,/set_source_files_properties\(components\/CastViews.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE\)/);
  assert.match(provider,/pragma Singleton/);
  assert.match(provider,/raw\.startsWith\("0x"\) \? raw : "0x" \+ raw/);
  assert.match(provider,/Object\.keys\(root\.pending\)\.slice\(0, 16\)/);
  assert.match(provider,/storage-farcaster\.genyleap\.com\/v1\/browser\/cast-views/);
  assert.match(provider,/xhr\.send\(JSON\.stringify\(\{casts: batch\}\)\)/);
  assert.match(provider,/property int revision: 0/);
  assert.match(provider,/root\.revision \+= 1/);
  assert.match(provider,/root\.retryAfter\[key\] = now \+ 90 \* 1000/);
});

test('every cast and reply uses the same view counter cache without replacing feed delegates',()=>{
  const bar=read('ui/components/EngagementBar.qml');
  const provider=read('ui/components/CastViews.qml');
  assert.match(bar,/CastViews\.ensure\(root\.castId, root\.authorFid, root\.castText\)/);
  assert.match(bar,/return CastViews\.countFor\(root\.castId\)/);
  assert.match(bar,/resolvedViewCount !== null && root\.resolvedViewCount !== undefined/);
  assert.match(bar,/LocaleFormat\.formatCompactNumber\(Number\(root\.resolvedViewCount\)\)/);
  assert.match(bar,/text: Theme\.iconPreview/);
  assert.match(bar,/Accessible\.role: Accessible\.StaticText/);
  assert.match(bar,/spacing: root\.compact \? 0 : Theme\.space1/);
  assert.match(provider,/return value === undefined \? null : value/);
  for(const name of ['CastItem.qml','ThreadReplyItem.qml','DetailSurface.qml']){
    const text=read('ui/components/'+name);
    assert.match(text,/castId: String\(root\.(?:modelData|reply|detail)\.castId \|\| ""\)/,name);
    assert.match(text,/authorFid: Number\(root\.(?:modelData|reply|detail)\.authorFid \|\| 0\)/,name);
    assert.match(text,/castText: String\(root\.(?:modelData|reply|detail)\.text \|\| ""\)/,name);
  }
});

test('verified Farcaster views are kept distinct from not-available and exact cast matching is enforced',()=>{
  const gateway=read('services/media-gateway/gateway.mjs');
  const worker=read('services/media-gateway/browser_cast_views.mjs');
  assert.match(worker,/https:\/\/api\.farcaster\.xyz\/v2\/casts\?fid=/);
  assert.match(worker,/https:\/\/api\.farcaster\.xyz\/v2\/search-casts\?q=/);
  assert.match(worker,/Number\(cast\?\.author\?\.fid\)===fid/);
  assert.match(worker,/String\(cast\?\.hash\|\|''\)\.toLowerCase\(\)===row\.castId/);
  assert.match(worker,/return Number\.isSafeInteger\(value\)&&value>=0\?value:null/);
  assert.match(gateway,/requestUrl\.pathname === '\/v1\/browser\/cast-views'/);
  assert.match(gateway,/readBody\(request, 5 \* 1024\)/);
  assert.match(gateway,/const castViewService = dependencies\.castViewService/);
});
