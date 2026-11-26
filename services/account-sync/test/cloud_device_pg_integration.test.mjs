import test from 'node:test';
import assert from 'node:assert/strict';
import {randomBytes} from 'node:crypto';
import {createPgStore} from '../pg_store.mjs';

// Explicitly provision and migrate a disposable database named genycaster_qa.
// Never run this destructive fixture on production, even if configured there.
const url=process.env.GENYCASTER_QA_DATABASE_URL;
const token=()=>randomBytes(32).toString('hex');
const csrf=()=>randomBytes(32).toString('base64url');

test('real PostgreSQL cloud devices: exact owner, rotation, expiry, replay and concurrent revoke', {
  skip: !url
},async()=>{
  const endpoint=new URL(url);
  assert.equal(endpoint.pathname,'/genycaster_qa','requires isolated QA database');
  assert.ok(['127.0.0.1','localhost','[::1]'].includes(endpoint.hostname));
  const store=await createPgStore(url);
  const fid=(2n**32n+BigInt('0x'+randomBytes(5).toString('hex'))).toString();
  const owner={issuer:'https://openproof.qa.invalid',subject:'farcaster:'+fid};
  const other={issuer:'hypersnap:native-registry',subject:'hypersnap:'+fid};
  const key=token(), replacement=token(), expiredKey=token();
  const create=(deviceKey,hash)=>store.createSession({...owner,deviceKey,authSource:'device'},hash,csrf());
  try {
    assert.equal(store.cloudDevicesReady(),true);
    assert.equal(await store.hasCloudDeviceHistory(owner),false);
    assert.equal(await store.revokeCloudDevice(owner,key),false);
    assert.equal(await store.hasCloudDeviceHistory(owner),false);
    await store.enrollCloudDevice(owner,key);
    assert.equal(await store.hasCloudDeviceHistory(owner),true);
    assert.equal(await store.touchCloudDevice(other,key),false);
    assert.equal(await store.revokeCloudDevice(other,key),false);
    await assert.rejects(store.enrollCloudDevice(other,key),e=>e.code==='23505');
    const old=token(),rotated=token();
    await create(key,old);
    assert.equal((await store.getSession(old)).auth_source,'device');
    await create(key,rotated);
    assert.equal(await store.getSession(old),null);
    assert.equal((await store.getSession(rotated)).subject,owner.subject);
    const independent=token();
    await store.createSession({...owner,authSource:'openproof'},independent,csrf());
    const racing=token();
    const [creation,revocation]=await Promise.allSettled([
      create(key,racing),store.revokeCloudDevice(owner,key)
    ]);
    assert.equal(revocation.status,'fulfilled');
    assert.equal(revocation.value,true);
    if(creation.status==='rejected')assert.match(creation.reason.message,/device_not_enrolled/);
    assert.equal(await store.getSession(rotated),null);
    assert.equal(await store.getSession(racing),null);
    assert.equal((await store.getSession(independent)).auth_source,'openproof');
    assert.equal(await store.touchCloudDevice(owner,key),false);
    assert.equal(await store.hasCloudDeviceHistory(owner),true);
    await assert.rejects(store.enrollCloudDevice(owner,key),/device_revoked/);
    await store.enrollCloudDevice(owner,replacement);
    const newSession=token();
    await create(replacement,newSession);
    assert.equal(await store.revokeCloudDevice(owner,key),true);
    assert.equal((await store.getSession(newSession)).device_key,replacement);
    await store.enrollCloudDevice(owner,expiredKey);
    const pg=await import('pg');
    const client=new pg.default.Client({connectionString:url});
    await client.connect();
    try {
      await client.query("UPDATE cloud_device_credentials SET last_used_at=now()-interval '91 days' WHERE public_key=$1",[expiredKey]);
    }finally{await client.end();}
    assert.equal(await store.touchCloudDevice(owner,expiredKey),false);
    await assert.rejects(create(expiredKey,token()),/device_not_enrolled/);
    assert.equal(await store.revokeCloudDevice(owner,replacement),true);
    assert.equal(await store.getSession(newSession),null);
  }finally{await store.close();}
});
