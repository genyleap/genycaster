import test from 'node:test';
import assert from 'node:assert/strict';
import { deriveFarcasterCustody, validateRecoveryWords } from '../../ui/wasm/farcaster_recovery.mjs';
import { keyAddTypedData, fromHex } from '../../ui/wasm/farcaster_protocol.mjs';
import { Signature, getPublicKey } from '../../ui/wasm/vendor/noble-secp256k1.mjs';
import { keccak256 } from '../../ui/wasm/browser_protocol_signer.mjs';
const phrase='abandon '.repeat(23)+'art';
test('BIP39 checksum rejects invalid or malformed recovery phrases',async()=>{
    assert.equal(await validateRecoveryWords(phrase),phrase);
    await assert.rejects(validateRecoveryWords('abandon '.repeat(23)+'abandon'),/checksum/);
    await assert.rejects(validateRecoveryWords('valid valid valid'),/24-word/);
    await assert.rejects(validateRecoveryWords('abandon '.repeat(23)+'notaword'),/unrecognized/);
    await assert.rejects(validateRecoveryWords('test '.repeat(11)+'junk'),/24-word/);
});
test('BIP44 Ethereum custody derivation supports the 24-word BIP39 vector',async()=>{
    const wallet=await deriveFarcasterCustody(phrase);
    try {
        assert.match(wallet.address,/^0x[0-9a-f]{40}$/);
    }finally{wallet.dispose();}
});
test('EIP-712 custody signer uses canonical 65-byte low-S signatures for both KeyAdd typed documents',async()=>{
    const wallet=await deriveFarcasterCustody(phrase);
    try {
        const documents=keyAddTypedData({fid:'42',publicKeyHex:'fe'.repeat(32),
            nonce:123,deadline:456789,ttl:2592000});
        for(const document of [documents.signedKeyRequest, documents.keyAdd]){
            const signature=await wallet.signTypedData(document);
            assert.match(signature,/^0x[\da-f]{130}$/);
            const bytes=fromHex(signature,65);
            assert.ok(bytes[64]===27||bytes[64]===28);
            const sig=Signature.fromBytes(bytes.subarray(0,64)).addRecoveryBit(bytes[64]-27);
            assert.ok(!sig.hasHighS());
        }
    }finally{wallet.dispose();}
});

test('24-word BIP39 recovery supports official zero-entropy vector',async()=>{
    const mnemonic='abandon '.repeat(23)+'art';
    assert.equal(await validateRecoveryWords(mnemonic),mnemonic);
    const wallet=await deriveFarcasterCustody(mnemonic);
    assert.match(wallet.address,/^0x[0-9a-f]{40}$/);
    wallet.dispose();
});
test('disposed ephemeral custody key can no longer authorize new messages',async()=>{
    const wallet=await deriveFarcasterCustody(phrase);
    const document=keyAddTypedData({fid:'42',publicKeyHex:'fe'.repeat(32),
        nonce:123,deadline:456789,ttl:2592000}).keyAdd;
    wallet.dispose();
    await assert.rejects(wallet.signTypedData(document));
});
