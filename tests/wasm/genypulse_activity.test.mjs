import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
import {resolve, dirname} from 'node:path';
import {runInNewContext} from 'node:vm';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const qml = readFileSync(resolve(root, 'ui/components/GenyPulsePanel.qml'), 'utf8');

function functionSource(name, nextName) {
  const begin = qml.indexOf('    function ' + name + '(');
  const end = qml.indexOf('    function ' + nextName + '(', begin);
  assert.ok(begin >= 0 && end > begin, 'Missing ' + name);
  return qml.slice(begin, end).trim();
}

const aggregate = functionSource('aggregateCasts', 'connectionState');
const connections = functionSource('connectionState', 'bucketCount');

function compute(context) {
  return runInNewContext(aggregate + '\naggregateCasts()', {root: context, isFinite});
}
function status(context, provider) {
  return runInNewContext(connections + '\nconnectionState(provider)', {
    root: context, provider
  });
}

test('Farcaster heatmap only counts unique author casts with valid timestamps', () => {
  const day = Math.floor(Date.now() / 86400000) - 3;
  const timestamp = day * 86400000 + 10_000;
  const rows = [
    {authorFid:42, castId:'0xa', createdAtMs:timestamp},
    {authorFid:42, castId:'0xa', createdAtMs:timestamp},
    {authorFid:42, castId:'0xb', createdAtMs:timestamp+100},
    {authorFid:43, castId:'0xc', createdAtMs:timestamp},
    {authorFid:42, castId:'0xd', createdAtMs:123}
  ];
  const result = compute({rows, profileFid:42, feedSupported:true,
      profileFeed:'user-posts', startUtc:day-20, todayUtc:day+3});
  assert.equal(result.total,2);
  assert.equal(result.days,1);
  assert.equal(result.counts[day],2);
});

test('Missing or unsupported profile rows do not create invented streaks', () => {
  const result = compute({rows:[{authorFid:42,castId:'0xa',createdAtMs:Date.now()}],
      profileFid:42, feedSupported:false, profileFeed:'user-likes',
      startUtc:0, todayUtc:99999});
  assert.deepEqual(JSON.parse(JSON.stringify(result)),{counts:{},total:0,days:0});
  assert.ok(!qml.includes('currentStreak:'));
});

test('Only verified own-profile accounts expose linked state', () => {
  const connectionsValue=[{provider:'github'},{provider:'ethereum-wallet'}];
  assert.equal(status({isOwnProfile:true,connectionsVerified:true,
      connections:connectionsValue}, 'github'),'linked');
  assert.equal(status({isOwnProfile:true,connectionsVerified:true,
      connections:connectionsValue}, 'ethereum-wallet'),'linked');
  assert.equal(status({isOwnProfile:false,connectionsVerified:true,
      connections:connectionsValue},'github'),'unavailable');
  assert.equal(status({isOwnProfile:true,connectionsVerified:false,
      connections:connectionsValue},'github'),'unavailable');
  assert.equal(status({isOwnProfile:true,connectionsVerified:true,
      connections:[]},'github'),'not-linked');
});

test('Presentation remains provider-free and localization has all activity labels', () => {
  assert.ok(!qml.includes("XMLHttpRequest") && !qml.includes("fetch("));
  const keys=[...qml.matchAll(/genycaster\.activity\.[a-z_]+/g)].map(m=>m[0]);
  for(const lang of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
      'pt_BR','ru_RU','hi_IN','ja_JP','ko_KR','zh_CN']) {
    const ts=readFileSync(resolve(root,'ui/i18n/qml_'+lang+'.ts'),'utf8');
    for(const key of new Set(keys))
      assert.ok(ts.includes('id="'+key+'"'),lang+' missing '+key);
  }
});
