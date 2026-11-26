import test from 'node:test';
import assert from 'node:assert/strict';
import { webcrypto } from 'node:crypto';
import { runInNewContext } from 'node:vm';
import { createBrowserSignerVault } from '../../ui/wasm/secure_signer_vault.mjs';

class MemoryIndexedDB {
    constructor() { this.data = new Map(); }
    open(name, version) {
        assert.equal(name, 'org.genycaster.protocol-signer');
        assert.equal(version, 1);
        const request = {};
        queueMicrotask(() => {
            const source = this;
            const db = {
                objectStoreNames: { contains: store => store === 'keys' },
                transaction: (_, mode) => {
                    const txn = {
                        oncomplete: null, onabort: null, onerror: null,
                        abort() { this.onabort?.(); },
                        objectStore() {
                            function work(action) {
                                const r = {};
                                setImmediate(() => {
                                    try {
                                        r.result = action();
                                        r.onsuccess?.();
                                        setImmediate(() => txn.oncomplete?.());
                                    } catch (error) {
                                        r.error = error;
                                        r.onerror?.();
                                        setImmediate(() => txn.onabort?.());
                                    }
                                });
                                return r;
                            }
                            return {
                                get: fid => work(() => source.data.get(fid)),
                                add: (value, fid) => work(() => {
                                    assert.equal(mode, 'readwrite');
                                    if (source.data.has(fid)) {
                                        const error = new Error('Key already exists');
                                        error.name = 'ConstraintError';
                                        throw error;
                                    }
                                    source.data.set(fid, value);
                                    return fid;
                                }),
                                delete: fid => work(() => {
                                    assert.equal(mode, 'readwrite');
                                    source.data.delete(fid);
                                }),
                            };
                        },
                    };
                    return txn;
                },
                close() {},
            };
            request.result = db;
            request.onsuccess?.();
        });
        return request;
    }
}

const environment = storage => ({
    isSecureContext: true,
    crypto: webcrypto,
    indexedDB: storage,
});

test('non-extractable signer persists by FID and signs real canonical hashes', async () => {
    const storage = new MemoryIndexedDB();
    const first = createBrowserSignerVault(environment(storage));
    assert.equal(await first.load('42'), null);
    const created = await first.ensure('42');
    assert.equal(created.fid, '42');
    assert.match(created.publicKeyHex, /^[a-f0-9]{64}$/);
    const record = storage.data.get('42');
    assert.equal(record.privateKey.extractable, false);
    await assert.rejects(
        webcrypto.subtle.exportKey('pkcs8', record.privateKey),
        /not extractable|InvalidAccess/i,
    );

    // Simulate a refresh by constructing a completely separate adapter;
    // IndexedDB retains the key's structured-cloned CryptoKey.
    const reloaded = createBrowserSignerVault(environment(storage));
    assert.deepEqual(await reloaded.load('42'), created);
    assert.deepEqual(await reloaded.ensure('42'), created);
    const digest = new Uint8Array(20).fill(0x31);
    const signature = await reloaded.signHash('42', digest);
    assert.equal(signature.byteLength, 64);
    const publicKey = await webcrypto.subtle.importKey(
        'raw', Buffer.from(created.publicKeyHex, 'hex'),
        'Ed25519', true, ['verify']);
    assert.equal(await webcrypto.subtle.verify(
        'Ed25519', publicKey, signature, digest), true);
    assert.equal(await webcrypto.subtle.verify(
        'Ed25519', publicKey, signature, new Uint8Array(20)), false);
    await reloaded.erase('42');
    assert.equal(await first.load('42'), null);
    await assert.rejects(first.signHash('42', digest), /no signing key/);
});

test('keys are strictly separated by FID, with no silent plaintext fallback', async () => {
    const storage = new MemoryIndexedDB();
    const vault = createBrowserSignerVault(environment(storage));
    const left = await vault.ensure('12');
    const right = await vault.ensure('13');
    assert.notEqual(left.publicKeyHex, right.publicKeyHex);
    await assert.rejects(vault.ensure('0'), /invalid non-zero FID/);
    await assert.rejects(vault.ensure('-2'), /invalid non-zero FID/);
    await assert.rejects(vault.ensure('18446744073709551616'), /invalid non-zero FID/);
    await assert.rejects(vault.ensure(Number.MAX_SAFE_INTEGER + 2), /safe integer/);
    await assert.rejects(vault.ensure(12.2), /safe integer/);
    await assert.rejects(vault.signHash('12', new Uint8Array(64)), /exactly 20 bytes/);
    assert.throws(
        () => createBrowserSignerVault({ isSecureContext: false, crypto: webcrypto, indexedDB: storage }),
        /secure context/,
    );
    assert.throws(
        () => createBrowserSignerVault({ isSecureContext: true, crypto: webcrypto }),
        /IndexedDB/,
    );
});

