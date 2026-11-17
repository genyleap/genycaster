import test from 'node:test';
import assert from 'node:assert/strict';
import {signAsync,getPublicKey} from '../../../ui/wasm/vendor/noble-secp256k1.mjs';
import {keccak256} from '../../../ui/wasm/browser_protocol_signer.mjs';
import {hex} from '../../../ui/wasm/farcaster_protocol.mjs';
import {proWalletMessage,verifyProWalletSignature,handleProWalletProof}
  from '../pro_wallet_proof.mjs';

const account={issuer:'https://identity.example.test',subject:'farcaster:42',fid:'42'};
const key=new Uint8Array(32).fill(0x11);
const pub=getPublicKey(key,false);
const address='0x'+hex(keccak256(pub.slice(1)).slice(-20));
const utf8=new TextEncoder();

async function signatureFor(message){
  const bytes=utf8.encode(message);
  const prefix=utf8.encode('\x19Ethereum Signed Message:\n'+bytes.length);
  const payload=new Uint8Array(prefix.length+bytes.length);
  payload.set(prefix);payload.set(bytes,prefix.length);
  const sig=await signAsync(keccak256(payload),key);
  return '0x'+hex(sig.toBytes())+(27+sig.recovery).toString(16);
}

test('Pro payer proof requires correct signer, account-bound nonce and EIP-191 domain',async()=>{
  const expires=new Date(Date.now()+60_000).toISOString();
  const message=proWalletMessage(account,address,'a1'.repeat(16),expires);
  assert.match(message,/Origin: https:\/\/genycaster\.xyz/);
  assert.match(message,/Account: farcaster:42/);
  assert.match(message,/does not approve a transaction/i);
  const signature=await signatureFor(message);
  assert.equal(verifyProWalletSignature(message,signature,address),true);
  assert.equal(verifyProWalletSignature(message+'\n',signature,address),false);
  assert.equal(verifyProWalletSignature(message,signature,'0x'+'22'.repeat(20)),false);
  assert.equal(verifyProWalletSignature(message,signature.slice(0,-2)+'ff',address),false);
  const another=proWalletMessage(
    {issuer:account.issuer,subject:'farcaster:43'},address,'a1'.repeat(16),expires);
  assert.equal(verifyProWalletSignature(another,signature,address),false);
});

test('Pro payer API needs account-owned, single-use signed proof before any wallet is saved',async()=>{
  const id='0d6a73b9-1f42-4b76-bb84-880b0a77a621';
  const expires=new Date(Date.now()+90_000).toISOString();
  const message=proWalletMessage(account,address,'b2'.repeat(16),expires);
  let issued=false,consumed=false,saved=[];
  const store={
    proWalletReady:()=>true,
    getProWallets:async()=>[...saved],
    createProWalletChallenge:async()=>{issued=true;return {id,message,address,expires_at:expires}},
    consumeProWalletChallenge:async(owner,challengeId)=>{
      assert.equal(owner.subject,account.subject);
      if(!issued || consumed || challengeId!==id)return null;
      consumed=true;return {address,message};
    },
    saveProWallet:async(owner,payer)=>{assert.equal(owner.subject,account.subject);saved.push(payer)}
  };
  const respond=(res,status,value)=>{res.status=status;res.body=value};
  const request={method:'POST',headers:{'content-type':'application/json'}};
  const invoke=async(entity,input,method='POST')=>{
    const response={};
    await handleProWalletProof({
      entity,request:{...request,method},response,owner:account,store,
      readBody:async()=>input,respond
    });
    return response;
  };
  assert.equal((await invoke('pro/wallet/challenge',{address})).status,201);
  assert.equal((await invoke('pro/wallet/verify',{challenge_id:id,signature:'0x'+'00'.repeat(65)})).status,403);
  assert.deepEqual(saved,[]);
  assert.equal((await invoke('pro/wallet/verify',{challenge_id:id,signature:await signatureFor(message)})).status,403);
  issued=false;consumed=false;
  assert.equal((await invoke('pro/wallet/challenge',{address})).status,201);
  assert.equal((await invoke('pro/wallet/verify',{challenge_id:id,signature:await signatureFor(message)})).status,200);
  assert.deepEqual(saved,[address]);
  assert.equal((await invoke('pro/wallets',{},'GET')).status,200);
  assert.equal((await invoke('pro/wallet/challenge',{address,issuer:account.issuer})).status,400);
  assert.equal((await invoke('pro/wallet/verify',{challenge_id:id,signature:await signatureFor(message)})).status,403);
});
