// Browser protocol signer: custody-wallet approval + origin-isolated Ed25519
// + canonical messages + authoritative Snapchain verification.
// No service ever receives the private Ed25519 key or the custody mnemonic.
// This is a browser-only asynchronous counterpart to SignerService/WriteService.
import { createBrowserSignerVault } from './secure_signer_vault.mjs';
import {
    abiMetadata, farcasterTimestamp, fromHex, hex, keyAddTypedData,
    sealMessage, SOCIAL_SCOPES, unsignedCastAdd, unsignedCastRemove,
    unsignedFollow, unsignedKeyAdd, unsignedKeyRemove, unsignedReaction,
} from './farcaster_protocol.mjs';

const registryAddress = '0x00000000fc6c5f01fc30151999387bb99a9f489b';
const farcasterEpoch = 1609459200;
const mask = (1n<<64n)-1n;
const rotation = [
    0,1,62,28,27, 36,44,6,55,20, 3,10,43,25,39,
    41,45,15,21,8, 18,2,61,56,14,
];
const rc = [
    0x0000000000000001n,0x0000000000008082n,0x800000000000808an,
    0x8000000080008000n,0x000000000000808bn,0x0000000080000001n,
    0x8000000080008081n,0x8000000000008009n,0x000000000000008an,
    0x0000000000000088n,0x0000000080008009n,0x000000008000000an,
    0x000000008000808bn,0x800000000000008bn,0x8000000000008089n,
    0x8000000000008003n,0x8000000000008002n,0x8000000000000080n,
    0x000000000000800an,0x800000008000000an,0x8000000080008081n,
    0x8000000000008080n,0x0000000080000001n,0x8000000080008008n,
];
const rot=(word,by)=>by===0?word:((word<<BigInt(by))|(word>>BigInt(64-by)))&mask;
export function keccak256(data) {
    if (!(data instanceof Uint8Array)) throw new TypeError('Keccak input must be bytes');
    const rate=136;
    // Keccak pad10*1 uses a single byte 0x81 when exactly one slot remains.
    const input=new Uint8Array(Math.ceil((data.length+1)/rate)*rate);
    input.set(data);input[data.length]^=1;input[input.length-1]^=128;
    const lanes=Array(25).fill(0n);
    for(let offset=0;offset<input.length;offset+=rate) {
        for(let j=0;j<rate;j++) {
            const index=j>>3;
            lanes[index]^=BigInt(input[offset+j])<<BigInt((j%8)*8);
        }
        for(const constant of rc){
            const parity=Array(5);
            for(let x=0;x<5;x++) parity[x]=lanes[x]^lanes[x+5]^lanes[x+10]^lanes[x+15]^lanes[x+20];
            for(let x=0;x<5;x++) {
                const d=parity[(x+4)%5]^rot(parity[(x+1)%5],1);
                for(let y=0;y<5;y++)lanes[x+5*y]^=d;
            }
            const moved=Array(25);
            for(let y=0;y<5;y++)for(let x=0;x<5;x++)
                moved[y+5*((2*x+3*y)%5)]=rot(lanes[x+5*y],rotation[x+5*y]);
            for(let y=0;y<5;y++)for(let x=0;x<5;x++)
                lanes[x+5*y]=moved[x+5*y]^((~moved[(x+1)%5+5*y])&moved[(x+2)%5+5*y]);
            lanes[0]^=constant;
        }
    }
    const result=new Uint8Array(32);
    for(let i=0;i<32;i++)result[i]=Number((lanes[i>>3]>>BigInt(8*(i%8)))&255n);
    return result;
}
const encoder=new TextEncoder();
const custodySelector='0x'+hex(keccak256(encoder.encode('custodyOf(uint256)')).subarray(0,4));
function requireFid(value) {
    if (typeof value!=='string' && !(typeof value==='number'&&Number.isSafeInteger(value)))
        throw new Error('FID must be an exact decimal string');
    const s=String(value);
    if (!/^[1-9][0-9]{0,19}$/.test(s)||BigInt(s)>0xffffffffffffffffn)
        throw new Error('invalid Farcaster FID');
    return s;
}
function requireAddress(value) {
    if(typeof value!=='string'||!/^0x[0-9a-f]{40}$/i.test(value))
        throw new Error('wallet returned an invalid Ethereum address');
    return value.toLowerCase();
}
function validateBase(base) {
    const url=new URL(base);
    if(url.protocol!=='https:'||url.username||url.password||url.search||url.hash)
        throw new Error('Snapchain origin must be credential-free HTTPS');
    return url.origin;
}
function uint32(value,name) {
    const n=Number(value);
    if(!Number.isSafeInteger(n)||n<0||n>0xffffffff)
        throw new Error('invalid Snapchain '+name);
    return n;
}
function recordScopes(record) {
    const names=['MESSAGE_TYPE_CAST_ADD','MESSAGE_TYPE_CAST_REMOVE',
        'MESSAGE_TYPE_REACTION_ADD','MESSAGE_TYPE_REACTION_REMOVE',
        'MESSAGE_TYPE_LINK_ADD','MESSAGE_TYPE_LINK_REMOVE'];
    return names.map((name,i)=>record.scopes?.includes(name)?i+1:null).filter(x=>x!==null);
}
function signerRecord(record,fid,publicKey) {
    if(String(record.fid)!==fid||Number(record.keyType)!==1
        ||record.key?.toLowerCase()!=='0x'+publicKey)return null;
    if(record.source==='SIGNER_SOURCE_ONCHAIN')return {
        source:'onchain',fid,key:publicKey,scopes:SOCIAL_SCOPES,requestFid:fid,
    };
    if(record.source!=='SIGNER_SOURCE_OFFCHAIN')throw new Error('unknown signer registration type');
    const ttl=uint32(record.ttl,'TTL');
    const lastUsed=uint32(record.lastUsedAt,'last used');
    const expiresAt=uint32(record.expiresAt??(lastUsed+ttl),'expiry');
    const scopes=recordScopes(record);
    if(ttl===0||ttl>7776000||expiresAt!==lastUsed+ttl
        ||expiresAt<=farcasterTimestamp() || String(record.requestFid)!==fid
        ||SOCIAL_SCOPES.some(s=>!scopes.includes(s)))
        return null;
    return {source:'gasless',fid,key:publicKey,scopes,requestFid:fid,expiresAt};
}

