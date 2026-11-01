/**
 * Farcaster custody recovery (BIP39 -> BIP32 m/44'/60'/0'/0/0).
 *
 * Quorum Mobile reference: services/onboarding/farcasterService.ts.
 * Unlike mobile's OS SecureStore, a browser has no portable equivalent of an
 * OS keychain. The recovery phrase and derived secp256k1 private key are used
 * ONLY during explicit signer authorization and are not persisted.
 *
 * Cryptographic primitives: audited noble-secp256k1 v2.3.0 (MIT), pinned in
 * ./vendor; BIP39 English wordlist: scure-bip39 (MIT), also vendored.
 * All hashing / KDFs use the browser WebCrypto implementation.
 */
import { CURVE, getPublicKey, signAsync } from './vendor/noble-secp256k1.mjs';
import { wordlist } from './vendor/bip39-english.mjs';
import { keccak256 } from './browser_protocol_signer.mjs';
import { hex, fromHex } from './farcaster_protocol.mjs';

const cryptoWeb = globalThis.crypto;
const encoder = new TextEncoder();
const wordToIndex = new Map(wordlist.map((word,index)=>[word,index]));
const hard = 0x80000000;
const path = [44+hard,60+hard,hard,0,0];
function invariant(yes,why){if(!yes)throw new Error('Recovery phrase: '+why);}
const concat=(...arrays)=>{
    const result=new Uint8Array(arrays.reduce((n,a)=>n+a.length,0));
    let index=0;
    for(const a of arrays){result.set(a,index);index+=a.length;}
    return result;
};
const word32=(n)=>Uint8Array.of((n>>>24)&255,(n>>>16)&255,(n>>>8)&255,n&255);
const uint256=(value)=>fromHex(BigInt(value).toString(16).padStart(64,'0'),32);
const hashText=(text)=>keccak256(encoder.encode(text));
async function hmac512(key,data){
    const k=await cryptoWeb.subtle.importKey('raw',key,{name:'HMAC',hash:'SHA-512'},false,['sign']);
    return new Uint8Array(await cryptoWeb.subtle.sign('HMAC',k,data));
}
export async function validateRecoveryWords(phrase){
    invariant(typeof phrase==='string' && phrase.length>0 && phrase.length<4096,'invalid input size');
    const normalized=phrase.normalize('NFKD').toLowerCase().trim();
    const words=normalized.split(/\s+/u);
    invariant(words.length===24,'enter a 24-word Farcaster recovery phrase');
    let bits='';
    for(const word of words){
        const index=wordToIndex.get(word);
        invariant(index!==undefined,'unrecognized BIP39 recovery word');
        bits+=index.toString(2).padStart(11,'0');
    }
    const entropySize=words.length/3*4; // 256 bits
    const entropyBits=bits.slice(0,entropySize*8);
    const checkBits=bits.slice(entropySize*8);
    const entropy=Uint8Array.from({length:entropySize},(_,i)=>parseInt(entropyBits.slice(i*8,i*8+8),2));
    try{
        const digest=new Uint8Array(await cryptoWeb.subtle.digest('SHA-256',entropy));
        const check=[...digest].map(x=>x.toString(2).padStart(8,'0')).join('').slice(0,checkBits.length);
        invariant(check===checkBits,'BIP39 checksum is invalid');
        return normalized;
    }finally{entropy.fill(0);}
}
export async function deriveFarcasterCustody(phrase){
    invariant(cryptoWeb?.subtle,'WebCrypto must be enabled in a secure origin');
    const normalized=await validateRecoveryWords(phrase);
    // Use the standard empty BIP39 optional passphrase, matching Quorum Mobile.
    const normalizedBytes=encoder.encode(normalized);
    const salt=encoder.encode('mnemonic');
    const pbkdfKey=await cryptoWeb.subtle.importKey('raw',normalizedBytes,'PBKDF2',false,['deriveBits']);
    normalizedBytes.fill(0);
    const seed=new Uint8Array(await cryptoWeb.subtle.deriveBits({
        name:'PBKDF2',hash:'SHA-512',salt,iterations:2048,
    },pbkdfKey,512));
    let key=new Uint8Array(32);
    let chain=new Uint8Array(32);
    try{
        const master=await hmac512(encoder.encode('Bitcoin seed'),seed);
        key.set(master.subarray(0,32));chain.set(master.subarray(32));master.fill(0);
        for(const index of path){
            const data=index>=hard
                ?concat(Uint8Array.of(0),key,word32(index))
                :concat(getPublicKey(key,true),word32(index));
            const result=await hmac512(chain,data);
            data.fill(0);
            const left=BigInt('0x'+hex(result.subarray(0,32)));
            const right=BigInt('0x'+hex(key));
            invariant(left<CURVE.n,'BIP32 invalid child derivation');
            const child=(left+right)%CURVE.n;
            invariant(child!==0n,'BIP32 zero child key');
            const next=uint256(child);
            key.fill(0);chain.fill(0);
            key=next;
            chain=result.slice(32);
            result.fill(0);
        }
        const publicKey=getPublicKey(key,false);
        const address='0x'+hex(keccak256(publicKey.subarray(1)).slice(-20));
        return Object.freeze({
            address,
            /**
             * Signs one OpenProof SIWF EIP-191 challenge in local memory.
             * No phrase, custody secret or re-usable signature is persisted.
             * OpenProof must bind the returned fresh challenge to this FID.
             */
            async signMessage(message){
                invariant(typeof message==='string' && message.length>=40
                          && message.length<=8192 && !message.includes('\0'),
                          'invalid OpenProof challenge');
                const text=encoder.encode(message);
                const prefix=encoder.encode('\x19Ethereum Signed Message:\n'+text.length);
                const digest=keccak256(concat(prefix,text));
                const signature=await signAsync(digest,key,{lowS:true,extraEntropy:true});
                invariant(signature.recovery===0||signature.recovery===1,
                    'unexpected EIP-191 recovery bit');
                const recoveredPublic=signature.recoverPublicKey(digest).toBytes(false);
                const recoveredAddress='0x'+hex(keccak256(recoveredPublic.subarray(1)).slice(-20));
                invariant(recoveredAddress===address,
                    'custody EIP-191 signature failed address verification');
                text.fill(0);
                return '0x'+hex(concat(signature.toBytes(),
                    Uint8Array.of(signature.recovery+27)));
            },
            /**
             * Signs Farcaster KeyAdd/SignedKeyRequest EIP-712 data.
             * The caller MUST dispose this custody key after proof.
             */
            async signTypedData(document){
                const digest=eip712Digest(document);
                const signature=await signAsync(digest,key,{lowS:true,extraEntropy:true});
                invariant(signature.recovery===0||signature.recovery===1,
                    'unexpected secp256k1 recovery bit');
                const recoveredPublic=signature.recoverPublicKey(digest).toBytes(false);
                const recoveredAddress='0x'+hex(keccak256(recoveredPublic.subarray(1)).slice(-20));
                invariant(recoveredAddress===address,
                    'custody EIP-712 signature failed recovery verification');
                return '0x'+hex(concat(signature.toBytes(),
                    Uint8Array.of(signature.recovery+27)));
            },
            dispose(){key.fill(0);chain.fill(0);}
        });
    }catch(error){key.fill(0);chain.fill(0);throw error;}
    finally{seed.fill(0);}
}

