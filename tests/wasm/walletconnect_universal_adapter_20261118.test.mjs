import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdtempSync,rmSync} from 'node:fs';
import {EventEmitter} from 'node:events';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {asEthereumProvider,canonicalEvmChain}
  from '../../ui/wasm/host-sdk/walletconnect-eip1193.mjs';

const address='0x00000000000000000000000000000000000000a1';
const session=(chain='1')=>({
  namespaces:{eip155:{
    accounts:[`eip155:${chain}:${address}`],
    chains:[`eip155:${chain}`],
    methods:['personal_sign'],
    events:['accountsChanged','chainChanged','disconnect']
  }}
});
function fakeUniversal({identity=session(),chain=1}={}){
  const bus=new EventEmitter();
  const calls=[];
  const universal={
    session:identity,
    on:(event,listener)=>bus.on(event,listener),
    removeListener:(event,listener)=>bus.removeListener(event,listener),
    emit:(event,value)=>bus.emit(event,value),
    async request(payload){
      calls.push(payload);
      if(payload.method==='eth_chainId')return chain;
      if(payload.method==='eth_accounts'||payload.method==='eth_requestAccounts')
        return [address];
      if(payload.method==='personal_sign')return '0xwallet-signature';
      return null;
    }
  };
  return {universal,calls,bus};
}

test('canonical chain values preserve valid EVM IDs without accepting unsafe numeric data',()=>{
  for(const [v,expected] of [
    [1,'0x1'],['1','0x1'],['0x1','0x1'],['eip155:8453','0x2105'],
    [8453,'0x2105'],['0x2105','0x2105'],[0,''],[-1,''],
    ['0x0',''],['0xGG',''],['01',''],['1.5',''],
    [Number.MAX_SAFE_INTEGER+1,''],['9007199254740992',''],
    ['solana:1',''],[null,''],[{},''],[1.5,'']
  ])assert.equal(canonicalEvmChain(v),expected,String(v));
});

test('EIP-1193 facade retains approved session and forwards wallet-owned signing',async()=>{
  const {universal,calls}=fakeUniversal({chain:8453});
  const eip=asEthereumProvider(universal);
  assert.equal(eip.session,universal.session);
  assert.equal(await eip.request({method:'eth_chainId'}),'0x2105');
  assert.deepEqual(await eip.request({method:'eth_accounts'}),[address]);
  assert.equal(await eip.request({method:'personal_sign',params:['0x01',address]}),
    '0xwallet-signature');
  assert.deepEqual(calls.map(x=>x.method),['eth_chainId','eth_accounts','personal_sign']);
  assert.equal(Object.hasOwn(eip,'privateKey'),false);
});

test('an unsupported/non-EVM session cannot enable any wallet RPC',async()=>{
  const {universal,calls}=fakeUniversal({
    identity:{namespaces:{solana:{accounts:['solana:mainnet:FakeKey']}}}
  });
  const eip=asEthereumProvider(universal);
  assert.equal(eip.session,null);
  await assert.rejects(eip.request({method:'personal_sign',params:[]}),e=>e.code===4900);
  assert.deepEqual(calls,[]);
});

test('empty EVM session and malformed chain response fail closed',async()=>{
  const {universal,calls}=fakeUniversal({
    identity:{namespaces:{eip155:{accounts:[]}}},
    chain:Number.MAX_SAFE_INTEGER+1
  });
  const eip=asEthereumProvider(universal);
  assert.equal(eip.session,null);
  await assert.rejects(eip.request({method:'eth_accounts'}),e=>e.code===4900);
  assert.equal(calls.length,0);
  universal.session=session();
  await assert.rejects(eip.request({method:'eth_chainId'}),e=>e.code===4900);
});

