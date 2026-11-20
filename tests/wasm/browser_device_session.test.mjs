import test from 'node:test';
import assert from 'node:assert/strict';
import { webcrypto } from 'node:crypto';
import { createDeviceSession } from '../../ui/wasm/browser_device_session.mjs';

function fakeBrowser() {
    const table=new Map();
    const db={
        objectStoreNames:{contains:name=>name==='device'},
        createObjectStore(){},
        close(){},
        transaction(name,mode) {
            assert.equal(name,'device');
            const tx={oncomplete:null,onerror:null,onabort:null};
            const action=(method,key,value)=>{
                const request={onsuccess:null,onerror:null};
                queueMicrotask(()=>{
                    if(method==='get')request.result=table.get(key);
                    else if(method==='put')table.set(key,value);
                    else if(method==='delete')table.delete(key);
                    request.onsuccess?.();
                    queueMicrotask(()=>tx.oncomplete?.());
                });
                return request;
            };
            tx.objectStore=()=>({
                get:key=>action('get',key),
                put:(value,key)=>action('put',key,value),
                delete:key=>action('delete',key)
            });
            return tx;
        }
    };
    const indexedDB={
        open() {
            const request={result:db,onsuccess:null,onerror:null,onblocked:null};
            queueMicrotask(()=>request.onsuccess?.());
            return request;
        }
    };
    return {browser:{isSecureContext:true,crypto:webcrypto,indexedDB},table};
}

test('local device continuity signs and verifies an ephemeral challenge without exporting private key',async()=>{
    const {browser,table}=fakeBrowser();
    const first=createDeviceSession(browser);
    assert.equal(await first.restore(),null);
    await first.remember('42');
    const entry=table.get('current');
    assert.equal(entry.privateKey.type,'private');
    assert.equal(entry.privateKey.extractable,false);
    assert.equal(entry.privateKey.algorithm.name,'Ed25519');
    assert.ok(!('phrase' in entry));
    assert.ok(!('token' in entry));
    assert.equal(await createDeviceSession(browser).restore(),'42');
    await first.clear();
    assert.equal(await createDeviceSession(browser).restore(),null);
});
test('tampered device key pairing fails local proof',async()=>{
    const {browser,table}=fakeBrowser();
    const keyStore=createDeviceSession(browser);
    await keyStore.remember('42');
    const original=table.get('current');
    const other=await webcrypto.subtle.generateKey({name:'Ed25519'},false,['sign','verify']);
    table.set('current',{...original,publicKey:other.publicKey});
    await assert.rejects(keyStore.restore(),/identity proof failed/);
});
test('malformed device identity and unavailable secure storage fail closed',async()=>{
    const {browser,table}=fakeBrowser();
    const service=createDeviceSession(browser);
    await assert.rejects(service.remember('0'),/FID/);
    await service.remember('42');
    table.set('current',{...table.get('current'),fid:'arbitrary'});
    await assert.rejects(service.restore(),/Invalid local device session/);
    assert.throws(()=>createDeviceSession({...browser,isSecureContext:false}),/HTTPS/);
});

test('expired device continuity requires Setup again without retaining an expired FID',async()=>{
    const {browser,table}=fakeBrowser();
    const session=createDeviceSession(browser);
    await session.remember('42');
    table.set('current',{...table.get('current'),createdAt:Date.now()-31*24*60*60*1000});
    assert.equal(await session.restore(),null);
    assert.equal(table.has('current'),false);
});
test('future or invalid continuity timestamp is never trusted',async()=>{
    const {browser,table}=fakeBrowser();
    const session=createDeviceSession(browser);
    await session.remember('42');
    const original=table.get('current');
    table.set('current',{...original,createdAt:Date.now()+24*60*60*1000});
    await assert.rejects(session.restore(),/timestamp/);
    table.set('current',{...original,createdAt:'yesterday'});
    await assert.rejects(session.restore(),/timestamp/);
});

test('trusted device continuity preserves the exact HyperSnap namespace across reloads',async()=>{
    const {browser,table}=fakeBrowser();
    const session=createDeviceSession(browser);
    const nativeFid='4294967296';
    await session.remember(nativeFid,'hypersnap-v2');
    const restored=createDeviceSession(browser);
    assert.equal(await restored.restore(),nativeFid);
    assert.equal(await restored.protocolFor(nativeFid),'hypersnap-v2');
    await assert.rejects(restored.protocolFor('42'),/changed/);
    await assert.rejects(session.remember('42','hypersnap-v2'),/namespace/);
    await assert.rejects(session.remember('42','unknown-protocol'),/Unrecognized/);
    await session.remember('42');
    assert.equal(await restored.protocolFor('42'),'farcaster-snapchain');
    const old=table.get('current');
    table.set('current',{...old,protocol:undefined});
    assert.equal(await restored.protocolFor('42'),'farcaster-snapchain');
});
