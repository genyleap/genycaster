import test from 'node:test';
import assert from 'node:assert/strict';
import {createMiniAppEthereumProvider} from '../../ui/wasm/miniapp-wallet-provider.mjs';

const ADDRESS='0x00000000000000000000000000000000000000a1';
const ORIGIN='https://miniapp.example';

function deferred(){
  let resolve;
  let reject;
  const promise=new Promise((a,b)=>{resolve=a;reject=b;});
  return {promise,resolve,reject};
}

function wallet(overrides={}){
  const listeners=new Map();
  const requests=[];
  const injected={
    async request(payload){
      requests.push(payload.method);
      if(overrides[payload.method])return overrides[payload.method](payload);
      if(payload.method==='eth_requestAccounts'||payload.method==='eth_accounts')
        return [ADDRESS];
      if(payload.method==='eth_chainId')return '0x2105';
      if(payload.method==='personal_sign')return '0xvalid';
      if(payload.method==='wallet_revokePermissions')return null;
      return [];
    },
    on(event,fn){
      if(!listeners.has(event))listeners.set(event,new Set());
      listeners.get(event).add(fn);
    },
    removeListener(event,fn){listeners.get(event)?.delete(fn);},
    emit(event,value){for(const fn of listeners.get(event)||[])fn(value);}
  };
  return {injected,requests};
}

function facade(injected){
  return createMiniAppEthereumProvider(ORIGIN,injected,async()=>true,ORIGIN,
    async()=>true);
}

test('a completed permission revoke clears the browser host grant even without wallet events',async()=>{
  const {injected,requests}=wallet();
  const app=facade(injected);
  await app.provider.request({method:'eth_requestAccounts'});
  assert.deepEqual(await app.provider.request({method:'eth_accounts'}),[ADDRESS]);
  await app.provider.request({method:'wallet_revokePermissions',params:[{eth_accounts:{}}]});
  assert.deepEqual(await app.provider.request({method:'eth_accounts'}),[]);
  await assert.rejects(
    app.provider.request({method:'personal_sign',params:['0x01',ADDRESS]}),
    e=>e.code===4100);
  assert.ok(requests.includes('wallet_revokePermissions'));
  assert.equal(requests.includes('personal_sign'),false);
  app.dispose();
});

test('a rejected permission revoke preserves the original grant',async()=>{
  const {injected}=wallet({
    wallet_revokePermissions:()=>{throw Object.assign(new Error('Rejected'),{code:4001});}
  });
  const app=facade(injected);
  await app.provider.request({method:'eth_requestAccounts'});
  await assert.rejects(app.provider.request({method:'wallet_revokePermissions',params:[]}),
    e=>e.code===4001);
  assert.deepEqual(await app.provider.request({method:'eth_accounts'}),[ADDRESS]);
  assert.equal(await app.provider.request({method:'personal_sign',params:['0x01',ADDRESS]}),
    '0xvalid');
  app.dispose();
});

test('a late signature response cannot escape after a chain-change revocation',async()=>{
  const started=deferred();
  const signing=deferred();
  const {injected}=wallet({
    personal_sign:()=>{started.resolve();return signing.promise;}
  });
  const app=facade(injected);
  await app.provider.request({method:'eth_requestAccounts'});
  const pending=app.provider.request({method:'personal_sign',params:['0x01',ADDRESS]});
  await started.promise;
  injected.emit('chainChanged','0x1');
  signing.resolve('0xstale');
  await assert.rejects(pending,e=>e.code===4100);
  assert.deepEqual(await app.provider.request({method:'eth_accounts'}),[]);
  app.dispose();
});

test('a delayed account-connect response cannot restore revoked authorization',async()=>{
  const started=deferred();
  const connect=deferred();
  let attempts=0;
  const {injected}=wallet({
    eth_requestAccounts:()=>{
      if(++attempts===1)return [ADDRESS];
      started.resolve();
      return connect.promise;
    }
  });
  const app=facade(injected);
  await app.provider.request({method:'eth_requestAccounts'});
  const pending=app.provider.request({method:'eth_requestAccounts'});
  await started.promise;
  injected.emit('accountsChanged',[]);
  connect.resolve([ADDRESS]);
  await assert.rejects(pending,e=>e.code===4100);
  assert.deepEqual(await app.provider.request({method:'eth_accounts'}),[]);
  app.dispose();
});

test('a new same-address consent never authorizes an older queued signature',async()=>{
  const approvalStarted=deferred();
  const approvalResult=deferred();
  let approvalCount=0;
  const {injected,requests}=wallet();
  const app=createMiniAppEthereumProvider(ORIGIN,injected,async()=>true,ORIGIN,
    async()=>{
      if(++approvalCount===1){
        approvalStarted.resolve();
        return approvalResult.promise;
      }
      return true;
    });
  await app.provider.request({method:'eth_requestAccounts'});
  const pending=app.provider.request({method:'personal_sign',params:['0x01',ADDRESS]});
  await approvalStarted.promise;
  injected.emit('accountsChanged',[]);
  await app.provider.request({method:'eth_requestAccounts'});
  approvalResult.resolve(true);
  await assert.rejects(pending,e=>e.code===4100);
  assert.equal(requests.includes('personal_sign'),false,
    'the earlier confirmation must not dispatch a signer RPC');
  assert.deepEqual(await app.provider.request({method:'eth_accounts'}),[ADDRESS]);
  app.dispose();
});

test('a delayed privileged read cannot escape a revoked grant',async()=>{
  const started=deferred();
  const response=deferred();
  const {injected}=wallet({
    wallet_getPermissions:()=>{started.resolve();return response.promise;}
  });
  const app=facade(injected);
  await app.provider.request({method:'eth_requestAccounts'});
  const pending=app.provider.request({method:'wallet_getPermissions'});
  await started.promise;
  injected.emit('disconnect',{code:4900});
  response.resolve([{parentCapability:'eth_accounts'}]);
  await assert.rejects(pending,e=>e.code===4100);
  app.dispose();
});

test('the initial wallet accountsChanged event during connection remains supported',async()=>{
  const {injected}=wallet({
    eth_requestAccounts:()=>{
      injected.emit('accountsChanged',[ADDRESS]);
      return [ADDRESS];
    }
  });
  const app=facade(injected);
  assert.deepEqual(await app.provider.request({method:'eth_requestAccounts'}),[ADDRESS]);
  assert.deepEqual(await app.provider.request({method:'eth_accounts'}),[ADDRESS]);
  app.dispose();
});