test('concurrent tab registrations cannot overwrite persisted signer keys', async () => {
    const storage = new MemoryIndexedDB();
    const a = createBrowserSignerVault(environment(storage));
    const b = createBrowserSignerVault(environment(storage));
    const [first, second] = await Promise.all([a.ensure('4242'), b.ensure('4242')]);
    assert.deepEqual(first, second);
    assert.deepEqual(await a.load('4242'), first);
});

test('an insecure or tampered CryptoKey record fails closed', async () => {
    const storage = new MemoryIndexedDB();
    const vault = createBrowserSignerVault(environment(storage));
    const created = await vault.ensure('99');
    storage.data.get('99').publicKeyHex = 'f'.repeat(64);
    await assert.rejects(vault.load('99'), /public key mismatch/);
    const record = storage.data.get('99');
    record.publicKeyHex = created.publicKeyHex;
    record.privateKey = { ...record.privateKey, extractable: true };
    await assert.rejects(vault.signHash('99', new Uint8Array(20)),
        /invalid security properties/);
});

test('real gateway-sized multipart media challenges sign without weakening vault scope', async () => {
    const storage = new MemoryIndexedDB();
    const vault = createBrowserSignerVault(environment(storage));
    const fid = '4294967297';
    const {publicKeyHex} = await vault.ensure(fid);
    // Opaque server-issued multipart ticket is embedded in the exact PUT
    // pathname, making the signed eight-line message exceed the old 900-byte
    // ceiling even for a small PNG.
    const ticket = 'a'.repeat(660) + '.' + 'b'.repeat(43);
    const partPath = '/v1/uploads/' + ticket + '/parts/1';
    const encode = (method, path, signerKey = publicKeyHex,
                    challengeFid = fid, nonce = 'ab'.repeat(16)) =>
        new TextEncoder().encode([
            'genycaster-media-upload-v1', method, path, challengeFid,
            signerKey, '1795471234', nonce, 'cd'.repeat(32),
        ].join('\n'));
    const multipart = encode('PUT', partPath);
    assert.ok(multipart.byteLength > 900 && multipart.byteLength < 4096);
    const result = await vault.signMediaChallenge(fid, multipart);
    assert.equal(result.publicKeyHex, publicKeyHex);
    const publicKey = await webcrypto.subtle.importKey(
        'raw', Buffer.from(publicKeyHex, 'hex'), 'Ed25519', true, ['verify']);
    assert.equal(await webcrypto.subtle.verify(
        'Ed25519', publicKey, Buffer.from(result.signature, 'hex'), multipart), true);
    // The other gateway routes retain the same canonical signature protocol.
    for (const [method, path] of [
        ['POST','/v1/uploads'], ['POST','/v1/uploads/complete'],
        ['DELETE','/v1/uploads'], ['DELETE','/v1/assets'],
    ]) {
        const signed = await vault.signMediaChallenge(fid, encode(method, path));
        assert.match(signed.signature, /^[a-f0-9]{128}$/);
    }
    for (const unsafe of [
        encode('PUT', '/admin/' + ticket + '/parts/1'),
        encode('GET', partPath),
        encode('PUT', '/v1/uploads/' + ticket + '/parts/0'),
        encode('PUT', partPath, publicKeyHex, '123'),
        encode('PUT', partPath, 'ff'.repeat(32)),
        encode('POST', '/v1/uploads', publicKeyHex, fid, 'not-a-nonce'),
        new Uint8Array(4097),
    ]) {
        await assert.rejects(vault.signMediaChallenge(fid, unsafe),
            /invalid media challenge|media challenge signer key mismatch/);
    }
});

