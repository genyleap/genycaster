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
                  wrongType=false, profileScope=true}={}){
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
                    'MESSAGE_TYPE_LINK_ADD','MESSAGE_TYPE_LINK_REMOVE',
                    ...(profileScope?['MESSAGE_TYPE_USER_DATA_ADD']:[])],
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
                11:'MESSAGE_TYPE_USER_DATA_ADD',
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
    assert.deepEqual(data.typed[1].message.scopes,[1,2,3,4,5,6,11]);
    assert.equal(data.typed[0].message.requestFid,fid);
    assert.equal(data.states.at(-1).phase,'ready');
    assert.equal(signer.ready,true);
    const hash=await signer.publishCast(fid,{text:'Hello from Genycaster'});
    assert.match(hash,/^0x[0-9a-f]{40}$/);
    assert.equal(data.submitted.length,2);
    assert.equal(signer.ready,true);
});


test('profile UserDataAdd signs each changed slot only with scope 11 and confirms canonical response', async()=>{
    const data=fixture(), signer=data.create();
    await signer.provision(fid);
    const before=data.submitted.length;
    const hash=await signer.updateProfile(fid,{display:'GenyCaster',bio:'test profile'});
    assert.match(hash,/^0x[0-9a-f]{40}$/);
    assert.equal(data.submitted.length,before+2);
    assert.equal(signer.ready,true);
    await assert.rejects(()=>signer.updateProfile(fid,{unknown:'x'}),/no profile changes/);
});

test('legacy signer without UserDataAdd authorization cannot change profile',async()=>{
    const data=fixture({profileScope:false}), signer=data.create();
    await signer.provision(fid);
    const before=data.submitted.length;
    await assert.rejects(()=>signer.updateProfile(fid,{display:'Not authorized'}),/not network-authorized/);
    assert.equal(data.submitted.length,before);
});

