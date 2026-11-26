// Browser implementation of the secure *key* boundary, not a protocol
// authorization service. Never interpret possession of an Ed25519 key as
// permission to publish Farcaster messages.
const databaseName = 'org.genycaster.protocol-signer';
const storeName = 'keys';
const digestSize = 20;
const publicKeySize = 32;
const signatureSize = 64;

function failure(message) {
    return new Error('Browser signer vault: ' + message);
}

function requireFid(fid) {
    // JavaScript numbers above MAX_SAFE_INTEGER lose FID precision.
    // Require decimal strings (or exact safe integers) at the boundary.
    if (typeof fid !== 'string' &&
        !(typeof fid === 'number' && Number.isSafeInteger(fid))) {
        throw failure('FID must be a decimal string or a safe integer');
    }
    const value = String(fid);
    if (!/^[1-9][0-9]{0,19}$/.test(value) ||
        BigInt(value) > 18446744073709551615n) {
        throw failure('invalid non-zero FID');
    }
    return value;
}

function requireSecureContext(environment) {
    // Keep separate errors: an untrustworthy emrun origin cannot be fixed by
    // wallet approval, while a storage-disabled browser needs a different fix.
    if (environment.isSecureContext !== true) {
        const localBindAddress = environment.location?.hostname === '0.0.0.0';
        throw failure(localBindAddress
            ? 'insecure origin 0.0.0.0: open the app at http://127.0.0.1 on the same port, then retry. Browser WebCrypto requires a secure context'
            : 'this page is not a secure context. Use HTTPS or the loopback addresses http://127.0.0.1 / http://localhost');
    }

    let subtleAvailable = false;
    let storageAvailable = false;
    try { subtleAvailable = typeof environment.crypto?.subtle?.sign === 'function'; }
    catch { /* deny rather than silently downgrading signing */ }
    try { storageAvailable = typeof environment.indexedDB?.open === 'function'; }
    catch { /* access can throw SecurityError when storage is restricted */ }

    if (!subtleAvailable) {
        throw failure('WebCrypto SubtleCrypto is unavailable. Check browser security settings and the current origin');
    }
    if (!storageAvailable) {
        throw failure('IndexedDB is unavailable or blocked. Allow persistent site storage or use a normal (non-private) browser window');
    }
}

function waitRequest(request) {
    return new Promise((resolve, reject) => {
        request.onsuccess = () => resolve(request.result);
        request.onerror = () => reject(request.error || failure('IndexedDB request failed'));
    });
}

function openDatabase(indexedDB) {
    return new Promise((resolve, reject) => {
        const request = indexedDB.open(databaseName, 1);
        request.onupgradeneeded = () => {
            const db = request.result;
            if (!db.objectStoreNames.contains(storeName)) db.createObjectStore(storeName);
        };
        request.onsuccess = () => resolve(request.result);
        request.onerror = () => reject(request.error || failure('IndexedDB open failed'));
        request.onblocked = () => reject(failure('IndexedDB upgrade is blocked'));
    });
}

async function transact(indexedDB, mode, operation) {
    const db = await openDatabase(indexedDB);
    try {
        const transaction = db.transaction(storeName, mode);
        const completed = new Promise((resolve, reject) => {
            transaction.oncomplete = resolve;
            transaction.onerror = () => reject(transaction.error || failure('IndexedDB transaction failed'));
            transaction.onabort = () => reject(transaction.error || failure('IndexedDB transaction aborted'));
        });
        let value;
        try { value = await operation(transaction.objectStore(storeName)); }
        catch (error) {
            try { transaction.abort(); } catch { /* already inactive */ }
            await completed.catch(() => {});
            throw error;
        }
        await completed;
        return value;
    } finally {
        db.close();
    }
}

function encodeHex(array) {
    return [...array].map(value => value.toString(16).padStart(2, '0')).join('');
}

async function validateRecord(crypto, fid, record) {
    if (!record) return null;
    const { privateKey, publicKey } = record;
    // Stored CryptoKeys must be non-extractable at the private boundary.
    if (!privateKey || privateKey.type !== 'private' ||
        privateKey.algorithm?.name !== 'Ed25519' ||
        privateKey.extractable !== false ||
        !privateKey.usages.includes('sign') ||
        !publicKey || publicKey.type !== 'public' ||
        publicKey.algorithm?.name !== 'Ed25519' ||
        !publicKey.usages.includes('verify')) {
        throw failure('persisted signing key has invalid security properties');
    }
    const raw = new Uint8Array(await crypto.subtle.exportKey('raw', publicKey));
    if (raw.byteLength !== publicKeySize) throw failure('invalid Ed25519 public key');
    const publicKeyHex = encodeHex(raw);
    if (record.fid !== fid || record.publicKeyHex !== publicKeyHex) {
        throw failure('persisted signer identity and public key mismatch');
    }
    return { fid, publicKeyHex, privateKey, publicKey };
}