export function createBrowserProtocolSigner({
    browser=globalThis, fetchImpl=globalThis.fetch,
    snapchainBase='https://snapchain.genyleap.com',
    vault=createBrowserSignerVault(browser), onState=()=>{},
    delay=ms=>new Promise(resolve=>setTimeout(resolve,ms)),
}={}) {
    const endpoint=validateBase(snapchainBase);
    if(typeof fetchImpl!=='function')throw new Error('fetch unavailable');
    let activeFid=null, activeKey=null, busy=false;
    let lastPhase='signed-out', lastError='';
    let generation=0;
    const verifySession=(fid,expectedGeneration)=>{
        if(fid!==activeFid||generation!==expectedGeneration)
            throw new Error('signing attempt invalidated by identity change');
    };

    const state=(phase,error='')=>{
        lastPhase=phase;lastError=error;
        onState({phase,fid:activeFid,publicKeyHex:activeKey,busy,
                 ready:phase==='ready'&&!busy,error});
    };
    const exclusive=async operation=>{
        if(busy)throw new Error('another signer operation is still running');
        busy=true;
        try {return await operation();}
        finally {busy=false;state(lastPhase,lastError);}
    };
    async function request(url,options={}){
        const response=await fetchImpl(endpoint+url,{
            ...options,credentials:'omit',redirect:'error',mode:'cors',
            signal:AbortSignal.timeout(16000),
        });
        if(!response.ok)throw new Error('Snapchain HTTP '+response.status+
            ': '+(await response.text()).slice(0,180));
        const text=await response.text();
        if(text.length>8*1024*1024)throw new Error('oversized Snapchain response');
        try{return JSON.parse(text);}
        catch {throw new Error('Snapchain returned non-JSON confirmation');}
    }
    async function inspect(fid,publicKey){
        let currentUserNonce=null,currentRequesterNonce=null,token='',match=null;
        const seen=new Set();
        for(let page=0;page<32;page++){
            const params=new URLSearchParams({fid,pageSize:'100'});
            params.append('requesterFids[0]',fid);
            if(token)params.set('pageToken',token);
            const result=await request('/v1/signersByFid?'+params.toString());
            if(!Array.isArray(result.signers))throw new Error('invalid signer registry response');
            currentUserNonce=uint32(result.currentUserNonce,'user nonce');
            // Some Snapchain gateway versions omit requesterFidNonces when
            // there are no gasless signer records. KEY_ADD uses *user* nonce,
            // while KEY_REMOVE must fail closed without a confirmed requester
            // nonce rather than guessing a replay counter.
            if(result.requesterFidNonces?.[fid]!==undefined)
                currentRequesterNonce=uint32(result.requesterFidNonces[fid],'requester nonce');
            for(const item of result.signers){
                const row=signerRecord(item,fid,publicKey);
                if(row){
                    if(match)throw new Error('duplicate signer entries in Snapchain');
                    match=row;
                }
            }
            if(!result.nextPageToken)break;
            token=String(result.nextPageToken);
            if(seen.has(token))throw new Error('repeated signer pagination');
            seen.add(token);
            if(page===31)throw new Error('signer pagination limit exceeded');
        }
        return {registration:match,userNonce:currentUserNonce,requesterNonce:currentRequesterNonce};
    }
    async function submit(unsigned,fid,expectedGeneration){
        verifySession(fid,expectedGeneration);
        const signature=await vault.signHash(fid,unsigned.hash);
        verifySession(fid,expectedGeneration);
        const envelope=sealMessage(unsigned,signature);
        const result=await request('/v1/submitMessage',{
            method:'POST',headers:{'content-type':'application/octet-stream',
                                    accept:'application/json'},body:envelope,
        });
        if(typeof result.hash!=='string' ||
            result.hash.toLowerCase()!=='0x'+hex(unsigned.hash))
            throw new Error('Snapchain submit response hash mismatch');
        // Native WriteSubmitter also checks the accepted message's author
        // and type. A matching hash alone is not a complete acknowledgment.
        const messageTypes={
            1:'MESSAGE_TYPE_CAST_ADD',2:'MESSAGE_TYPE_CAST_REMOVE',
            3:'MESSAGE_TYPE_REACTION_ADD',4:'MESSAGE_TYPE_REACTION_REMOVE',
            5:'MESSAGE_TYPE_LINK_ADD',6:'MESSAGE_TYPE_LINK_REMOVE',
            16:'MESSAGE_TYPE_KEY_ADD',17:'MESSAGE_TYPE_KEY_REMOVE',
        };
        const kind=unsigned.data[0]===8 ? unsigned.data[1] : undefined;
        if(!result.data||result.data.type!==messageTypes[kind]||
           String(result.data.fid)!==fid)
            throw new Error('Snapchain submit response type or FID mismatch');
        return '0x'+hex(unsigned.hash);
    }
    async function walletCustody(fid){
        const ethereum=browser.ethereum;
        if(!ethereum || typeof ethereum.request!=='function')
            throw new Error('Farcaster custody wallet extension is unavailable. Connect the wallet that owns your FID.');
        const accounts=await ethereum.request({method:'eth_requestAccounts'});
        if(!Array.isArray(accounts)||!accounts.length)throw new Error('custody wallet did not grant an account');
        const address=requireAddress(accounts[0]);
        const current=await ethereum.request({method:'eth_chainId'});
        if(current!=='0xa')
            await ethereum.request({method:'wallet_switchEthereumChain',params:[{chainId:'0xa'}]});
        const selector=custodySelector+BigInt(fid).toString(16).padStart(64,'0');
        const result=await ethereum.request({method:'eth_call',params:[
            {to:registryAddress,data:selector},'latest']});
        if(!/^0x[0-9a-f]{64}$/i.test(result))
            throw new Error('Optimism IdRegistry returned malformed custody data');
        const custody=requireAddress('0x'+result.slice(-40));
        if(custody===('0x'+'0'.repeat(40))||custody!==address)
            throw new Error('Selected wallet address does not own Farcaster FID '+fid);
        if((await ethereum.request({method:'eth_chainId'}))!=='0x1')
            await ethereum.request({method:'wallet_switchEthereumChain',params:[{chainId:'0x1'}]});
        return {address,
            signTypedData:document=>ethereum.request({
                method:'eth_signTypedData_v4',
                params:[address,JSON.stringify(document)],
            }),
        };
    }

    // The imported Farcaster recovery phrase is used ONLY to derive the
    // custody signer for initial authorization. It is never sent to a server
    // or placed in IndexedDB, Qt state, logs or a persistent browser cache.
    async function recoveryCustody(fid, phrase){
        const { deriveFarcasterCustody } = await import('./farcaster_recovery.mjs');
        const recovered=await deriveFarcasterCustody(phrase);
        try{
            // Public chain lookup from the canonical Optimism IdRegistry.
            // Do not trust the local mnemonic's address without checking FID.
            const rpc='https://mainnet.optimism.io';
            let requestId=0;
            const call=async(method,params)=>{
                const response=await fetchImpl(rpc,{
                    method:'POST',credentials:'omit',redirect:'error',mode:'cors',
                    headers:{'content-type':'application/json'},
                    signal:AbortSignal.timeout(16000),
                    body:JSON.stringify({jsonrpc:'2.0',method,params,id:++requestId}),
                });
                if(!response.ok)throw new Error('Optimism JSON-RPC HTTP '+response.status);
                const result=await response.json();
                if(result.error||typeof result.result!=='string')
                    throw new Error('Optimism RPC failed to verify custody');
                return result.result;
            };
            if((await call('eth_chainId',[])).toLowerCase()!=='0xa')
                throw new Error('Custody verification RPC is not Optimism mainnet');
            const selector=custodySelector+BigInt(fid).toString(16).padStart(64,'0');
            const result=await call('eth_call',[{
                to:registryAddress,data:selector,
            },'latest']);
            if(!/^0x[0-9a-f]{64}$/i.test(result))
                throw new Error('Optimism IdRegistry returned malformed custody data');
            const custody=requireAddress('0x'+result.slice(-40));
            if(custody!==requireAddress(recovered.address))
                throw new Error('This recovery phrase does not own the signed-in Farcaster FID');
            return recovered;
        }catch(error){recovered.dispose();throw error;}
    }

    async function restore(fidValue){
        const fid=requireFid(fidValue);
        return exclusive(async()=>{
            activeFid=fid;activeKey=null;const expectedGeneration=++generation;state('checking');
            try {
                const key=await vault.load(fid);
                if(!key){state('missing');return null;}
                activeKey=key.publicKeyHex;
                const observed=await inspect(fid,activeKey);
                verifySession(fid,expectedGeneration);
                if(observed.registration){state('ready');return observed.registration;}
                state('missing');return null;
            }catch(error){state('unavailable',String(error.message||error));throw error;}
        });
    }

    async function provision(fidValue, custodyProvider=walletCustody){
        const fid=requireFid(fidValue);
        return exclusive(async()=>{
            activeFid=fid;activeKey=null;const expectedGeneration=++generation;
            state('provisioning');
            let wallet=null;
            try {
                // No KEY_ADD can be requested without verifying the current
                // custody address on Optimism for this exact signed-in FID.
                wallet=await custodyProvider(fid);
                verifySession(fid,expectedGeneration);
                const key=await vault.ensure(fid);
                activeKey=key.publicKeyHex;
                const existing=await inspect(fid,activeKey);
                if(existing.registration){state('ready');return existing.registration;}
                const nonce=existing.userNonce+1;
                if(nonce>0xffffffff)throw new Error('KEY_ADD nonce overflow');
                const deadline=farcasterTimestamp()+3600;
                const ttl=30*86400;
                const typed=keyAddTypedData({fid,publicKeyHex:activeKey,
                    scopes:SOCIAL_SCOPES,nonce,deadline,ttl});
                verifySession(fid,expectedGeneration);
                const signedRequest=await wallet.signTypedData(typed.signedKeyRequest);
                fromHex(signedRequest,65);
                verifySession(fid,expectedGeneration);
                const custodySignature=await wallet.signTypedData(typed.keyAdd);
                fromHex(custodySignature,65);
                verifySession(fid,expectedGeneration);
                const metadata=abiMetadata(fid,wallet.address,signedRequest,deadline);
                const unsigned=unsignedKeyAdd({fid,publicKeyHex:activeKey,
                    nonce,deadline,ttl,custodySignature,metadata,
                    scopes:SOCIAL_SCOPES});
                // Never replay a submission after ambiguous network failure.
                await submit(unsigned,fid,expectedGeneration);
                for(let attempt=0;attempt<90;attempt++){
                    const observed=await inspect(fid,activeKey);
                    verifySession(fid,expectedGeneration);
                    if(observed.registration?.source==='gasless'
                       && observed.userNonce>=nonce) {
                        state('ready');return observed.registration;
                    }
                    if(observed.userNonce>=nonce)break;
                    await delay(1000);
                }
                throw new Error('KEY_ADD was submitted but registration was not confirmed by Snapchain.');
            }catch(error){
                state('authorization-required',String(error.message||error));
                throw error;
            }finally {
                // Explicitly drop custody key even after a failed KEY_ADD.
                wallet?.dispose?.();
            }
        });
    }

    async function provisionFromRecovery(fid,phrase) {
        return provision(fid,owner=>recoveryCustody(owner,phrase));
    }

    async function signedWrite(fidValue,scope,encode){
        const fid=requireFid(fidValue);
        return exclusive(async()=>{
            if(activeFid!==fid) throw new Error('signed-in FID changed: signer is locked');
            const expectedGeneration=generation;
            state('checking');
            try {
                const key=await vault.load(fid);
                if(!key||key.publicKeyHex!==activeKey)throw new Error('browser signer key unavailable');
                const observed=await inspect(fid,activeKey);
                verifySession(fid,expectedGeneration);
                if(!observed.registration||!observed.registration.scopes.includes(scope)) {
                    state('expired');
                    throw new Error('Farcaster signer is not network-authorized for this operation');
                }
                const unsigned=encode(activeKey,fid);
                const hash=await submit(unsigned,fid,expectedGeneration);
                verifySession(fid,expectedGeneration);
                state('ready');
                return hash;
            }catch(error){state('failed',String(error.message||error));throw error;}
        });
    }
    const publishCast=(fid,request)=>signedWrite(fid,1,(key,id)=>
        unsignedCastAdd({...request,fid:id,publicKeyHex:key}));

    async function publishThread(fidValue, request) {
        const fid=requireFid(fidValue);
        const casts=request?.casts;
        if(!Array.isArray(casts)||!casts.length||casts.length>32)
            throw new Error('thread must contain 1..32 casts');
        return exclusive(async()=>{
            if(activeFid!==fid)throw new Error('signer FID changed');
            const expectedGeneration=generation;
            state('checking');
            const key=await vault.load(fid);
            if(!key||key.publicKeyHex!==activeKey)throw new Error('signer key unavailable');
            const observed=await inspect(fid,activeKey);
            verifySession(fid,expectedGeneration);
            if(!observed.registration?.scopes.includes(1)) {
                state('expired');
                throw new Error('CastAdd scope is not authorized');
            }
            let parent=request.parent||null;
            let hash='';
            for(const cast of casts) {
                const unsigned=unsignedCastAdd({...cast,fid,publicKeyHex:activeKey,
                    ...(parent?{parent}:{})});
                hash=await submit(unsigned,fid,expectedGeneration);
                parent={fid,hash};
            }
            verifySession(fid,expectedGeneration);
            state('ready');
            return hash;
        });
    }
    const castRemove=(fid,target)=>signedWrite(fid,2,(key,id)=>
        unsignedCastRemove({fid:id,publicKeyHex:key,target}));
    const reaction=(fid,target,kind,active)=>signedWrite(fid,active?3:4,(key,id)=>
        unsignedReaction({fid:id,publicKeyHex:key,target,reaction:kind,active}));
    const follow=(fid,targetFid,active)=>signedWrite(fid,active?5:6,(key,id)=>
        unsignedFollow({fid:id,publicKeyHex:key,targetFid,active}));

    async function revoke(fidValue){
        const fid=requireFid(fidValue);
        return exclusive(async()=>{
            if(activeFid!==fid)throw new Error('signer FID changed');
            const expectedGeneration=generation;state('checking');
            const key=await vault.load(fid);
            if(!key || key.publicKeyHex!==activeKey)throw new Error('signer key unavailable');
            const observed=await inspect(fid,activeKey);
            if(!observed.registration){state('revoked');return;}
            if(observed.requesterNonce===null)
                throw new Error('Snapchain did not expose the requester revocation nonce');
            const nonce=observed.requesterNonce+1;
            const deadline=farcasterTimestamp()+3600;
            const unsigned=unsignedKeyRemove({fid,publicKeyHex:activeKey,nonce,deadline});
            await submit(unsigned,fid,expectedGeneration);
            for(let attempt=0;attempt<90;attempt++){
                const latest=await inspect(fid,activeKey);
                verifySession(fid,expectedGeneration);
                if(!latest.registration && latest.requesterNonce>=nonce){
                    await vault.erase(fid);activeKey=null;state('revoked');return;
                }
                await delay(1000);
            }
            state('unavailable','KEY_REMOVE confirmation timed out; local key retained');
            throw new Error('KEY_REMOVE is not network-confirmed; local key retained');
        });
    }
    function signedOut(){generation++;activeFid=null;activeKey=null;state('signed-out');}
    return Object.freeze({
        restore,provision,provisionFromRecovery,publishCast,publishThread,castRemove,reaction,follow,revoke,signedOut,
        get ready(){return !!activeFid&&!!activeKey&&!busy&&lastPhase==='ready';},
    });
}