test('a late wallet signature is discarded after silent session revocation',async()=>{
  const {universal}=fakeUniversal();
  let resolveSignature;
  universal.request=()=>new Promise(resolve=>{resolveSignature=resolve;});
  const eip=asEthereumProvider(universal);
  const pending=eip.request({method:'personal_sign',params:['0x01',address]});
  universal.session=null; // the transport may not emit disconnect in time
  resolveSignature('0xstale');
  await assert.rejects(pending,e=>e.code===4100);
});

test('a replaced topic or approved account cannot inherit an older RPC',async()=>{
  const {universal}=fakeUniversal();
  universal.session.topic='old-topic';
  let resolveSignature;
  universal.request=()=>new Promise(resolve=>{resolveSignature=resolve;});
  const eip=asEthereumProvider(universal);
  const pending=eip.request({method:'personal_sign',params:['0x01',address]});
  universal.session={...session(),topic:'new-topic'};
  resolveSignature('0xstale');
  await assert.rejects(pending,e=>e.code===4100);
  let complete;
  universal.request=()=>new Promise(resolve=>{complete=resolve;});
  const again=eip.request({method:'personal_sign',params:['0x01',address]});
  universal.session.namespaces.eip155.accounts=[
    'eip155:1:0x00000000000000000000000000000000000000b2'];
  complete('0xstale-too');
  await assert.rejects(again,e=>e.code===4100);
});

test('silent default-chain rotation invalidates an in-flight signing result',async()=>{
  const {universal}=fakeUniversal();
  let chain='1';
  universal.rpcProviders={eip155:{getDefaultChain:()=>chain}};
  let finish;
  universal.request=()=>new Promise(resolve=>{finish=resolve;});
  const eip=asEthereumProvider(universal);
  const pending=eip.request({method:'personal_sign',params:['0x01',address]});
  chain='8453';
  finish('0xstale');
  await assert.rejects(pending,e=>e.code===4100);
});

test('account/chain/disconnect events remain revocable and listeners are detachable',()=>{
  const {universal,bus}=fakeUniversal();
  const eip=asEthereumProvider(universal);
  const chains=[];
  const accounts=[];
  const disconnects=[];
  const changed=chain=>chains.push(chain);
  eip.on('chainChanged',changed);
  eip.on('chainChanged',changed); // no duplicate subscriptions
  const accountChange=a=>accounts.push(a);
  const disconnected=e=>disconnects.push(e);
  eip.on('accountsChanged',accountChange);
  eip.on('disconnect',disconnected);
  universal.emit('chainChanged','eip155:8453');
  universal.emit('chainChanged','bad-chain');
  universal.emit('accountsChanged',[]);
  universal.emit('disconnect',{code:4900});
  assert.deepEqual(chains,['0x2105','']);
  assert.deepEqual(accounts,[[]]);
  assert.deepEqual(disconnects,[{code:4900}]);
  eip.removeListener('chainChanged',changed);
  eip.removeListener('accountsChanged',accountChange);
  eip.removeListener('disconnect',disconnected);
  assert.equal(bus.listenerCount('chainChanged'),0);
  assert.equal(bus.listenerCount('accountsChanged'),0);
  assert.equal(bus.listenerCount('disconnect'),0);
});

test('lockfile removes vulnerable Reown Zod 3 and keeps the official SDK pinned',()=>{
  const manifest=JSON.parse(readFileSync(new URL(
    '../../ui/wasm/host-sdk/package.json',import.meta.url)));
  const lock=JSON.parse(readFileSync(new URL(
    '../../ui/wasm/host-sdk/package-lock.json',import.meta.url)));
  assert.equal(manifest.dependencies['@walletconnect/universal-provider'],'2.26.0');
  assert.equal(manifest.dependencies['@walletconnect/ethereum-provider'],undefined);
  assert.equal(lock.packages['node_modules/@walletconnect/universal-provider']?.version,'2.26.0');
  assert.equal(lock.packages['node_modules/@walletconnect/ethereum-provider'],undefined);
  assert.equal(lock.packages['node_modules/@reown/appkit-wallet'],undefined);
  const zods=Object.entries(lock.packages).filter(([name])=>
    name==='node_modules/zod'||name.endsWith('/node_modules/zod'));
  assert.ok(zods.length>0);
  assert.ok(zods.every(([,entry])=>entry.version==='4.6.7'),
    'the lock must not reintroduce a vulnerable Zod 3 instance');
});

