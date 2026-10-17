import test from 'node:test';
import assert from 'node:assert/strict';
import { webcrypto } from 'node:crypto';
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
