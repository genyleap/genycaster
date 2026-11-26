import test from 'node:test';
import assert from 'node:assert/strict';
import {generateKeyPairSync,verify} from 'node:crypto';
import {createNodeDirectory,nodes,probeNode,loadDirectory} from './node_directory.mjs';
const response=body=>new Response(JSON.stringify(body),{headers:{'content-type':'application/json'}});
test('directory retains all requested candidates, isolates networks and signs exact bytes',async()=>{
 const {privateKey,publicKey}=generateKeyPairSync('ed25519');
 const handle=createNodeDirectory({privateKeyPem:privateKey.export({format:'pem',type:'pkcs8'}),now:()=>1700000000000});
 let output;const res={setHeader(){},writeHead(code){assert.equal(code,200);return this;},end(body){output=JSON.parse(body);}};
 assert(await handle({method:'GET'},res,new URL('https://genycaster.xyz/api/v1/nodes?network=hypersnap')));
 assert.equal(output.nodes.length,6);
 assert(verify(null,Buffer.from(output.signature.payload,'base64url'),publicKey,Buffer.from(output.signature.value,'base64url')));
 assert.deepEqual(JSON.parse(Buffer.from(output.signature.payload,'base64url')).nodes,output.nodes);
});
test('health requires a real profile and does not claim sync or signer eligibility',async()=>{
 const node=nodes[0];
 const info={version:'0.17.21',numShards:2,shardInfos:[{},{}]};
 const good=await probeNode(node,{fetchImpl:async url=>response(url.endsWith('/v1/info')?info:{user:{fid:3}})});
 assert.equal(good.status,'healthy');assert.equal(good.syncStatus,'unknown');assert.equal(good.capabilities.write,'unknown');
 const bad=await probeNode(node,{fetchImpl:async url=>response(url.endsWith('/v1/info')?info:{user:{fid:4}})});
 assert.equal(bad.status,'incompatible');
});
test('setup eligibility rejects lagging shard zero even if profile works',async()=>{
 const info={version:'0.17.21',numShards:2,shardInfos:[{shardId:0,blockDelay:2198},{shardId:1,blockDelay:1}]};
 const fetchImpl=async url=>response(url.endsWith('/v1/info')?info:{user:{fid:3}});
 const result=await probeNode(nodes[0],{fetchImpl});
 assert.equal(result.status,'degraded');
 assert.equal(result.syncStatus,'lagging');
 assert.equal(result.capabilities.nativeRegistration,'unknown');
 const readyInfo={...info,shardInfos:[{shardId:0,blockDelay:2},{shardId:1,blockDelay:1}]};
 const challenge={active:true,anchorBlockHash:'0x'+'a'.repeat(64),anchorBlockHeight:10,nextBlock:11,anchorWindow:20,minDifficultyBits:1,firstNativeFid:'4294967296'};
 const ready=await probeNode(nodes[2],{fetchImpl:async url=>response(url.endsWith('/v1/info')?readyInfo:url.endsWith('/v1/nativeRegistration')?challenge:{user:{fid:3}})});
 assert.equal(ready.capabilities.nativeRegistration,'preflight_available');
 assert.equal(ready.capabilities.write,'unknown');
});
test('unreachable and auth gated endpoints are not eligible',async()=>{
 assert.equal((await probeNode(nodes[1],{fetchImpl:async()=>{throw Error('DNS');}})).status,'unreachable');
 assert.equal((await probeNode(nodes.find(n=>n.network==='farcaster'),{fetchImpl:async()=>new Response('',{status:403})})).status,'auth_required');
});

test('operator directory supports additional read-only nodes and signed revisions',async()=>{
 const {privateKey}=generateKeyPairSync('ed25519');
 const directory={revision:2,nodes:[...nodes,{id:'directory-future',name:'Future',endpoint:'https://node.genycaster.xyz',network:'hypersnap',contract:'hypersnap-v2',enabled:true,priority:50}]};
 const handle=createNodeDirectory({directory,privateKeyPem:privateKey.export({format:'pem',type:'pkcs8'})});
 let output;const res={setHeader(){},writeHead(code){assert.equal(code,200);return this;},end(body){output=JSON.parse(body);}};
 await handle({method:'GET'},res,new URL('https://genycaster.xyz/api/v1/nodes?format=2'));
 assert.equal(output.revision,2);
 assert.equal(output.nodes.length,12);
 assert.deepEqual(output.nodes.at(-1).interfaces[0].access,['read']);
});

test('extended format includes all nodes; legacy clients receive compatible subset',async()=>{
 const handle=createNodeDirectory();
 let output;const res={setHeader(){},writeHead(code){assert.equal(code,200);return this;},end(body){output=JSON.parse(body);}};
 await handle({method:'GET'},res,new URL('https://genycaster.xyz/api/v1/nodes?format=2'));
 assert.equal(output.nodes.length,11);
 assert.equal(output.nodes.filter(n=>n.network==='hypersnap').length,8);
 assert.equal(output.nodes.filter(n=>n.registrationAllowed).length,3);
 assert.equal(output.nodes.filter(n=>n.contract==='hypersnap-raw-v1').length,2);
 await handle({method:'GET'},res,new URL('https://genycaster.xyz/api/v1/nodes'));
 assert.equal(output.nodes.length,7);
 assert.ok(output.nodes.every(n=>n.contract!=='hypersnap-raw-v1'&&!new URL(n.endpoint).port));
});