function driverWith(universal){
  // Evaluate only the checked-in, first-party driver, with the network SDK
  // injected as a fake. Never call the relay or create live wallet sessions.
  const code=readFileSync(new URL(
    '../../ui/wasm/host-sdk/walletconnect-entry.mjs',import.meta.url),'utf8')
    .replace(/^import .*;$/gm,'').replace(/^export /gm,'');
  const fakeSdk={init:async()=>universal};
  const qr={toDataURL:async()=>''};
  return new Function('UniversalProvider','asEthereumProvider','QRCode','window',
    code+'\nreturn {connectWalletConnect,supportedChains};')(
    fakeSdk,asEthereumProvider,qr,{location:{origin:'https://genycaster.xyz'}});
}

test('WalletConnect pairing passes approved optional namespace to the real SDK boundary',async()=>{
  const {universal}=fakeUniversal({identity:null,chain:8453});
  let options;
  let complete;
  const uri='wc:abcdef01@2?relay-protocol=irn&symKey=012345';
  universal.connect=opts=>{
    options=opts;
    const pending=new Promise(resolve=>{complete=resolve;});
    queueMicrotask(()=>universal.emit('display_uri',uri));
    return pending;
  };
  universal.abortPairingAttempt=()=>{};
  const driver=driverWith(universal);
  const statuses=[];
  const transport=await driver.connectWalletConnect({
    onUri:received=>{
      assert.equal(received,uri);
      universal.session=session('8453');
      complete(universal.session);
    },
    onStatus:status=>statuses.push(status)
  });
  assert.equal(transport.session,universal.session);
  assert.equal(await transport.request({method:'eth_chainId'}),'0x2105');
  assert.deepEqual(options.optionalNamespaces.eip155.chains,driver.supportedChains);
  assert.ok(options.optionalNamespaces.eip155.methods.includes('personal_sign'));
  assert.ok(options.optionalNamespaces.eip155.events.includes('accountsChanged'));
  assert.deepEqual(statuses,['initializing','creating','approval','connected']);
});

test('reconnecting to the same approved wallet preserves event facade identity',async()=>{
  const {universal}=fakeUniversal({identity:session()});
  let reconnected=0;
  universal.connect=()=>{reconnected++;return Promise.resolve(universal.session);};
  const driver=driverWith(universal);
  const first=await driver.connectWalletConnect();
  const second=await driver.connectWalletConnect();
  assert.strictEqual(first,second);
  assert.equal(reconnected,0,'an existing approved session must not pair again');
});

test('aborted WalletConnect pairing cannot create an EVM signer session',async()=>{
  const {universal}=fakeUniversal({identity:null});
  let aborts=0;
  universal.abortPairingAttempt=()=>{aborts++;};
  universal.connect=()=>{
    queueMicrotask(()=>universal.emit('display_uri','wc:abc123@2?relay-protocol=irn'));
    return new Promise(()=>{});
  };
  const controller=new AbortController();
  const driver=driverWith(universal);
  const result=driver.connectWalletConnect({
    signal:controller.signal,
    onUri:()=>controller.abort()
  });
  await assert.rejects(result,e=>e.code===4001);
  assert.ok(aborts>=1);
  assert.equal(universal.session,null);
});