/**
 * Browser key vault with the same per-FID isolation as the desktop vault.
 * The returned API is asynchronous, because WebCrypto and IndexedDB must
 * never block the Qt WebAssembly GUI thread.
 *
 * This is a cryptographic building block. KEY_ADD custody authorization,
 * network registration, scopes and signed protocol submission are separate
 * required steps before exposing write capability to the user.
 */
export function createBrowserSignerVault(environment = globalThis, options = {}) {
    requireSecureContext(environment);
    const { crypto, indexedDB } = environment;
    // Historical Farcaster keys retain their original IndexedDB primary key.
    // HyperSnap keys are scoped by protocol to prevent a numerical FID
    // collision from reusing authority issued by another chain.
    const scope = options.scope === undefined ? '' : options.scope;
    const cloudNetwork=scope==='cloud-device-farcaster'?'farcaster':
        scope==='cloud-device-hypersnap'?'hypersnap':null;
    if(scope !== '' && scope !== 'hypersnap-v2' && !cloudNetwork)
        throw failure('unsupported protocol signer vault scope');
    const keyOf = fid => scope ? scope + ':' + fid : fid;
    const read = async fid => validateRecord(crypto, fid,
        await transact(indexedDB, 'readonly', store => waitRequest(store.get(keyOf(fid)))));

    return Object.freeze({
        async load(fidValue) {
            const fid = requireFid(fidValue);
            const record = await read(fid);
            return record ? { fid, publicKeyHex: record.publicKeyHex } : null;
        },

        async ensure(fidValue) {
            const fid = requireFid(fidValue);
            const existing = await read(fid);
            if (existing) return { fid, publicKeyHex: existing.publicKeyHex };
            // The private CryptoKey is never exportable, even at creation.
            const pair = await crypto.subtle.generateKey(
                { name: 'Ed25519' }, false, ['sign', 'verify']);
            const publicKeyHex = encodeHex(new Uint8Array(
                await crypto.subtle.exportKey('raw', pair.publicKey)));
            if (publicKeyHex.length !== publicKeySize * 2 ||
                pair.privateKey.extractable) {
                throw failure('cryptographic provider returned an unsafe key pair');
            }
            try {
                // add(), not put(): another tab must not overwrite a key
                // that a concurrent signer-registration flow might use.
                await transact(indexedDB, 'readwrite', store => waitRequest(
                    store.add({ fid, publicKeyHex,
                        privateKey: pair.privateKey, publicKey: pair.publicKey }, keyOf(fid))));
            } catch (error) {
                if (error?.name !== 'ConstraintError') throw error;
                const winner = await read(fid);
                if (!winner) throw failure('concurrent key creation could not be resolved');
                return { fid, publicKeyHex: winner.publicKeyHex };
            }
            return { fid, publicKeyHex };
        },

        async signHash(fidValue, hash) {
            if(cloudNetwork)throw failure('cloud device key cannot sign social or media messages');
            const fid = requireFid(fidValue);
            if (!(hash instanceof Uint8Array) || hash.byteLength !== digestSize) {
                throw failure('canonical Farcaster hash must be exactly 20 bytes');
            }
            const record = await read(fid);
            if (!record) throw failure('no signing key exists for this FID');
            const signature = new Uint8Array(
                await crypto.subtle.sign('Ed25519', record.privateKey, hash));
            if (signature.byteLength !== signatureSize ||
                !await crypto.subtle.verify('Ed25519',
                    record.publicKey, signature, hash)) {
                throw failure('Ed25519 signature self-verification failed');
            }
            return signature;
        },

        // The account-session capability is independent of Farcaster
        // message signatures and media uploads. Only this exact
        // domain-separated nonce challenge is permitted.
        async signAccountChallenge(fidValue, nonce) {
            if(cloudNetwork)throw failure('cloud device key cannot sign social or media messages');
            const fid=requireFid(fidValue);
            if(typeof nonce!=='string'||!/^[A-Za-z0-9_-]{43}$/.test(nonce))
                throw failure('invalid account-session challenge');
            const challenge=new TextEncoder().encode(
                'genycaster-account-session-v1\n'+fid+'\n'+nonce);
            const record=await read(fid);
            if(!record)throw failure('authorized signing key unavailable');
            const signature=new Uint8Array(await crypto.subtle.sign(
                'Ed25519',record.privateKey,challenge));
            if(signature.length!==64 || !await crypto.subtle.verify(
                'Ed25519',record.publicKey,signature,challenge))
                throw failure('account-session proof self-verification failed');
            return {publicKeyHex:record.publicKeyHex,signature:encodeHex(signature)};
        },

        async signCloudDeviceChallenge(fidValue,challenge) {
            const fid=requireFid(fidValue);
            const {purpose,network,public_key,nonce}=challenge||{};
            if(!cloudNetwork||network!==cloudNetwork||challenge.fid!==fid||
                !['enroll','restore'].includes(purpose)||
                typeof public_key!=='string'||!/^[0-9a-f]{64}$/.test(public_key)||
                typeof nonce!=='string'||!/^[A-Za-z0-9_-]{43}$/.test(nonce))
                throw failure('invalid cloud device challenge');
            const record=await read(fid);
            if(!record||record.publicKeyHex!==public_key)
                throw failure('cloud device key changed');
            const bytes=new TextEncoder().encode([
                'genycaster-cloud-device-v2',purpose,network,fid,public_key,nonce].join('\n'));
            const signature=new Uint8Array(await crypto.subtle.sign('Ed25519',record.privateKey,bytes));
            if(signature.length!==64||!await crypto.subtle.verify('Ed25519',record.publicKey,signature,bytes))
                throw failure('cloud device proof self-verification failed');
            return {signature:encodeHex(signature)};
        },

        // A narrowly scoped media proof is not a Farcaster cast signature.
        // Do not expose an arbitrary-message signer to UI code.
        async signMediaChallenge(fidValue, challenge) {
            if(cloudNetwork)throw failure('cloud device key cannot sign social or media messages');
            const fid = requireFid(fidValue);
            // Upload tickets are HMAC-authenticated opaque tokens embedded in
            // the canonical PUT pathname. A valid gateway-issued v3 ticket
            // can push the eight-line challenge beyond 900 bytes (928 bytes
            // observed in production); this is not the image's byte length.
            // Keep a strict bound AND validate every field and allowed route,
            // rather than widening an arbitrary-message signing capability.
            // A JS Uint8Array can cross a browser realm (WASM host/iframe):
            // instanceof Uint8Array then fails despite valid signed bytes.
            // ArrayBuffer.isView is realm-independent; reject DataView and all
            // non-byte typed arrays before examining the canonical request.
            const canonicalByteView = ArrayBuffer.isView(challenge)
                && challenge.BYTES_PER_ELEMENT === 1
                && Object.prototype.toString.call(challenge) === '[object Uint8Array]';
            const size = canonicalByteView ? challenge.byteLength : 0;
            if (!canonicalByteView || size > 4096 || size < 190)
                throw failure('invalid media challenge size (' + size + ' bytes)');
            const prefix = new TextEncoder().encode('genycaster-media-upload-v1\n');
            if (!prefix.every((value,index) => challenge[index] === value))
                throw failure('media challenge domain mismatch');
            const fields = new TextDecoder('utf-8', {fatal:true})
                .decode(challenge).split('\n');
            const [domain,method,path,challengeFid,signerKey,timestamp,nonce,digest] = fields;
            const routeAllowed =
                (method === 'POST' && (path === '/v1/uploads'
                    || path === '/v1/uploads/complete'))
                || (method === 'DELETE' && (path === '/v1/uploads'
                    || path === '/v1/assets'))
                || (method === 'PUT' && /^\/v1\/uploads\/[A-Za-z0-9_-]{16,3500}\.[A-Za-z0-9_-]{43}\/parts\/[1-9][0-9]{0,4}$/.test(path));
            if (fields.length !== 8 || domain !== 'genycaster-media-upload-v1'
                || !routeAllowed || challengeFid !== fid
                || !/^[a-f0-9]{64}$/.test(signerKey || '')
                || !/^[1-9][0-9]{9,10}$/.test(timestamp || '')
                || !/^[a-f0-9]{32}$/.test(nonce || '')
                || !/^[a-f0-9]{64}$/.test(digest || ''))
                throw failure('invalid media challenge format');
            const record = await read(fid);
            if (!record) throw failure('authorized media signer unavailable');
            if (signerKey !== record.publicKeyHex)
                throw failure('media challenge signer key mismatch');
            const signature = new Uint8Array(
                await crypto.subtle.sign('Ed25519', record.privateKey, challenge));
            if (signature.length !== signatureSize || !await crypto.subtle.verify(
                'Ed25519', record.publicKey, signature, challenge))
                throw failure('media proof self-verification failed');
            return { publicKeyHex: record.publicKeyHex, signature: encodeHex(signature) };
        },

        async erase(fidValue) {
            const fid = requireFid(fidValue);
            await transact(indexedDB, 'readwrite', store =>
                waitRequest(store.delete(keyOf(fid))));
        },
    });
}