const domainTypeHash=hashText('EIP712Domain(string name,string version,uint256 chainId)');
const keyRequestTypeHash=hashText('SignedKeyRequest(uint256 requestFid,bytes key,uint256 deadline)');
const keyAddTypeHash=hashText('KeyAdd(uint256 fid,bytes key,uint32 keyType,uint32[] scopes,uint32 ttl,uint32 nonce,uint256 deadline)');

function eip712Digest(document){
    invariant(document && document.domain?.name==='Farcaster KeyAdd' &&
        document.domain.version==='1' && document.domain.chainId===1,
        'unexpected Farcaster EIP-712 domain');
    const d=document.domain;
    const domainHash=keccak256(concat(domainTypeHash,hashText(d.name),
        hashText(d.version),uint256(d.chainId)));
    let digest;
    const msg=document.message;
    if(document.primaryType==='SignedKeyRequest'){
        digest=keccak256(concat(keyRequestTypeHash,uint256(msg.requestFid),
            keccak256(fromHex(msg.key,32)),uint256(msg.deadline)));
    }else if(document.primaryType==='KeyAdd'){
        invariant(msg.keyType===1 && Array.isArray(msg.scopes) &&
            msg.scopes.length>0,'invalid signer scopes');
        const scopesHash=keccak256(concat(...msg.scopes.map(x=>uint256(x))));
        digest=keccak256(concat(keyAddTypeHash,uint256(msg.fid),
            keccak256(fromHex(msg.key,32)),uint256(msg.keyType),
            scopesHash,uint256(msg.ttl),uint256(msg.nonce),uint256(msg.deadline)));
    }else throw new Error('Unsupported EIP-712 signer message type');
    return keccak256(concat(Uint8Array.of(0x19,0x01),domainHash,digest));
}
