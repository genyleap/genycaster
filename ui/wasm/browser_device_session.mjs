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
export function createDeviceSession(browser=globalThis) {
    requireBrowser(browser);
    const crypto=browser.crypto;
    return Object.freeze({
        async remember(fid) {
            if(!validFid(fid))throw new Error('Invalid remembered Farcaster FID');
            // Every successful authentication rotates the local continuity key.
            const pair=await crypto.subtle.generateKey({name:'Ed25519'},false,['sign','verify']);
            if(pair.privateKey.extractable!==false)throw new Error('Device key is extractable');
            await access(browser,'readwrite',store=>store.put({
                fid,privateKey:pair.privateKey,publicKey:pair.publicKey,
                createdAt:Date.now(),version:1,
            },entry));
            return fid;
        },
        async restore() {
            const row=await access(browser,'readonly',store=>store.get(entry));
            if(!row)return null;
            if(!Number.isSafeInteger(row.createdAt) || row.createdAt<=0
                || row.createdAt>Date.now()+5*60*1000)
                throw new Error('Invalid local device session timestamp; sign in again');
            if(Date.now()-row.createdAt>maxDeviceAgeMs) {
                // Remove an expired remembered FID without touching separate
                // signer keys or user preferences; Setup will require sign-in.
                await access(browser,'readwrite',store=>store.delete(entry));
                return null;
            }
            if(row.version!==1||!validFid(row.fid)
                ||row.privateKey?.type!=='private'
                ||row.privateKey?.algorithm?.name!=='Ed25519'
                ||row.privateKey?.extractable!==false
                ||!row.privateKey?.usages?.includes('sign')
                ||row.publicKey?.type!=='public'
                ||row.publicKey?.algorithm?.name!=='Ed25519'
                ||!row.publicKey?.usages?.includes('verify'))
                throw new Error('Invalid local device session; sign in again');
            const challenge=new Uint8Array(32);
            crypto.getRandomValues(challenge);
            const payload=new Uint8Array(32+row.fid.length);
            payload.set(challenge);payload.set(new TextEncoder().encode(row.fid),32);
            const proof=await crypto.subtle.sign('Ed25519',row.privateKey,payload);
            if(!await crypto.subtle.verify('Ed25519',row.publicKey,proof,payload))
                throw new Error('Local device identity proof failed');
            return row.fid; // device-local viewer identity, not provider token
        },
        async clear() {
            await access(browser,'readwrite',store=>store.delete(entry));
        },
    });
}
