import test from 'node:test';
import assert from 'node:assert/strict';
import { createBrowserProtocolSigner, keccak256 } from '../../ui/wasm/browser_protocol_signer.mjs';
import { hex } from '../../ui/wasm/farcaster_protocol.mjs';

const fid='42';
const address='0x1234567890123456789012345678901234567890';
const key='a1'.repeat(32);
const signature='0x'+ 'a2'.repeat(65);
const now=Math.floor(Date.now()/1000)-1609459200;

function fixture({custodyAddress=address, noWallet=false, activeInitially=false,
                  wrongResponse=false, rejectSubmit=false, wrongFid=false,
                  wrongType=false}={}){
    const typed=[],submitted=[],changes=[],states=[],calls=[];
    let active=activeInitially, nonce=0;
    const browser={isSecureContext:true};
    if(!noWallet)browser.ethereum={
        async request({method,params}){
            calls.push(method);
            if(method==='eth_requestAccounts')return [address];
            if(method==='eth_chainId')return '0xa';
            if(method==='wallet_switchEthereumChain')return null;
            if(method==='eth_call'){
                assert.match(params[0].data,/^0x65269e47[0-9a-f]{64}$/);
                return '0x'+'0'.repeat(24)+custodyAddress.slice(2);
            }
            if(method==='eth_signTypedData_v4'){
                const item=JSON.parse(params[1]);typed.push(item);
                return signature;
            }
            throw new Error('unexpected ethereum RPC '+method);
        },
    };
    const vault={
        async load(){return {fid,publicKeyHex:key};},
        async ensure(){return {fid,publicKeyHex:key};},
        async signHash(_fid,hash){
            assert.equal(hash.length,20);
            return new Uint8Array(64).fill(0x17);
        },
        async erase(){changes.push('erase');},
    };
    const fetchImpl=async(url,options)=>{
        if(url.includes('/v1/signersByFid')){
            const query=new URL(url);
            assert.equal(query.searchParams.get('fid'),fid);
            assert.equal(query.searchParams.get('requesterFids[0]'),fid);
            const record={
                source:'SIGNER_SOURCE_OFFCHAIN',key:'0x'+key,keyType:1,
                fid:Number(fid),ttl:2592000,lastUsedAt:now,
                expiresAt:now+2592000,requestFid:fid,
                scopes:['MESSAGE_TYPE_CAST_ADD','MESSAGE_TYPE_CAST_REMOVE',
                    'MESSAGE_TYPE_REACTION_ADD','MESSAGE_TYPE_REACTION_REMOVE',
                    'MESSAGE_TYPE_LINK_ADD','MESSAGE_TYPE_LINK_REMOVE'],
            };
            return new Response(JSON.stringify({
                signers: active?[record]:[],currentUserNonce:nonce,
                requesterFidNonces:{[fid]:0},
            }));
        }
        if(url.endsWith('/v1/submitMessage')){
            assert.equal(options.method,'POST');
            assert.equal(options.credentials,'omit');
            assert.equal(options.headers['content-type'],'application/octet-stream');
            const bytes=options.body;
            const serialized=Uint8Array.from(bytes);
            assert.equal(serialized[0],0x12,'protobuf envelope hash field 2');
            assert.equal(serialized[1],20,'BLAKE3 hash size');
            const hash='0x'+hex(serialized.subarray(2,22));
            submitted.push({bytes,hash});
            if(rejectSubmit)throw new Error('network timeout');
            // First write is KEY_ADD (kind inside MessageData); thereafter CAST.
            if(submitted.length===1 && !active){active=true;nonce=1;}
            // Envelope field 7 contains MessageData; its length is a varint
            // and can exceed one byte for KEY_ADD.
            const messageDataIndex=serialized.findIndex((v,i)=>i>25&&v===0x3a);
            assert.ok(messageDataIndex>25);
            let cursor=messageDataIndex+1;
            while(serialized[cursor++]&0x80) {}
            const kind=serialized[cursor+1];
            const types={
                1:'MESSAGE_TYPE_CAST_ADD',2:'MESSAGE_TYPE_CAST_REMOVE',
                3:'MESSAGE_TYPE_REACTION_ADD',4:'MESSAGE_TYPE_REACTION_REMOVE',
                5:'MESSAGE_TYPE_LINK_ADD',6:'MESSAGE_TYPE_LINK_REMOVE',
                16:'MESSAGE_TYPE_KEY_ADD',17:'MESSAGE_TYPE_KEY_REMOVE',
            };
            assert.ok(types[kind], 'canonical message type expected');
            return new Response(JSON.stringify({
                hash:wrongResponse?'0x'+'0'.repeat(40):hash,
                data:{fid:wrongFid?Number(fid)+1:Number(fid),
                      type:wrongType?'MESSAGE_TYPE_USER_DATA_ADD':types[kind]},
            }));
        }
        throw new Error('unknown network path: '+url);
    };
    return {browser,vault,fetchImpl,typed,submitted,changes,states,calls,
        create:()=>createBrowserProtocolSigner({
            browser,vault,fetchImpl,onState:s=>states.push(s),delay:async()=>{},
        })};
}

