import test from 'node:test';
import assert from 'node:assert/strict';
import {webcrypto} from 'node:crypto';
import {createSetupWallet} from '../../ui/wasm/browser_setup_wallet.mjs';

const phrase=('abandon '.repeat(23)+'art').trim(),address='0x'+'a'.repeat(40);
const hyper='hypersnap-v2',far='farcaster-snapchain';
function fixture(extra={},storage=new Map()){
 const browser={isSecureContext:true,crypto:webcrypto,localStorage:{
  setItem:(k,v)=>storage.set(k,v),getItem:k=>storage.get(k)??null,removeItem:k=>storage.delete(k)
 },navigator:{clipboard:{writeText:async()=>{}}}};
 const wallet=createSetupWallet({browser,generate:async()=>phrase,derive:async()=>({address,dispose(){}}),...extra});
 return {wallet,browser,storage};
}
async function backup(wallet,protocol=hyper){
 assert.equal(await wallet.begin(protocol),true);
 assert.equal(wallet.challenge(),true);
 const s=wallet.snapshot();
 assert.notEqual(s.indexA,s.indexB);
 assert.equal(wallet.verify(phrase.split(' ')[s.indexA],phrase.split(' ')[s.indexB]),true);
}
test('new wallet displays only during backup; snapshot never contains words or voucher',async()=>{
 const {wallet,storage}=fixture();
 assert.equal(await wallet.begin(hyper),true);
 assert.equal(wallet.displayWords(words=>assert.equal(words,phrase)),true);
 assert.equal(wallet.prepare(hyper),false);
 assert.equal(wallet.challenge(),true);
 assert.equal(wallet.displayWords(()=>assert.fail()),false);
 assert.equal(wallet.verify('wrong','wrong'),false);
 const s=wallet.snapshot();
 assert.equal(wallet.verify(phrase.split(' ')[s.indexA],phrase.split(' ')[s.indexB]),true);
 assert.equal(wallet.displayWords(()=>assert.fail()),false);
 assert.doesNotMatch(JSON.stringify(wallet.snapshot()),/abandon|art art/);
 assert.equal(storage.size,0);
 assert.equal(wallet.prepare(far),false);
});
test('cancel pre-POST invalidates work; submission marker blocks cancellation and duplicate attempts',async()=>{
 const {wallet,storage}=fixture();
 await backup(wallet);
 assert.equal(wallet.prepare(hyper),true);
 let release;const wait=new Promise(r=>release=r);
 const pending=wallet.submit(hyper,null,async(_,options)=>{options.onProgress(1,10,'proof');await wait;options.onProgress(0,1,'submitting');assert.fail('POST after cancel');});
 assert.equal(wallet.cancel(),true);
 release();await assert.rejects(pending,/cancelled/i);
 assert.equal(storage.size,0);
 await backup(wallet);
 assert.equal(wallet.prepare(hyper),true);
 await assert.rejects(wallet.submit(hyper,null,async(_,options)=>{
  options.onProgress(0,1,'submitting');
  assert.equal(wallet.cancel(),false);
  assert.equal(wallet.prepare(hyper),false);
  throw Object.assign(new Error('timeout'),{outcomeUnknown:true});
 }));
 assert.equal(wallet.snapshot().locked,true);
 assert.equal(storage.size,1);
 assert.equal(wallet.leave(),true);
 assert.equal(await wallet.begin(hyper,phrase),true);
 assert.equal(wallet.snapshot().resumeOnly,true);
 assert.equal(wallet.prepare(hyper),true);
 await wallet.submit(hyper,null,async(_,o)=>{assert.equal(o.resumeOnly,true);return {fid:'4294967296',address};});
 assert.equal(storage.size,0);
 assert.equal(wallet.snapshot().phase,'confirmed');
});
test('paid registration requires voucher, explicit reservation, then a separate transfer',async()=>{
 let reserved=0,submitted=0;
 const sponsor={reserve:async(_,o)=>{reserved++;o.onProgress(0,1,'reserve');return{fid:'12345'};}};
 const {wallet,storage}=fixture({createSponsor:()=>sponsor});
 await backup(wallet,far);
 assert.equal(wallet.prepare(far,'','alice'),false);
 assert.equal(wallet.acceptVoucher('short'),false);
 const voucher='v'.repeat(64);
 assert.equal(wallet.acceptVoucher(voucher),true);
 assert.equal(await wallet.reserveSponsored(),true);
 assert.equal(reserved,1);
 assert.equal(wallet.cancel(),false);
 assert.equal(storage.size,1);
 assert.equal(wallet.snapshot().paidPhase,'reserved');
 assert.equal(wallet.prepare(far,'','alice'),true);
 await wallet.submit(far,null,async(_,o)=>{
  assert.equal(o.sponsoredRegistration,sponsor);assert.equal(o.voucher,voucher);
  assert.equal(o.username,'alice');submitted++;
  o.onProgress(0,1,'submit');return{fid:'12345',address};
 });
 assert.equal(submitted,1);assert.equal(reserved,1);
 assert.equal(storage.size,0);
 assert.equal(wallet.snapshot().paidPhase,'confirmed');
});
test('failed backup challenge clears after five attempts and blocked clipboard is not success',async()=>{
 const {wallet,browser}=fixture();
 browser.navigator.clipboard.writeText=async()=>{throw Error('denied');};
 await wallet.begin(hyper);
 wallet.copy();await Promise.resolve();await Promise.resolve();
 assert.equal(wallet.snapshot().copied,false);
 assert.equal(wallet.challenge(),true);
 for(let i=0;i<5;i++)assert.equal(wallet.verify('wrong','wrong'),false);
 assert.equal(wallet.snapshot().hasWallet,false);
 assert.equal(wallet.displayWords(()=>assert.fail()),false);
});
test('missing persistent public submission guard fails closed',async()=>{
 const {wallet,browser}=fixture();delete browser.localStorage;
 await backup(wallet);wallet.prepare(hyper);
 await assert.rejects(wallet.submit(hyper,null,async(_,o)=>{o.onProgress(0,1,'submitting');assert.fail('must not POST');}),/journal/);
});