test('223 KiB Stats PNG completes a multipart upload using real browser vault signatures', async () => {
    const {uploadBrowserImage} = await import('../../ui/wasm/browser_media_upload.mjs');
    const fid = '4294967297';
    const vault = createBrowserSignerVault(environment(new MemoryIndexedDB()));
    const signer = await vault.ensure(fid);
    const publicKey = await webcrypto.subtle.importKey(
        'raw', Buffer.from(signer.publicKeyHex,'hex'), 'Ed25519', true, ['verify']);
    const bytes = new Uint8Array(223*1024);
    const file = {
        name:'Genycaster-Stats-1795438652836.png', type:'image/png', size:bytes.length,
        slice:(from,to)=>new Blob([bytes.subarray(from,to)]),
    };
    const ticket = 'a'.repeat(660)+'.'+'b'.repeat(43);
    const assetUrl='https://media-farcaster.genyleap.com/m/v1/2026/11/stats.png';
    const sizes=[];
    const fetchImpl=async(url,opts)=>{
        const path=new URL(url).pathname;
        const digest=Buffer.from(await webcrypto.subtle.digest('SHA-256',opts.body)).toString('hex');
        const challenge=new TextEncoder().encode([
            'genycaster-media-upload-v1', opts.method, path, fid, signer.publicKeyHex,
            opts.headers['x-genycaster-timestamp'],opts.headers['x-genycaster-nonce'],digest,
        ].join('\n'));
        sizes.push(challenge.byteLength);
        assert.equal(await webcrypto.subtle.verify('Ed25519',publicKey,
            Buffer.from(opts.headers['x-genycaster-signature'],'hex'),challenge),true);
        if(path==='/v1/uploads' && opts.method==='POST')
            return new Response(JSON.stringify({
                uploadId:ticket, mode:'gateway-multipart', partCount:1,
                partSizeBytes:1024*1024, assetUrl, optimizationMode:'original',
            }),{status:201});
        if(path.endsWith('/parts/1') && opts.method==='PUT')
            return new Response(JSON.stringify({partNumber:1,etag:'"part-1"'}),{status:200});
        if(path==='/v1/uploads/complete' && opts.method==='POST')
            return new Response(JSON.stringify({
                assetUrl,sizeBytes:bytes.length,contentType:'image/png',
                fileName:file.name,
            }),{status:200});
        throw new Error('Unexpected mocked upload route '+path);
    };
    const result=await uploadBrowserImage({
        file,fid,vault,crypto:webcrypto,fetchImpl,optimizationMode:'original',
    });
    assert.equal(result.assetUrl,assetUrl);
    assert.equal(result.sizeBytes,223*1024);
    assert.equal(sizes.length,3);
    assert.ok(sizes[1]>900,'PUT multipart challenge reproduces old production error');
});

test('media signing accepts canonical Uint8Array bytes from a separate browser realm', async () => {
    const vault = createBrowserSignerVault(environment(new MemoryIndexedDB()));
    const fid = '4294967297';
    const signer = await vault.ensure(fid);
    const canonical = new TextEncoder().encode([
        'genycaster-media-upload-v1','POST','/v1/uploads',fid,
        signer.publicKeyHex,'1795471234','aa'.repeat(16),'cd'.repeat(32),
    ].join('\n'));
    const foreign = runInNewContext('new Uint8Array(bytes)', {
        bytes: [...canonical],
    });
    assert.equal(foreign instanceof Uint8Array, false,
        'different realms do not share Uint8Array constructors');
    assert.equal(ArrayBuffer.isView(foreign), true);
    const result = await vault.signMediaChallenge(fid,foreign);
    const publicKey = await webcrypto.subtle.importKey(
        'raw',Buffer.from(signer.publicKeyHex,'hex'),'Ed25519',true,['verify']);
    assert.equal(await webcrypto.subtle.verify('Ed25519',
        publicKey,Buffer.from(result.signature,'hex'),canonical),true);
    await assert.rejects(vault.signMediaChallenge(fid,new DataView(canonical.buffer)),
        /invalid media challenge size/);
    await assert.rejects(vault.signMediaChallenge(fid,new Int8Array(canonical)),
        /invalid media challenge size/);
});


