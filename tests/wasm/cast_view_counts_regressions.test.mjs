import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../../'+p,import.meta.url),'utf8');

test('the Qt Views controller batches and bounds network work outside QML',()=>{
  const cmake=read('ui/CMakeLists.txt');
  const facade=read('ui/components/CastViews.qml');
  const controller=read('ui/mobile/cast_view_controller.cpp');
  const header=read('ui/mobile/cast_view_controller.hpp');
  assert.match(cmake,/mobile\/cast_view_controller.cpp/);
  assert.match(facade,/pragma Singleton/);
  assert.doesNotMatch(facade,/XMLHttpRequest|JSON\.parse|revision/);
  assert.match(controller,/maxBatch = 16/);
  assert.match(controller,/maxPending = 96/);
  assert.match(controller,/maxEntries = 384/);
  assert.match(controller,/requestDeadlineMs = 8 \* 1000/);
  assert.match(controller,/QTimer::singleShot\(requestDeadlineMs, reply/);
  assert.match(controller,/storage-farcaster\.genyleap\.com\/v1\/browser\/cast-views/);
  assert.match(controller,/QJsonDocument::fromJson/);
  assert.match(header,/Q_PROPERTY\(QVariant count READ count NOTIFY countChanged FINAL\)/);
});

test('cast delegates retain and release per-cast analytics interest without global invalidation',()=>{
  const bar=read('ui/components/EngagementBar.qml');
  assert.match(bar,/castViewController\.ensure\(root\.descriptorCastId,/);
  assert.match(bar,/root\.viewDescriptor\.authorFid/);
  assert.match(bar,/root\.enrichmentActive/);
  assert.match(bar,/onCastDataChanged:/);
  assert.match(bar,/root\.castViewEntry\.count/);
  assert.match(bar,/castViewController\.retain\(next\)/);
  assert.match(bar,/castViewController\.release\(root\.retainedViewsCastId\)/);
  assert.match(bar,/resolvedViewCount !== null && root\.resolvedViewCount !== undefined/);
  assert.match(bar,/LocaleFormat\.formatCompactNumber\(Number\(root\.resolvedViewCount\)\)/);
  assert.match(bar,/text: Theme\.iconPreview/);
  assert.match(bar,/Accessible\.role: Accessible\.StaticText/);
  assert.match(bar,/spacing: root\.compact \? 0 : Theme\.space1/);
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
