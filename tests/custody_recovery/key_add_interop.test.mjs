import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {Signature} from '../../ui/wasm/vendor/noble-secp256k1.mjs';
import {keccak256} from '../../ui/wasm/browser_protocol_signer.mjs';
import {fromHex,hex} from '../../ui/wasm/farcaster_protocol.mjs';

const [deadlineText,keyAddHex,metadataHex] = execFileSync(process.argv[2],[],
  {encoding:'utf8',timeout:5000}).trim().split('\n');
const expectedAddress='0xf278cf59f82edcf871d630f28ecc8056f25c1cdb';
const encoder=new TextEncoder();
const h=bytes=>keccak256(bytes);
const hs=value=>h(encoder.encode(value));
const concat=(...values)=>Uint8Array.from(values.flatMap(x=>[...x]));
const word=value=>fromHex(BigInt(value).toString(16).padStart(64,'0'),32);
const key=fromHex('11'.repeat(32),32);
const domainHash=h(concat(hs('EIP712Domain(string name,string version,uint256 chainId)'),
  hs('Farcaster KeyAdd'),hs('1'),word(1)));
const scopesHash=h(concat(word(1),word(3)));
const addHash=h(concat(
  hs('KeyAdd(uint256 fid,bytes key,uint32 keyType,uint32[] scopes,uint32 ttl,uint32 nonce,uint256 deadline)'),
  word(42),h(key),word(1),scopesHash,word(3600),word(9),word(deadlineText)));
const requestHash=h(concat(
  hs('SignedKeyRequest(uint256 requestFid,bytes key,uint256 deadline)'),
  word(42),h(key),word(deadlineText)));
const digest=hash=>h(concat(Uint8Array.of(0x19,0x01),domainHash,hash));
function verify(serialized,hash){
  const bytes=fromHex(serialized,65);
  assert.ok([27,28].includes(bytes[64]));
  const signature=Signature.fromBytes(bytes.subarray(0,64)).addRecoveryBit(bytes[64]-27);
  assert.equal(signature.hasHighS(),false);
  const publicKey=signature.recoverPublicKey(digest(hash)).toBytes(false);
  assert.equal('0x'+hex(h(publicKey.subarray(1)).slice(-20)),expectedAddress);
}
verify(keyAddHex,addHash);
const metadata=fromHex(metadataHex);
assert.equal(metadata.length,288);
const requestSignature=metadata.subarray(192,257);
verify(hex(requestSignature),requestHash);
assert.equal(hex(metadata.subarray(76,96)),expectedAddress.slice(2));
console.log('Native EIP-712 KEY_ADD and SignedKeyRequest independently verified with Noble.');