test('cloud device keys are isolated from social signers and exact protocol',async()=>{
    const storage=new MemoryIndexedDB(),env=environment(storage),fid='4294967296';
    const social=createBrowserSignerVault(env);
    const cloud=createBrowserSignerVault(env,{scope:'cloud-device-farcaster'});
    const native=createBrowserSignerVault(env,{scope:'cloud-device-hypersnap'});
    const a=await social.ensure(fid),b=await cloud.ensure(fid),c=await native.ensure(fid);
    assert.equal(new Set([a.publicKeyHex,b.publicKeyHex,c.publicKeyHex]).size,3);
    const challenge={purpose:'restore',network:'farcaster',fid,
        public_key:b.publicKeyHex,nonce:'a'.repeat(43)};
    const proof=await cloud.signCloudDeviceChallenge(fid,challenge);
    const key=await webcrypto.subtle.importKey('raw',Buffer.from(b.publicKeyHex,'hex'),
        'Ed25519',true,['verify']);
    const bytes=new TextEncoder().encode(['genycaster-cloud-device-v2','restore','farcaster',
        fid,b.publicKeyHex,challenge.nonce].join('\n'));
    assert.equal(await webcrypto.subtle.verify('Ed25519',key,Buffer.from(proof.signature,'hex'),bytes),true);
    await assert.rejects(social.signCloudDeviceChallenge(fid,challenge),/invalid cloud device/);
    await assert.rejects(native.signCloudDeviceChallenge(fid,challenge),/invalid cloud device/);
    await assert.rejects(cloud.signHash(fid,new Uint8Array(20)),/cloud device key cannot/);
    await assert.rejects(cloud.signAccountChallenge(fid,challenge.nonce),/cloud device key cannot/);
    await cloud.erase(fid);
    assert.equal(await cloud.load(fid),null);
    assert.deepEqual(await social.load(fid),a);
    assert.deepEqual(await native.load(fid),c);
});


test('offline retirement persists a revocation-only receipt before erasing the cloud key',async()=>{
    const {retireBrowserCloudDevice,flushBrowserCloudRevocations,finishBrowserCloudRetirements}
        =await import('../../ui/wasm/browser_account_session.mjs');
    const storage=new MemoryIndexedDB(),saved=new Map();
    let failWrite=false;
    const localStorage={get length(){return saved.size;},key:i=>[...saved.keys()][i]??null,
        getItem:k=>saved.get(k)??null,removeItem:k=>saved.delete(k),
        setItem(k,v){if(failWrite&&k.includes('cloud-revocation.'))throw Error('storage full');saved.set(k,v);}};
    const env={...environment(storage),localStorage},fid='42',network='farcaster';
    const vault=createBrowserSignerVault(env,{scope:'cloud-device-farcaster'});
    const initial=await vault.ensure(fid);
    failWrite=true;
    await assert.rejects(retireBrowserCloudDevice({fid,network,environment:env,vault}),/storage full/);
    assert.deepEqual(await vault.load(fid),initial);
    assert.equal(saved.has('genycaster.cloud-retirement.farcaster.42'),true);
    failWrite=false;
    await finishBrowserCloudRetirements({environment:env});
    assert.equal(await vault.load(fid),null);
    const slot='genycaster.cloud-revocation.'+initial.publicKeyHex;
    const receipt=JSON.parse(saved.get(slot));
    assert.equal(receipt.purpose,'revoke');
    const publicKey=await webcrypto.subtle.importKey('raw',Buffer.from(initial.publicKeyHex,'hex'),
        'Ed25519',true,['verify']);
    const bytes=new TextEncoder().encode(['genycaster-cloud-device-v2','revoke',network,
        fid,initial.publicKeyHex,'A'.repeat(43)].join('\n'));
    assert.equal(await webcrypto.subtle.verify('Ed25519',publicKey,Buffer.from(receipt.signature,'hex'),bytes),true);
    await assert.rejects(flushBrowserCloudRevocations({environment:env,origin:'https://genycaster.xyz',
        fetchImpl:async()=>{throw Error('offline');}}),/offline/);
    assert.equal(saved.has(slot),true);
    await flushBrowserCloudRevocations({environment:env,origin:'https://genycaster.xyz',
        fetchImpl:async()=>new Response('{}',{status:409})});
    assert.equal(saved.has(slot),true);
    const next=await vault.ensure(fid);
    assert.notEqual(next.publicKeyHex,initial.publicKeyHex);
    await flushBrowserCloudRevocations({environment:env,origin:'https://genycaster.xyz',
        fetchImpl:async(path,options)=>{
            assert.equal(path,'/api/v1/cloud/session/device/revoke');
            assert.equal(options.credentials,'omit');assert.equal(options.headers.authorization,undefined);
            assert.deepEqual(JSON.parse(options.body),receipt);
            return Response.json({revoked:true});
        }});
    assert.equal(saved.has(slot),false);
    assert.deepEqual(await vault.load(fid),next);
});
