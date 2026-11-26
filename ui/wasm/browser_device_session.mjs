// Local trusted-device continuity after an already verified account sign-in.
// This is NOT an OAuth refresh token or fresh proof of server authentication.
// No custody phrase, wallet key, bearer token, or protocol signer key is stored.
const dbName='org.genycaster.browser-identity';
const storeName='device';
const entry='current';
const fidPattern=/^[1-9][0-9]{0,19}$/;
// Device-local continuity is not an OpenProof session or signer authorization.
// Require explicit authentication again after 30 days on this browser.
const maxDeviceAgeMs=30*24*60*60*1000;
function requireBrowser(browser) {
    if(!browser.isSecureContext||typeof browser.crypto?.subtle?.generateKey!=='function'
       ||typeof browser.indexedDB?.open!=='function')
        throw new Error('Trusted-device storage requires HTTPS, WebCrypto and IndexedDB');
}
function openDb(browser) {
    return new Promise((resolve,reject)=>{
        const req=browser.indexedDB.open(dbName,1);
        req.onupgradeneeded=()=>{
            const db=req.result;
            if(!db.objectStoreNames.contains(storeName))db.createObjectStore(storeName);
        };
        req.onsuccess=()=>resolve(req.result);
        req.onerror=()=>reject(req.error||new Error('Cannot open trusted-device storage'));
        req.onblocked=()=>reject(new Error('Trusted-device storage is blocked'));
    });
}
async function access(browser,mode,run) {
    const db=await openDb(browser);
    try{
        return await new Promise((resolve,reject)=>{
            const tx=db.transaction(storeName,mode);
            const req=run(tx.objectStore(storeName));
            let data;
            req.onsuccess=()=>{data=req.result;};
            req.onerror=()=>reject(req.error||new Error('Device session storage failed'));
            tx.oncomplete=()=>resolve(data);
            tx.onabort=()=>reject(tx.error||new Error('Device session storage aborted'));
            tx.onerror=()=>reject(tx.error||new Error('Device session storage failed'));
        });
    }finally{db.close();}
}
function validFid(value) {
    return typeof value==='string'&&fidPattern.test(value)&&BigInt(value)<=18446744073709551615n;
}
function sealPayload(row) {
    return new TextEncoder().encode(JSON.stringify([
        'genycaster-browser-device-v2',row.fid,row.protocol,row.createdAt]));
}
export function createDeviceSession(browser=globalThis) {
    requireBrowser(browser);
    const crypto=browser.crypto;
    async function readVerified() {
        const row=await access(browser,'readonly',store=>store.get(entry));
        if(!row || row.version===1)return null; // old metadata requires one verified login
        if(!Number.isSafeInteger(row.createdAt) || row.createdAt<=0
            || row.createdAt>Date.now()+5*60*1000)
            throw new Error('Invalid local device session timestamp; sign in again');
        if(Date.now()-row.createdAt>maxDeviceAgeMs) return null;
        if(row.version!==2||!validFid(row.fid)
            ||!['farcaster-snapchain','hypersnap-v2'].includes(row.protocol)
            ||(row.protocol==='hypersnap-v2'&&BigInt(row.fid)<4294967296n)
            ||row.privateKey?.type!=='private'
            ||row.privateKey?.algorithm?.name!=='Ed25519'
            ||row.privateKey?.extractable!==false
            ||!row.privateKey?.usages?.includes('sign')
            ||row.publicKey?.type!=='public'
            ||row.publicKey?.algorithm?.name!=='Ed25519'
            ||!row.publicKey?.usages?.includes('verify')
            ||!(row.seal instanceof ArrayBuffer)||row.seal.byteLength!==64)
            throw new Error('Invalid local device session; sign in again');
        if(!await crypto.subtle.verify('Ed25519',row.publicKey,row.seal,sealPayload(row)))
            throw new Error('Local device identity proof failed');
        const challenge=new Uint8Array(32);
        crypto.getRandomValues(challenge);
        const proof=await crypto.subtle.sign('Ed25519',row.privateKey,challenge);
        if(!await crypto.subtle.verify('Ed25519',row.publicKey,proof,challenge))
            throw new Error('Local device identity proof failed');
        return row;
    }
    return Object.freeze({
        async remember(fid, protocol='farcaster-snapchain') {
            if(!validFid(fid))throw new Error('Invalid remembered protocol FID');
            if(!['farcaster-snapchain','hypersnap-v2'].includes(protocol))
                throw new Error('Unrecognized browser identity protocol');
            if(protocol==='hypersnap-v2' && BigInt(fid)<4294967296n)
                throw new Error('Native HyperSnap FID is outside the registry namespace');
            // Every successful authentication rotates the local continuity key.
            const pair=await crypto.subtle.generateKey({name:'Ed25519'},false,['sign','verify']);
            if(pair.privateKey.extractable!==false)throw new Error('Device key is extractable');
            const row={fid,privateKey:pair.privateKey,publicKey:pair.publicKey,
                createdAt:Date.now(),version:2,protocol};
            row.seal=await crypto.subtle.sign('Ed25519',pair.privateKey,sealPayload(row));
            await access(browser,'readwrite',store=>store.put(row,entry));
            return fid;
        },
        async restore() {
            const row=await readVerified();
            // Do not delete on a stale read: another tab may have just saved
            // a new verified identity. Expired or legacy rows grant no access.
            return row?.fid||null;
        },
        async protocolFor(fid) {
            if(!validFid(fid))throw new Error('Invalid local identity');
            const row=await readVerified();
            if(!row || row.fid!==fid)
                throw new Error('Local protocol identity has changed');
            return row.protocol;
        },
        async clear() {
            await access(browser,'readwrite',store=>store.delete(entry));
        },
    });
}
