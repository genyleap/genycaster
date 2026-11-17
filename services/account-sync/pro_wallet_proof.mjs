// App-owned Pro payer proof: an account session AND an EIP-191 wallet
// signature are required. eth_requestAccounts alone establishes neither.
// No protocol verification or wallet transfer is forged by this extension.
import {Signature} from '../../ui/wasm/vendor/noble-secp256k1.mjs';
import {keccak256} from '../../ui/wasm/browser_protocol_signer.mjs';
import {fromHex,hex} from '../../ui/wasm/farcaster_protocol.mjs';

export const PRO_WALLET_ADDRESS=/^0x[0-9a-fA-F]{40}$/;
export const PRO_WALLET_SIGNATURE=/^0x[0-9a-fA-F]{130}$/;
const CHALLENGE_ID=/^[0-9a-f]{8}-[0-9a-f]{4}-[1-8][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;
const utf8=new TextEncoder();

export function proWalletMessage(owner,address,nonce,expiresAt) {
  if(!owner || typeof owner.issuer!=='string'
      || typeof owner.subject!=='string'
      || !PRO_WALLET_ADDRESS.test(address)
      || typeof nonce!=='string' || !/^[0-9a-f]{32}$/.test(nonce)
      || !Number.isFinite(Date.parse(expiresAt)))throw new Error('invalid_wallet_challenge');
  return [
    'GenyCaster Pro — verify payment wallet',
    'This signature authorizes ONLY linking this wallet to your GenyCaster account.',
    'It does not approve a transaction or grant Pro membership.',
    'Origin: https://genycaster.xyz',
    'Account issuer: '+owner.issuer,
    'Account: '+owner.subject,
    'Wallet: '+address.toLowerCase(),
    'Nonce: '+nonce,
    'Expires: '+expiresAt
  ].join('\n');
}

export function verifyProWalletSignature(message,signature,address) {
  if(typeof message!=='string' || message.length>2500 || message.length<100
     || typeof signature!=='string' || !PRO_WALLET_SIGNATURE.test(signature)
     || typeof address!=='string' || !PRO_WALLET_ADDRESS.test(address))
    return false;
  try{
    const bytes=fromHex(signature.slice(2),65);
    const recovery=bytes[64]===27 || bytes[64]===28 ? bytes[64]-27
      :bytes[64]===0 || bytes[64]===1 ?bytes[64] : -1;
    if(recovery<0)return false;
    const encoded=utf8.encode(message);
    const prefix=utf8.encode('\x19Ethereum Signed Message:\n'+encoded.length);
    const payload=new Uint8Array(prefix.length+encoded.length);
    payload.set(prefix);
    payload.set(encoded,prefix.length);
    const digest=keccak256(payload);
    const sig=Signature.fromBytes(bytes.slice(0,64)).addRecoveryBit(recovery);
    if(sig.hasHighS())return false;
    const pub=sig.recoverPublicKey(digest).toBytes(false);
    return '0x'+hex(keccak256(pub.slice(1)).slice(-20))===address.toLowerCase();
  }catch{return false;}
}

export async function handleProWalletProof({entity,request,response,owner,store,readBody,respond}) {
  if(entity!=='pro/wallet/challenge' && entity!=='pro/wallet/verify'
     && entity!=='pro/wallets')return false;
  if(!store.proWalletReady?.()) {
    respond(response,503,{error:'pro_wallet_verification_unavailable'});return true;
  }
  if(entity==='pro/wallets'){
    if(request.method!=='GET'){respond(response,405,{error:'method_not_allowed'});return true;}
    respond(response,200,{wallets:await store.getProWallets(owner)});return true;
  }
  if(request.method!=='POST'){respond(response,405,{error:'method_not_allowed'});return true;}
  if(!String(request.headers['content-type']||'').startsWith('application/json')){
    respond(response,415,{error:'unsupported_media_type'});return true;
  }
  const input=await readBody(request,2048);
  if(!input || typeof input!=='object' || Array.isArray(input)){
    respond(response,400,{error:'invalid_wallet_proof'});return true;
  }
  if(entity==='pro/wallet/challenge'){
    if(Object.keys(input).join(',')!=='address' || !PRO_WALLET_ADDRESS.test(input.address)){
      respond(response,400,{error:'invalid_wallet_address'});return true;
    }
    const challenge=await store.createProWalletChallenge(owner,input.address.toLowerCase());
    if(!challenge){respond(response,429,{error:'wallet_proof_rate_limited'});return true;}
    respond(response,201,{challenge});return true;
  }
  if(Object.keys(input).sort().join(',')!=='challenge_id,signature'
     || typeof input.challenge_id!=='string' || !CHALLENGE_ID.test(input.challenge_id)
     || typeof input.signature!=='string' || !PRO_WALLET_SIGNATURE.test(input.signature)){
    respond(response,400,{error:'invalid_wallet_proof'});return true;
  }
  // Consume an expiring, account-scoped nonce before verifying. Even if the
  // signature fails, the challenge cannot be replayed.
  const saved=await store.consumeProWalletChallenge(owner,input.challenge_id);
  if(!saved || !verifyProWalletSignature(saved.message,input.signature,saved.address)){
    respond(response,403,{error:'wallet_signature_invalid'});return true;
  }
  await store.saveProWallet(owner,saved.address);
  respond(response,200,{wallet:{address:saved.address,verified:true}});
  return true;
}
