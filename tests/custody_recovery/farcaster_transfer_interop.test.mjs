import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {Signature} from '../../ui/wasm/vendor/noble-secp256k1.mjs';
import {keccak256} from '../../ui/wasm/browser_protocol_signer.mjs';
import {fromHex, hex} from '../../ui/wasm/farcaster_protocol.mjs';

const [custody, fidString, nonceString, deadlineString, signed] =
  execFileSync(process.argv[2],[],{encoding:'utf8',timeout:10000}).trim().split('\n');
const toBytes = x => new TextEncoder().encode(x);
const h = bytes => keccak256(bytes);
const hashText = text => h(toBytes(text));
const bytes = (...items) => Uint8Array.from(items.flatMap(x => [...x]));
const num = x => fromHex(BigInt(x).toString(16).padStart(64,'0'),32);
const address = str => bytes(new Uint8Array(12),fromHex(str,20));
const domainHash = h(bytes(
  hashText('EIP712Domain(string name,string version,uint256 chainId,address verifyingContract)'),
  hashText('Farcaster IdRegistry'),
  hashText('1'),
  num(10),
  address('0x00000000fc6c5f01fc30151999387bb99a9f489b')
));
const typedBody = h(bytes(
  hashText('Transfer(uint256 fid,address to,uint256 nonce,uint256 deadline)'),
  num(fidString),
  address(custody),
  num(nonceString),
  num(deadlineString)
));
const digest = h(bytes(Uint8Array.of(0x19,0x01),domainHash,typedBody));
const proof = fromHex(signed,65);
assert.ok([27,28].includes(proof[64]));
const signature = Signature.fromBytes(proof.slice(0,64)).addRecoveryBit(proof[64]-27);
assert.equal(signature.hasHighS(),false);
const publicKey = signature.recoverPublicKey(digest).toBytes(false);
assert.equal('0x'+hex(h(publicKey.slice(1)).slice(-20)),custody);
const wrongChain = h(bytes(
  hashText('EIP712Domain(string name,string version,uint256 chainId,address verifyingContract)'),
  hashText('Farcaster IdRegistry'), hashText('1'), num(1),
  address('0x00000000fc6c5f01fc30151999387bb99a9f489b')
));
const wrongDigest = h(bytes(Uint8Array.of(0x19,0x01),wrongChain,typedBody));
const wrongPublicKey = signature.recoverPublicKey(wrongDigest).toBytes(false);
assert.notEqual('0x'+hex(h(wrongPublicKey.slice(1)).slice(-20)),custody);
console.log('Farcaster IdRegistry reserved-FID transfer signature independently verified.');