test('WASM package rejects a generated host with a missing lazy WalletConnect bundle',()=>{
  const dir=mkdtempSync(join(tmpdir(),'genycaster-wc-asset-'));
  const script=new URL('../../ci/package_wasm_assets.py',import.meta.url).pathname;
  try{
    writeFileSync(join(dir,'genycaster.wasm'),Buffer.from([0,97,115,109,1,0,0,0]));
    writeFileSync(join(dir,'genycaster.html'),'<title>Genycaster</title>');
    writeFileSync(join(dir,'genycaster.js'),'/* generated boot fixture */');
    writeFileSync(join(dir,'miniapp-host-bridge.mjs'),'export const active=true;');
    const missing=spawnSync('python3',[script,'--dir',dir],{encoding:'utf8'});
    assert.notEqual(missing.status,0);
    assert.match(missing.stderr,/Mini App host requires genycaster-walletconnect\.mjs/);
    writeFileSync(join(dir,'genycaster-walletconnect.mjs'),'export const active=true;');
    const ready=spawnSync('python3',[script,'--dir',dir],{encoding:'utf8'});
    assert.equal(ready.status,0,ready.stderr);
    const metadata=readFileSync(join(dir,'genycaster-assets.json'),'utf8');
    assert.match(metadata,/genycaster-walletconnect\.mjs/);
  }finally{
    rmSync(dir,{recursive:true,force:true});
  }
});

test('PWA caches only the first-party WalletConnect module, never wallet or account data',()=>{
  const source=readFileSync(new URL(
    '../../ui/wasm/genycaster-sw.js',import.meta.url),'utf8');
  const before=source.slice(0,source.indexOf('self.addEventListener("install"'));
  const origin='https://genycaster.xyz';
  const allowed=new Function('self',before+'\nreturn allowed;')({
    location:{href:origin+'/genycaster-sw.js',origin}
  });
  assert.equal(allowed(new URL(origin+'/genycaster-walletconnect.mjs')),true);
  assert.equal(allowed(new URL(origin+'/miniapp-host-bridge.mjs')),true);
  assert.equal(allowed(new URL('https://untrusted.example/genycaster-walletconnect.mjs')),false);
  assert.equal(allowed(new URL(origin+'/wallet-session.json')),false);
  assert.equal(allowed(new URL(origin+'/private-key.json')),false);
  assert.match(source,/request\.credentials==="include"/);
});

test('CMake builds both first-party Mini App and lazy WalletConnect bundles after npm ci',()=>{
  const cmake=readFileSync(new URL('../../ui/CMakeLists.txt',import.meta.url),'utf8');
  const marker='COMMENT "Bundling pinned Farcaster Mini App host SDK';
  const block=cmake.slice(cmake.indexOf('set(GENYCASTER_MINIAPP_SDK_STAGE'),cmake.indexOf(marker));
  assert.match(block,/walletconnect-entry\.mjs/);
  assert.match(block,/walletconnect-eip1193\.mjs/);
  assert.match(block,/\$<TARGET_FILE_DIR:genycaster>\/miniapp-host-bridge\.mjs/);
  assert.match(block,/\$<TARGET_FILE_DIR:genycaster>\/genycaster-walletconnect\.mjs/);
  assert.ok(block.indexOf('ci\n')<block.lastIndexOf('genycaster-walletconnect.mjs'));
  assert.match(block,/--ignore-scripts --no-audit --no-fund/);
});

test('WalletConnect driver uses origin-bound, optional EVM namespaces without AppKit modal',()=>{
  const driver=readFileSync(new URL(
    '../../ui/wasm/host-sdk/walletconnect-entry.mjs',import.meta.url),'utf8');
  assert.match(driver,/import UniversalProvider from '@walletconnect\/universal-provider'/);
  assert.match(driver,/provider\.connect\(\{optionalNamespaces\}\)/);
  assert.match(driver,/provider\.abortPairingAttempt\?\.\(\)/);
  assert.match(driver,/const eip1193=facadeFor\(provider\)/);
  assert.match(driver,/const ethereumFacades=new WeakMap\(\)/);
  assert.match(driver,/return eip1193;/);
  assert.doesNotMatch(driver,/showQrModal|provider\.signer|EthereumProvider\.init/);
});
