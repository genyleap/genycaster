import test from 'node:test';
import assert from 'node:assert/strict';
import {getPublicKey,signAsync} from '../../../ui/wasm/vendor/noble-secp256k1.mjs';
import {keccak256} from '../../../ui/wasm/browser_protocol_signer.mjs';
import {hex} from '../../../ui/wasm/farcaster_protocol.mjs';
import {
  exactAddress,hashFarcasterTransfer,verifyFarcasterTransfer,
  createOptimismIdRegistryReader,OPTIMISM_ID_REGISTRY
} from '../neynar_id_registry.mjs';
import {createNeynarRegistrationRelay} from '../neynar_registration_relay.mjs';

const priv=Uint8Array.from({length:32},(_,i)=>i+1);
const custody='0x'+hex(keccak256(getPublicKey(priv,false).slice(1)).slice(-20));
const now=1794562200000;
const fid='123456';
const hexWord=n=>'0x'+BigInt(n).toString(16).padStart(64,'0');
async function signed(fidValue=fid, nonce='3',deadline=now/1000+1200) {
  const digest=hashFarcasterTransfer({
    fid:fidValue,to:custody,nonce,deadline:String(deadline)});
  const sig=await signAsync(digest,priv);
  return '0x'+hex(sig.toCompactRawBytes())+(27+sig.recovery).toString(16);
}
class MemoryLease {
  byId=new Map(); byOwner=new Map();
  async acquire(args) {
    const key=args.principal+'/'+args.custody;
    if(this.byOwner.has(key))return {...this.byId.get(this.byOwner.get(key))};
    const lease={...args,status:'reserving',started:false,fid:null,
                 reservedAt:null,txHash:null};
    this.byId.set(lease.id,lease);this.byOwner.set(key,lease.id);
    return {...lease};
  }
  async get(id,principal) {
    const lease=this.byId.get(id);
    return lease?.principal===principal?{...lease}:null;
  }
  async claimStart(id,principal) {
    const row=await this.get(id,principal);
    if(!row||row.started||row.status!=='reserving')return false;
    this.byId.get(id).started=true;return true;
  }
  async setReserved(id,principal,{fid:reservedFid,reservedAt}) {
    const row=await this.get(id,principal);
    if(!row||row.status!=='reserving'||!row.started)throw Error('conflict');
    Object.assign(this.byId.get(id),{fid:reservedFid,reservedAt,status:'reserved'});
  }
  async claimSubmit(id,principal) {
    const row=await this.get(id,principal);
    if(!row||row.status!=='reserved')return false;
    this.byId.get(id).status='submitting';return true;
  }
  async setOutcome(id,principal,patch) {
    const row=await this.get(id,principal);
    if(!row)return false;
    if(row.status==='confirmed'&&patch.status!=='confirmed')return false;
    Object.assign(this.byId.get(id),patch);return true;
  }
}
const key='a553b925-1173-4140-a3e0-409d4b1507c5';
function mockContext(overrides={}) {
  const ledger=new MemoryLease();
  const state={registered:false,confirmed:false,nonce:'3',getCount:0,postCount:0};
  const registry={
    idOf:async()=>state.registered?fid:'0',
    nonce:async()=>{if(state.failNonce)throw Error('offline');return state.nonce;},
    confirmed:async(id,address)=>state.confirmed&&id===fid&&address===custody
  };
  const fetchImpl=async(url,opts)=>{
    assert.equal(opts.headers['x-api-key'],'mock-secret');
    assert.equal(opts.headers['x-wallet-id'],'mock-app-wallet');
    if(url.endsWith('/fid')) {
      state.getCount++;
      return new Response(JSON.stringify({fid:Number(fid)}),{status:200});
    }
    assert.ok(url.endsWith('/user/'));
    state.postCount++;
    if(overrides.failPost)throw Error('simulated network timeout');
    const p=JSON.parse(opts.body);
    assert.equal(p.fid,Number(fid));assert.equal(p.requested_user_custody_address,custody);
    assert.ok(!Object.keys(p).some(k=>/mnemonic|private/i.test(k)));
    return new Response(JSON.stringify({success:true}),{status:200});
  };
  const authorizeEnrollment=async(enrollment,address)=>{
    if(enrollment!=='paid-valid-proof'||address!==custody)return null;
    return {approved:true,subject:'funded-enrollment-unique-subject'};
  };
  const relay=createNeynarRegistrationRelay({
    apiKey:'mock-secret',walletId:'mock-app-wallet',
    ledger,registry,fetchImpl,authorizeEnrollment,now:()=>now
  });
  return {relay,state,ledger};
}
test('rejects service startup without separate funding authorization and durable ledger',()=>{
  assert.throws(()=>createNeynarRegistrationRelay({
    apiKey:'key',walletId:'wallet',registry:{},ledger:{}}),/requires/);
});
test('Farcaster Transfer signature verifies exactly one custody address and domain',async()=>{
  const signature=await signed();
  assert.equal(verifyFarcasterTransfer({
    fid,to:custody,nonce:'3',deadline:String(now/1000+1200),signature}),true);
  assert.equal(verifyFarcasterTransfer({
    fid,to:custody,nonce:'4',deadline:String(now/1000+1200),signature}),false);
  assert.equal(verifyFarcasterTransfer({
    fid:'777',to:custody,nonce:'3',deadline:String(now/1000+1200),signature}),false);
  assert.equal(verifyFarcasterTransfer({
    fid,to:'0x'+'0'.repeat(40),nonce:'3',deadline:String(now/1000+1200),signature}),false);
  assert.equal(verifyFarcasterTransfer({
    fid,to:custody,nonce:'3',deadline:String(now/1000+1200),signature:'0x'+ '00'.repeat(65)}),false);
});
test('reserve requires funded enrollment and uses a durable single lease',async()=>{
  const {relay,state}=mockContext();
  await assert.rejects(relay.reserve({
    enrollment:'none',custodyAddress:custody,idempotencyKey:key}),/not_authorized/);
  assert.equal(state.getCount,0);
  const args={enrollment:'paid-valid-proof',custodyAddress:custody,idempotencyKey:key};
  const reservation=await relay.reserve(args);
  assert.equal(reservation.fid,fid);
  assert.equal(reservation.status,'reserved');
  assert.equal(reservation.id_registry_nonce,'3');
  assert.equal(reservation.custody_address,custody);
  assert.equal(reservation.expires_at,now+600000);
  assert.equal(state.getCount,1);
  assert.equal((await relay.reserve(args)).reservation_id,key);
  assert.equal(state.getCount,1);
  assert.ok(!JSON.stringify(reservation).includes('mock-secret'));
});
test('validates exact signed nonce before irreversible transfer and confirms IdRegistry separately',async()=>{
  const {relay,state}=mockContext();
  const args={enrollment:'paid-valid-proof',custodyAddress:custody,idempotencyKey:key};
  await relay.reserve(args);
  const call={enrollment:args.enrollment,custodyAddress:custody,reservationId:key,
    deadline:String(now/1000+1200),fname:'genycaster',signature:await signed()};
  await assert.rejects(relay.submit({...call,signature:await signed(fid,'9')}),/invalid_custody_signature/);
  assert.equal(state.postCount,0);
  const result=await relay.submit(call);
  assert.equal(state.postCount,1);
  assert.equal(result.status,'submitted');
  assert.equal((await relay.submit(call)).status,'submitted');
  assert.equal(state.postCount,1);
  state.confirmed=true;
  const result2=await relay.status({
    enrollment:call.enrollment,custodyAddress:custody,reservationId:key});
  assert.equal(result2.status,'confirmed');
});
test('ambiguous transfer result cannot silently replay and never reports confirmed',async()=>{
  const {relay,state}=mockContext({failPost:true});
  await relay.reserve({enrollment:'paid-valid-proof',custodyAddress:custody,idempotencyKey:key});
  const call={enrollment:'paid-valid-proof',custodyAddress:custody,
    reservationId:key,deadline:String(now/1000+1200),fname:'genycaster',
    signature:await signed()};
  assert.equal((await relay.submit(call)).status,'submission_unknown');
  assert.equal((await relay.submit(call)).status,'submission_unknown');
  assert.equal(state.postCount,1);
  assert.equal((await relay.status({enrollment:'paid-valid-proof',
      custodyAddress:custody,reservationId:key})).status,'submission_unknown');
});
test('registered custody prevents cost-bearing reserved FID allocation',async()=>{
  const {relay,state}=mockContext();
  state.registered=true;
  await assert.rejects(relay.reserve({
    enrollment:'paid-valid-proof',custodyAddress:custody,idempotencyKey:key
  }),/custody_already_registered/);
  assert.equal(state.getCount,0);
});
test('safe Optimism registry reads require matching idOf and custodyOf',async()=>{
  let owned='123456';
  const responses=[];
  const selector=(name)=>'0x'+hex(keccak256(new TextEncoder().encode(name)).slice(0,4));
  const registry=createOptimismIdRegistryReader({
    rpcUrl:'https://rpc.example/',
    fetchImpl:async(_url,opts)=>{
      const body=JSON.parse(opts.body);responses.push(body);
      const data=body.params[0].data;
      let value;
      if(data.startsWith(selector('idOf(address)')))value=BigInt(owned);
      else if(data.startsWith(selector('custodyOf(uint256)')))value=BigInt(custody);
      else if(data.startsWith(selector('nonces(address)')))value=3n;
      else throw Error('unexpected eth_call');
      return new Response(JSON.stringify({jsonrpc:'2.0',id:1,
        result:hexWord(value)}),{status:200});
    }
  });
  assert.equal(await registry.nonce(custody),'3');
  assert.equal(await registry.confirmed(fid,custody),true);
  owned='0';
  assert.equal(await registry.confirmed(fid,custody),false);
  assert.ok(responses.every(r=>r.params[0].to===OPTIMISM_ID_REGISTRY
    &&r.params[1]==='safe'));
  assert.throws(()=>createOptimismIdRegistryReader({
    rpcUrl:'http://rpc.example/'}),/HTTPS/);
});
test('temporary RPC failure does not destroy an already paid reservation',async()=>{
  const {relay,state}=mockContext();
  state.failNonce=true;
  const params={enrollment:'paid-valid-proof',custodyAddress:custody,
    idempotencyKey:key};
  const result=await relay.reserve(params);
  assert.equal(result.status,'reserved');
  assert.equal(result.id_registry_nonce,null);
  assert.equal(state.getCount,1);
  state.failNonce=false;
  const checked=await relay.status({
    enrollment:params.enrollment,custodyAddress:custody,reservationId:key
  });
  assert.equal(checked.id_registry_nonce,'3');
  assert.equal(state.getCount,1);
});

test('migration 013 enforces one funded invoice and one custody per registration',async()=>{
  const {readFileSync}=await import('node:fs');
  const ddl=readFileSync(new URL('../migrations/013_neynar_registration.sql',
    import.meta.url),'utf8');
  assert.match(ddl,/principal text NOT NULL UNIQUE/);
  assert.match(ddl,/custody varchar\(42\) NOT NULL UNIQUE/);
  assert.match(ddl,/UNIQUE \(principal,custody\)/);
  assert.match(ddl,/started boolean NOT NULL DEFAULT false/);
  assert.match(ddl,/INSERT INTO schema_migrations\(version\) VALUES\(13\)/);
});
