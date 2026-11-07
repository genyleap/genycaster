import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';

const profile=readFileSync(new URL('../../ui/components/profile/ProfileSurface.qml',import.meta.url),'utf8');
const start=profile.indexOf('    function migrateLegacyProfileExtras() {');
const end=profile.indexOf('    onProfileExtrasOwnerKeyChanged:',start);
assert(start>=0 && end>start);
const migration=profile.slice(start,end);

function migrate({fid='123',issuer='https://openproof.genyleap.com',
  entries={},flags={},isOwnProfile=true}={}) {
  const data={entriesJson:JSON.stringify(entries)};
  const outbox={perAccountJson:JSON.stringify(flags)};
  const root={accountFid:fid,accountIssuer:issuer,
    profileExtrasOwnerKey:isOwnProfile?'farcaster:'+fid:''};
  runInNewContext(migration+'\nmigrateLegacyProfileExtras();',{
    root,profileExtrasSettings:data,profileCloudSettings:outbox,JSON,Array,Object,String
  });
  return {entries:JSON.parse(data.entriesJson),flags:JSON.parse(outbox.perAccountJson)};
}

test('route-specific legacy profile is accessible under canonical Farcaster FID',()=>{
  const legacy='https://openproof.genyleap.com:123';
  const original={profession:'Engineer',category:'Technology',
    links:['https://example.com'],labels:['Website']};
  const {entries}=migrate({entries:{[legacy]:original}});
  assert.deepEqual(entries['farcaster:123'],original);
  assert.deepEqual(entries[legacy],original);
});

test('unacknowledged device-local edit wins over older canonical account cache',()=>{
  const legacy='genycaster:device-local:123';
  const {entries,flags}=migrate({
    entries:{'farcaster:123':{profession:'Artist'},
      [legacy]:{profession:'Engineer'}},
    flags:{'farcaster:123':{dirty:false},[legacy]:{dirty:true}}
  });
  assert.equal(entries['farcaster:123'].profession,'Engineer');
  assert.equal(flags['farcaster:123'].dirty,true);
});

test('pending canonical edits are never overwritten by legacy data',()=>{
  const {entries}=migrate({entries:{
    'farcaster:123':{profession:'Engineer'},
    'local:123':{profession:'Artist'}
  },flags:{'farcaster:123':{dirty:true},'local:123':{dirty:true}}});
  assert.equal(entries['farcaster:123'].profession,'Engineer');
});

test('no account owner means no per-account data is migrated',()=>{
  const {entries}=migrate({
    isOwnProfile:false,entries:{'local:123':{profession:'Engineer'}}
  });
  assert.equal(entries['farcaster:123'],undefined);
});