test('gasless signer confirmation survives registry lag after nonce advancement',async()=>{
    const data=fixture();
    let staleReads=0;
    const signer=createBrowserProtocolSigner({
        browser:data.browser,vault:data.vault,
        fetchImpl:async(url,options)=>{
            if(url.includes('/v1/signersByFid') && data.submitted.length && staleReads++<3) {
                return new Response(JSON.stringify({signers:[],currentUserNonce:1,
                    requesterFidNonces:{[fid]:0}}));
            }
            return data.fetchImpl(url,options);
        },
        onState:s=>data.states.push(s),delay:async()=>{},
    });
    const authorized=await signer.provision(fid);
    assert.equal(authorized.source,'gasless');
    assert.equal(data.submitted.length,1,'never resubmit an ambiguous KEY_ADD');
    assert.equal(staleReads,4,'registry visibility lags nonce visibility');
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

const recoveredPhrase='abandon '.repeat(23)+'art';
const recoveredAddress='0xf278cf59f82edcf871d630f28ecc8056f25c1cdb';

test('recovery phrase authorizes KEY_ADD without any browser wallet extension',async()=>{
    const data=fixture({noWallet:true});
    const chainCalls=[];
    const signer=createBrowserProtocolSigner({
        browser:data.browser,vault:data.vault,
        fetchImpl:async(url,options)=>{
            if(url==='https://mainnet.optimism.io'){
                assert.equal(options.method,'POST');
                assert.equal(options.credentials,'omit');
                const body=JSON.parse(options.body);
                chainCalls.push(body.method);
                assert.ok(!options.body.includes(recoveredPhrase),
                    'the recovery phrase must never enter an HTTP request');
                if(body.method==='eth_chainId')
                    return new Response(JSON.stringify({jsonrpc:'2.0',id:body.id,result:'0xa'}));
                if(body.method==='eth_call'){
                    assert.match(body.params[0].data,/^0x65269e47[0-9a-f]{64}$/);
                    return new Response(JSON.stringify({jsonrpc:'2.0',id:body.id,
                        result:'0x'+'0'.repeat(24)+recoveredAddress.slice(2)}));
                }
                throw new Error('unexpected RPC call');
            }
            return data.fetchImpl(url,options);
        },
        onState:s=>data.states.push(s),delay:async()=>{},
    });
    const authorized=await signer.provisionFromRecovery(fid,recoveredPhrase);
    assert.equal(authorized.source,'gasless');
    assert.deepEqual(chainCalls,['eth_chainId','eth_call']);
    assert.equal(data.typed.length,0,'wallet extension must not be invoked');
    assert.equal(data.submitted.length,1,'signed KEY_ADD submitted once');
    assert.equal(signer.ready,true);
    await signer.publishCast(fid,{text:'Hello, imported from Farcaster phrase'});
    assert.equal(data.submitted.length,2);
});

test('wrong recovery phrase FID ownership rejects KEY_ADD and does not submit',async()=>{
    const data=fixture({noWallet:true});
    const signer=createBrowserProtocolSigner({
        browser:data.browser,vault:data.vault,
        fetchImpl:async(url,options)=>{
            if(url==='https://mainnet.optimism.io'){
                const request=JSON.parse(options.body);
                if(request.method==='eth_chainId')
                    return new Response(JSON.stringify({result:'0xa'}));
                return new Response(JSON.stringify({result:'0x'+'0'.repeat(24)+
                    '5'.repeat(40)}));
            }
            return data.fetchImpl(url,options);
        },
        onState:s=>data.states.push(s),
    });
    await assert.rejects(signer.provisionFromRecovery(fid,recoveredPhrase),
        /does not own/);
    assert.equal(data.submitted.length,0);
});

test('invalid recovery BIP39 checksum fails closed without touching Snapchain',async()=>{
    const data=fixture({noWallet:true});
    const signer=data.create();
    await assert.rejects(signer.provisionFromRecovery(fid,
        'abandon '.repeat(24).trim()),/checksum/);
    assert.equal(data.submitted.length,0);
});


test('direct setup recovery derives an onchain FID and verifies KEY_ADD without SIWF',async()=>{
    const data=fixture({noWallet:true});
    const requests=[];
    const signer=createBrowserProtocolSigner({
        browser:data.browser,vault:data.vault,
        fetchImpl:async(url,options)=>{
            if(url==='https://mainnet.optimism.io'){
                assert.equal(options.credentials,'omit');
                assert.equal(options.method,'POST');
                assert.ok(!options.body.includes(recoveredPhrase));
                const body=JSON.parse(options.body);
                requests.push(body);
                let result;
                if(body.method==='eth_chainId')result='0xa';
                else if(body.method==='eth_call'){
                    if(body.params[0].data.startsWith('0x'+
                        hex(keccak256(new TextEncoder().encode('idOf(address)')).slice(0,4)))){
                        assert.ok(body.params[0].data.endsWith(recoveredAddress.slice(2)));
                        result='0x'+BigInt(fid).toString(16).padStart(64,'0');
                    }else{
                        assert.match(body.params[0].data,/^0x65269e47[0-9a-f]{64}$/);
                        result='0x'+'0'.repeat(24)+recoveredAddress.slice(2);
                    }
                } else throw Error('unexpected RPC');
                return new Response(JSON.stringify({result}));
            }
            return data.fetchImpl(url,options);
        },onState:s=>data.states.push(s),delay:async()=>{},
    });
    assert.equal(await signer.recoverAccountFromPhrase(recoveredPhrase),fid);
    assert.equal(signer.ready,true);
    assert.equal(data.submitted.length,1);
    assert.deepEqual(requests.map(x=>x.method),['eth_chainId','eth_call','eth_call']);
    assert.equal(data.typed.length,0);
});

test('direct recovery wraps readonly DOMException-style codes without losing verification phase',async()=>{
    const data=fixture({noWallet:true});
    const signer=createBrowserProtocolSigner({
        browser:data.browser,vault:data.vault,
        fetchImpl:async(url,options)=>{
            if(url==='https://mainnet.optimism.io'){
                const failure=new Error('Unexpected provider response');
                Object.defineProperty(failure,'code',{value:18,writable:false});
                throw failure;
            }
            return data.fetchImpl(url,options);
        },
    });
    await assert.rejects(signer.recoverAccountFromPhrase(recoveredPhrase),error=>
        error.code==='recovery-verification'
        && error.message==='Unexpected provider response');
    assert.equal(data.submitted.length,0,'never register a signer after failed custody verification');
});

test('direct setup refuses unregistered or transferred custody before KEY_ADD',async()=>{
    for(const mismatch of ['unregistered','transferred','wrong-chain']){
        const data=fixture({noWallet:true});
        const signer=createBrowserProtocolSigner({
            browser:data.browser,vault:data.vault,
            fetchImpl:async(url,options)=>{
                if(url==='https://mainnet.optimism.io'){
                    const body=JSON.parse(options.body);
                    let result;
                    if(body.method==='eth_chainId')
                        result=mismatch==='wrong-chain'?'0x1':'0xa';
                    else if(body.method==='eth_call' &&
                             body.params[0].data.startsWith('0x'+
                             hex(keccak256(new TextEncoder().encode('idOf(address)')).slice(0,4))))
                        result='0x'+(mismatch==='unregistered'?'0':BigInt(fid).toString(16)).padStart(64,'0');
                    else result='0x'+'0'.repeat(24)+'5'.repeat(40);
                    return new Response(JSON.stringify({result}));
                }
                return data.fetchImpl(url,options);
            },
        });
        await assert.rejects(signer.recoverAccountFromPhrase(recoveredPhrase),
            error=>mismatch==='unregistered'
                ? error.code==='recovery-unregistered'
                : mismatch==='transferred'
                  ? error.code==='recovery-ownership'
                  : error.code==='recovery-network');
        assert.equal(data.submitted.length,0);
    }
});

test('invalid mnemonic cannot resolve identity or authorize a signer',async()=>{
    const data=fixture({noWallet:true});
    await assert.rejects(data.create().recoverAccountFromPhrase(
        'abandon '.repeat(24).trim()),/checksum/);
    assert.equal(data.submitted.length,0);
});


test('sign-out while resolving IdRegistry aborts before any signer submission',async()=>{
    const data=fixture({noWallet:true});
    let allowResponse;
    let enteredCall;
    const entered=new Promise(resolve=>{enteredCall=resolve;});
    const blocked=new Promise(resolve=>{allowResponse=resolve;});
    let current=true;
    const signer=createBrowserProtocolSigner({
        browser:data.browser,vault:data.vault,
        fetchImpl:async(url,options)=>{
            if(url==='https://mainnet.optimism.io'){
                const body=JSON.parse(options.body);
                if(body.method==='eth_chainId')
                    return new Response(JSON.stringify({result:'0xa'}));
                enteredCall();
                await blocked;
                return new Response(JSON.stringify({result:
                    '0x'+BigInt(fid).toString(16).padStart(64,'0')}));
            }
            return data.fetchImpl(url,options);
        }
    });
    const pending=signer.recoverAccountFromPhrase(recoveredPhrase,()=>current);
    await entered;
    current=false;
    signer.signedOut();
    allowResponse();
    await assert.rejects(pending,/Recovery cancelled/);
    assert.equal(data.submitted.length,0);
});