test('two custody EIP-712 signatures and canonical KEY_ADD before network-ready',async()=>{
    const data=fixture(), signer=data.create();
    const ready=await signer.provision(fid);
    assert.equal(ready.source,'gasless');
    assert.equal(data.submitted.length,1);
    assert.equal(data.typed.length,2);
    assert.equal(data.typed[0].primaryType,'SignedKeyRequest');
    assert.equal(data.typed[1].primaryType,'KeyAdd');
    assert.equal(data.typed[1].message.fid,fid);
    assert.equal(data.typed[1].message.nonce,1);
    assert.deepEqual(data.typed[1].message.scopes,[1,2,3,4,5,6]);
    assert.equal(data.typed[0].message.requestFid,fid);
    assert.equal(data.states.at(-1).phase,'ready');
    assert.equal(signer.ready,true);
    const hash=await signer.publishCast(fid,{text:'Hello from Genycaster'});
    assert.match(hash,/^0x[0-9a-f]{40}$/);
    assert.equal(data.submitted.length,2);
    assert.equal(signer.ready,true);
});

test('wrong custody address aborts before signer generation and network writes',async()=>{
    const data=fixture({custodyAddress:'0x'+'3'.repeat(40)});
    await assert.rejects(data.create().provision(fid),/does not own/);
    assert.equal(data.submitted.length,0);
    assert.equal(data.typed.length,0);
});

test('wallet absence returns explicit custody approval error',async()=>{
    const data=fixture({noWallet:true});
    await assert.rejects(data.create().provision(fid),/wallet extension is unavailable/);
    assert.equal(data.submitted.length,0);
});

test('an existing verified signer is restored without new KEY_ADD',async()=>{
    const data=fixture({activeInitially:true});
    const signer=data.create();
    assert.equal((await signer.restore(fid)).source,'gasless');
    assert.equal(signer.ready,true);
    assert.equal(data.submitted.length,0);
});

test('network submit hash mismatch never reports a confirmed write',async()=>{
    const data=fixture({activeInitially:true,wrongResponse:true});
    const signer=data.create();
    await signer.restore(fid);
    await assert.rejects(signer.publishCast(fid,{text:'test'}),/hash mismatch/);
    assert.equal(data.submitted.length,1);
    assert.equal(data.states.at(-1).phase,'failed');
    assert.equal(signer.ready,false);
});

test('network timeout cannot retry ambiguously submitted cast',async()=>{
    const data=fixture({activeInitially:true,rejectSubmit:true});
    const signer=data.create();
    await signer.restore(fid);
    await assert.rejects(signer.publishCast(fid,{text:'test'}),/timeout/);
    assert.equal(data.submitted.length,1);
});

test('signed out account cannot publish with previously ready FID',async()=>{
    const data=fixture({activeInitially:true}), signer=data.create();
    await signer.restore(fid);
    signer.signedOut();
    await assert.rejects(signer.publishCast(fid,{text:'test'}),/FID changed/);
    assert.equal(data.submitted.length,0);
});

test('typed signing parameters never include key seeds or mnemonic',async()=>{
    const data=fixture(), signer=data.create();
    await signer.provision(fid);
    const text=JSON.stringify(data.typed);
    assert.ok(!text.includes('privateKey'));
    assert.ok(!text.includes('mnemonic'));
    assert.equal(hex(keccak256(new Uint8Array())),
        'c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470');
});

test('network rejects a response with a different FID despite identical hash',async()=>{
    const data=fixture({activeInitially:true,wrongFid:true}),signer=data.create();
    await signer.restore(fid);
    await assert.rejects(signer.publishCast(fid,{text:'test'}),/type or FID mismatch/);
    assert.equal(data.submitted.length,1);
    assert.equal(data.states.at(-1).phase,'failed');
});

test('network rejects a response with wrong protocol message type',async()=>{
    const data=fixture({activeInitially:true,wrongType:true}),signer=data.create();
    await signer.restore(fid);
    await assert.rejects(signer.publishCast(fid,{text:'test'}),/type or FID mismatch/);
    assert.equal(data.submitted.length,1);
});

test('sign-out racing with write rejects stale completion',async()=>{
    const data=fixture({activeInitially:true}),signer=data.create();
    await signer.restore(fid);
    // Take signed write authority away before the pending read completes.
    let release;
    let injected=false;
    const wrapped=createBrowserProtocolSigner({
        browser:data.browser,vault:data.vault,
        fetchImpl:async(...args)=>{
            if(args[0].includes('/v1/signersByFid') && injected){
                await new Promise(resolve=>{release=resolve;});
            }
            return data.fetchImpl(...args);
        },
        onState:s=>data.states.push(s),
    });
    await wrapped.restore(fid);
    injected=true;
    const write=wrapped.publishCast(fid,{text:'stale'});
    await new Promise(resolve=>setImmediate(resolve));
    wrapped.signedOut();
    if(release)release();
    await assert.rejects(write,/identity change/);
    assert.equal(data.submitted.length,0);
});
