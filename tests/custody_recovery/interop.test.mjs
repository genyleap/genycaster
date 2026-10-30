import {execFileSync} from 'node:child_process';
import assert from 'node:assert/strict';
import {Signature} from '../../ui/wasm/vendor/noble-secp256k1.mjs';
import {keccak256} from '../../ui/wasm/browser_protocol_signer.mjs';
import {fromHex,hex} from '../../ui/wasm/farcaster_protocol.mjs';

if (!process.argv[2]) throw Error('Native C++ custody proof binary not supplied');
const message =
  'genyleap.com wants you to sign in with your Ethereum account:\n'
  +'0xf39fd6e51aad88f6f4ce6ab8827279cfffb92266\n\nFarcaster Auth\n\n'
  +'URI: https://genyleap.com/account\n'
  +'Version: 1\n'
  +'Chain ID: 10\n'
  +'Nonce: abcdefgh123456\n'
  +'Issued At: 2026-10-30T00:00:00Z\n'
  +'Resources:\n- farcaster://fid/123';
const proof=execFileSync(process.argv[2],[],{encoding:'utf8',timeout:4000}).trim();
assert.match(proof,/^0x[0-9a-f]{130}$/);
const bytes=fromHex(proof,65);
assert.ok([27,28].includes(bytes[64]));
const encoded=new TextEncoder().encode(message);
const prefixed=new TextEncoder().encode('\x19Ethereum Signed Message:\n'+encoded.length);
const digest=keccak256(new Uint8Array([...prefixed,...encoded]));
const signature=Signature.fromBytes(bytes.subarray(0,64)).addRecoveryBit(bytes[64]-27);
assert.equal(signature.hasHighS(),false,'native signature must be canonical low-S');
const pub=signature.recoverPublicKey(digest).toBytes(false);
assert.equal('0x'+hex(keccak256(pub.subarray(1)).slice(-20)),
  '0xf39fd6e51aad88f6f4ce6ab8827279cfffb92266',
  'independent Noble ECDSA verification must recover Hardhat custody address');
console.log('Native OpenSSL EIP-191/low-S proof independently verified with Noble secp256k1');
