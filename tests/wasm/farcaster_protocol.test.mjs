import test from 'node:test';
import assert from 'node:assert/strict';
import {
    blake3Digest20,hex,fromHex,abiMetadata,keyAddTypedData,
    unsignedKeyAdd,unsignedCastAdd,unsignedKeyRemove,sealMessage,
    unsignedReaction,unsignedFollow,
} from '../../ui/wasm/farcaster_protocol.mjs';
import { keccak256 } from '../../ui/wasm/browser_protocol_signer.mjs';

const publicKeyHex='000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f';
test('official BLAKE3 vectors match C++ protocol/hash.cpp',()=>{
    for(const [size,expected] of [
        [0,'af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9'],
        [1,'2d3adedff11b61f14c886e35afa036736dcd87a7'],
        [1023,'10108970eeda3eb932baac1428c7a2163b0e924c'],
        [1024,'42214739f095a406f3fc83deb889744ac00df831'],
        [1025,'d00278ae47eb27b34faecf67b4fe263f82d54129'],
        [2048,'e776b6028c7cd22a4d0ba182a8bf62205d2ef576'],
        [2049,'5f4d72f40d7a5f82b15ca2b2e44b1de3c2ef86c4'],
    ]){
        const bytes=Uint8Array.from({length:size},(_,i)=>i%251);
        assert.equal(hex(blake3Digest20(bytes)),expected,String(size));
    }
});
test('canonical CastAdd protobuf matches exact native C++ vector',()=>{
    const cast=unsignedCastAdd({
        fid:'42',publicKeyHex,text:'hello',timestamp:123000,
        embeds:[{url:'https://example.com'},
            {cast:{fid:'7',hash:'0x000102030405060708090a0b0c0d0e0f10111213'}}],
    });
    assert.equal(hex(cast.data),
        '0801102a18f8c00720012a3a220568656c6c6f32150a1368747470733a2f2f6578616d706c652e636f6d321a121808071214000102030405060708090a0b0c0d0e0f10111213');
    const reply=unsignedCastAdd({
        fid:'42',publicKeyHex,text:'hello',timestamp:123000,
        parent:{fid:'7',hash:'0x000102030405060708090a0b0c0d0e0f10111213'},
    });
    assert.equal(hex(reply.data),
        '0801102a18f8c00720012a211a1808071214000102030405060708090a0b0c0d0e0f10111213220568656c6c6f');
});
test('channel and token CAST_ADD parentUrl is encoded as field 7 and excludes parent CastId',()=>{
    const args={fid:'42',publicKeyHex,text:'Hello channel',timestamp:123000};
    const url='https://warpcast.com/~/channel/degen';
    const message=unsignedCastAdd({...args,parentUrl:url});
    const bytes=Buffer.from(message.data);
    const bodyStart=bytes.indexOf(Buffer.from(url));
    assert.ok(bodyStart>0,'parent URL serialized in canonical message');
    assert.equal(bytes[bodyStart-2],0x3a,'protobuf body field 7 (wire type 2)');
    const token='chain://eip155:8453/erc20:0x0000000000000000000000000000000000000001';
    assert.doesNotThrow(()=>unsignedCastAdd({...args,parentUrl:token}));
    assert.throws(()=>unsignedCastAdd({...args,parentUrl:'https://a.example',parent:{fid:'7',hash:'0x000102030405060708090a0b0c0d0e0f10111213'}}),/both a parent cast and a parent URL/);
    assert.throws(()=>unsignedCastAdd({...args,parentUrl:'http://unsafe.example'}),/scheme/);
    assert.throws(()=>unsignedCastAdd({...args,parentUrl:'https://a.example/'+'x'.repeat(257)}),/256-byte/);
});
test('channel and ticker CAST_ADD parent_url is encoded as field 7 and excludes reply parent',()=>{
 const common={fid:'42',publicKeyHex,text:'hello',timestamp:123000};
 const url='https://farcaster.group/dev';
 const result=unsignedCastAdd({...common,parentUrl:url});
 const bytes=hex(result.data);
 assert.ok(bytes.includes('3a1b'+hex(new TextEncoder().encode(url))));
 const ticker='chain://eip155:8453/erc20:0x1234567890123456789012345678901234567890';
 assert.doesNotThrow(()=>unsignedCastAdd({...common,parentUrl:ticker}));
 assert.throws(()=>unsignedCastAdd({...common,parentUrl:'https://'+'x'.repeat(255)}),/256-byte/);
 assert.throws(()=>unsignedCastAdd({...common,parentUrl:'javascript:alert(1)'}),/parent URL scheme/);
 assert.throws(()=>unsignedCastAdd({...common,parentUrl:url,parent:{fid:'7',hash:'0x'+'0'.repeat(40)}}),/cannot have both/);
});
test('canonical Farcaster / HyperSnap embeds use URL or CastId, with a strict 256 UTF-8 byte bound',()=>{
    const source={fid:'42',publicKeyHex,text:'image',timestamp:123000};
    const good='https://media-farcaster.genyleap.com/m/v1/2026/10/photo.jpg';
    assert.doesNotThrow(()=>unsignedCastAdd({...source,embeds:[{url:good}]}));
    assert.doesNotThrow(()=>unsignedCastAdd({...source,embeds:[{url:'x'.repeat(256)}]}));
    assert.throws(()=>unsignedCastAdd({...source,embeds:[{url:'x'.repeat(257)}]}),
        /256-byte/);
    // UTF-8 bytes (not JS code units) define the protocol limit.
    assert.throws(()=>unsignedCastAdd({...source,embeds:[{url:'é'.repeat(129)}]}),
        /256-byte/);
    assert.throws(()=>unsignedCastAdd({...source,embeds:[{url:''}]}),
        /256-byte/);
    assert.throws(()=>unsignedCastAdd({...source,embeds:[{url:good},{url:good},{url:good}]}),
        /cast field limit/);
    assert.throws(()=>unsignedCastAdd({...source,embeds:[{cast:null}]}),
        /invalid cast reference/);
});
test('canonical KEY_ADD protobuf matches native C++ vector',()=>{
    const key=unsignedKeyAdd({
        fid:'42',publicKeyHex,scopes:[1,2,3,4,5,6],
        ttl:2592000,nonce:7,deadline:123456,timestamp:123000,
        custodySignature:'0x'+hex(Uint8Array.from({length:65},(_,i)=>i)),
        metadata:Uint8Array.from({length:96},(_,i)=>i),
    });
    assert.equal(hex(key.data),
        '0810102a18f8c00720019a01de010a20000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f10011a41000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f4020c0c40728073260000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f38014a0601020304050650809a9e01');
    assert.equal(key.hash.length,20);
});
test('SignedKeyRequest ABI metadata is bit-for-bit native',()=>{
    const metadata=abiMetadata('43','0x'+hex(Uint8Array.from({length:20},(_,i)=>i)),
        '0x'+hex(Uint8Array.from({length:65},(_,i)=>i)),123456);
    assert.equal(metadata.length,288);
    assert.equal(hex(metadata).slice(0,128),
        '0000000000000000000000000000000000000000000000000000000000000020'
       +'000000000000000000000000000000000000000000000000000000000000002b');
});
test('KEY_ADD typed documents bind exact FID, scopes, nonce, TTL and deadline',()=>{
    const typed=keyAddTypedData({fid:'42',publicKeyHex,nonce:7,
        deadline:123456,ttl:2592000});
    assert.equal(typed.keyAdd.primaryType,'KeyAdd');
    // Snapchain gasless signatures MUST NOT use the Optimism onchain
    // SignedKeyRequestValidator domain. Farcaster's official gaslessKeys
    // helpers intentionally share this offchain EIP-712 domain (chain 1).
    assert.equal(typed.keyAdd.domain.chainId,1);
    assert.equal(typed.keyAdd.domain.name,'Farcaster KeyAdd');
    assert.deepEqual(typed.signedKeyRequest.domain,typed.keyAdd.domain);
    assert.equal(typed.signedKeyRequest.domain.verifyingContract,undefined);
    assert.equal(typed.signedKeyRequest.message.deadline,'123456');
    assert.equal(typed.keyAdd.message.fid,'42');
    assert.deepEqual(typed.keyAdd.message.scopes,[1,2,3,4,5,6]);
    assert.equal(typed.signedKeyRequest.message.requestFid,'42');
    assert.throws(()=>keyAddTypedData({fid:'42',publicKeyHex,nonce:0,deadline:123456,ttl:2592000}),/nonce/);
    assert.throws(()=>keyAddTypedData({fid:'42',publicKeyHex,nonce:1,deadline:123456,ttl:2592000,scopes:[1,1]}),/scopes/);
});
test('KEY_REMOVE self-revocation codec exactly matches native protobuf fixture',()=>{
    const message=unsignedKeyRemove({fid:'42',publicKeyHex,nonce:9,
        deadline:123456,timestamp:123001});
    assert.equal(hex(message.data),
        '0811102a18f9c0072001a2012a0a20000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f180220c0c4072809');
});
test('Keccak256 matches Ethereum custody selector and official empty digest',()=>{
    assert.equal(hex(keccak256(new Uint8Array())),
        'c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470');
    assert.equal(hex(keccak256(new TextEncoder().encode('custodyOf(uint256)'))).slice(0,8),
        '65269e47');
});
test('canonical envelope refuses invalid signer and signature bytes',()=>{
    const unsigned=unsignedCastAdd({fid:'42',publicKeyHex,text:'test',timestamp:123000});
    assert.throws(()=>sealMessage(unsigned,new Uint8Array(63)),/invalid Ed25519 signer/);
    const sealed=sealMessage(unsigned,new Uint8Array(64));
    assert.ok(sealed.length>unsigned.data.length+64);
    assert.throws(()=>fromHex('0xeeee',20),/length/);
});
test('canonical reaction/follow have expected message types',()=>{
    const args={fid:'42',publicKeyHex,timestamp:123000};
    const reaction=unsignedReaction({...args,
        target:{fid:'7',hash:'0x000102030405060708090a0b0c0d0e0f10111213'}});
    assert.equal(reaction.data[1],3);
    assert.ok(hex(reaction.data).includes('3a'), 'reaction protobuf uses field 7');
    const follow=unsignedFollow({...args,targetFid:'7'});
    assert.equal(follow.data[1],5);
    assert.ok(hex(follow.data).includes('72'), 'link protobuf uses field 14');
    assert.throws(()=>unsignedFollow({...args,targetFid:'42'}),/self/);
});
