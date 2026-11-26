// QML onboarding port. The private phrase and voucher are lexical,
// ephemeral state. Only public metadata is reported to the Qt controller.
import {generateNativePhrase} from './hypersnap_native_account.mjs';
import {deriveFarcasterCustody} from './farcaster_recovery.mjs';
import {createBrowserFarcasterRegistration} from './farcaster_new_account.mjs';

const initial=()=>({hasWallet:false,revealed:false,verified:false,busy:false,
 locked:false,indexA:-1,indexB:-1,address:'',phase:'',done:0,total:0,error:'',
 requestAccepted:false,paidPhase:'idle',hasVoucher:false,reservedFid:'',copied:false,resumeOnly:false,restored:false});
const validProtocol=p=>['hypersnap-v2','farcaster-snapchain'].includes(p);
export function createSetupWallet({browser=globalThis,generate=generateNativePhrase,
 derive=deriveFarcasterCustody,preflight=async()=>{},
 createSponsor=()=>createBrowserFarcasterRegistration({browser})}={}){
 let phrase='',voucher='',generation=0,request=null,protocol='',failures=0,sponsor=null;
 let state=initial();
 const patch=value=>{state={...state,...value};};
 const snapshot=()=>({...state});
 const markerKey=()=> 'genycaster.registration.pending.'+protocol+'.'+state.address.toLowerCase();
 function markPending(){
  // Public custody address only. This journal is a conservative duplicate
  // submission guard, never identity evidence. No phrase, voucher or key.
  const storage=browser.localStorage;
  if(!storage)throw Error('Persistent registration safety journal unavailable');
  storage.setItem(markerKey(),'1');
  if(storage.getItem(markerKey())!=='1')throw Error('Registration safety journal unavailable');
 }
 function clearMarker(){try{browser.localStorage?.removeItem(markerKey());}catch{}}
 function hasMarker(){try{return browser.localStorage?.getItem(markerKey())==='1';}catch{return true;}}
 const clear=()=>{generation++;phrase='';voucher='';request=null;protocol='';failures=0;sponsor=null;state=initial();};
 const discard=()=>{if(state.busy||state.locked&&state.phase!=='confirmed')return false;clear();return true;};
 async function begin(p,recovery=''){
  if(browser.isSecureContext!==true||!validProtocol(p)||state.hasWallet||state.busy||state.locked)return false;
  const g=++generation;protocol=p;patch({busy:true,phase:'preflight',error:''});
  let secret='',wallet;
  try{
   if(recovery&&recovery.trim().split(/\s+/).length!==24)throw Error('New-account backup requires 24 words');
   await preflight(p);
   if(g!==generation)return false;
   secret=recovery||await generate(browser);
   wallet=await derive(secret);
   if(g!==generation)return false;
   if(!/^0x[0-9a-f]{40}$/i.test(wallet.address))throw Error('Invalid custody address');
   phrase=secret;patch({hasWallet:true,revealed:!recovery,verified:!!recovery,
    restored:!!recovery,busy:false,phase:'',address:wallet.address});
   patch({resumeOnly:hasMarker()});
   return true;
  }catch{if(g===generation)patch({busy:false,phase:'',error:'wallet_unavailable'});return false;}
  finally{secret='';recovery='';wallet?.dispose();}
 }
 function challenge(){
  if(!state.hasWallet||state.verified||state.busy||!state.revealed)return false;
  const random=new Uint32Array(2);browser.crypto.getRandomValues(random);
  const a=random[0]%24;let b=random[1]%23;if(b>=a)b++;
  patch({revealed:false,indexA:a,indexB:b,error:'',copied:false});return true;
 }
 function verify(a,b){
  if(!state.hasWallet||state.verified||state.revealed||state.indexA<0||state.busy)return false;
  const words=phrase.split(' ');
  const ok=words[state.indexA]===String(a).trim().toLowerCase()&&words[state.indexB]===String(b).trim().toLowerCase();
  words.fill('');
  if(!ok){if(++failures>=5){clear();patch({error:'backup_failed'});}else patch({error:'backup_mismatch'});return false;}
  patch({verified:true,revealed:false,indexA:-1,indexB:-1,error:''});return true;
 }
 function reveal(){if(state.hasWallet&&!state.verified&&!state.busy)patch({revealed:true,error:''});}
 function displayWords(write){if(!state.revealed||state.verified||state.busy||!phrase)return false;write(phrase);return true;}
 function copy(){
  if(!state.revealed||state.verified||state.busy||!phrase||!browser.navigator?.clipboard?.writeText)return false;
  // Clipboard reads/clear are permission-gated in browsers. Never claim
  // successful copy or automatic clearing before the browser reports it.
  const g=generation;
  browser.navigator.clipboard.writeText(phrase).then(()=>{if(g===generation)patch({copied:true,error:''});})
    .catch(()=>{if(g===generation)patch({error:'clipboard_denied'});});return true;
 }
 function acceptVoucher(value){
  if(protocol!=='farcaster-snapchain'||!state.verified||state.busy||state.locked||state.resumeOnly
     ||typeof value!=='string'||value.length<60||value.length>1100||/[\r\n]/.test(value))return false;
  voucher=value;patch({paidPhase:'ready',hasVoucher:true,error:''});return true;
 }
 async function reserveSponsored(){
  if(protocol!=='farcaster-snapchain'||state.paidPhase!=='ready'||!voucher||!state.verified
     ||state.busy||state.locked||state.resumeOnly||!phrase)return false;
  const g=generation;patch({busy:true,paidPhase:'reserving',phase:'network',error:''});
  try{
   sponsor=createSponsor();
   const reserved=await sponsor.reserve(phrase,{voucher,confirmIrreversible:true,
    onProgress:(_,__,phase)=>{if(g!==generation)throw Error('Registration cancelled');
     if(phase==='reserve'){markPending();patch({locked:true,phase});}}});
   if(g!==generation)throw Error('Registration cancelled');
   patch({busy:false,locked:true,paidPhase:'reserved',reservedFid:reserved.fid,phase:'reserved'});return true;
  }catch(error){
   if(g===generation){const locked=state.locked||error?.outcomeUnknown===true;
    if(locked){phrase='';voucher='';sponsor=null;}
    patch({busy:false,locked,paidPhase:locked?'pending':'ready',phase:locked?'outcome-unknown':'failed',error:locked?'registration_pending':'registration_failed'});}
   return false;
  }
 }
 function prepare(p,_unused='',username=''){
  const paid=p==='farcaster-snapchain';
  if(p!==protocol||!state.verified||state.busy||!phrase||request)return false;
  if(paid){
   if(state.paidPhase!=='reserved'||!sponsor||!voucher||!state.locked||! /^[a-z0-9][a-z0-9-]{0,15}$/.test(username))return false;
  }else if(state.locked)return false;
  request={username};patch({busy:true,phase:'network',done:0,total:0,error:'',requestAccepted:false});return true;
 }
 async function submit(p,signal,operation){
  const paid=p==='farcaster-snapchain';
  if(p!==protocol||!request||!state.verified||!state.busy||!phrase
     ||(paid?state.paidPhase!=='reserved':state.locked))throw Error('Verified backup and explicit registration consent required');
  const options=request;request=null;const g=generation;
  try{
   const result=await operation(phrase,{...options,voucher,sponsoredRegistration:sponsor,
    signal,confirmIrreversible:true,resumeOnly:state.resumeOnly,restored:state.restored,address:state.address,
    onProgress:(done,total,phase)=>{
     if(g!==generation||signal?.aborted)throw Error('Registration cancelled');
     const irreversible=['reserve','submit','submitting','accepted','checking','outcome-unknown','verify'].includes(phase);
     if(irreversible&&!state.locked){markPending();}
     const locked=state.locked||irreversible;
     patch({phase,requestAccepted:state.requestAccepted||phase==='accepted',done:Number.isSafeInteger(done)?done:0,total:Number.isSafeInteger(total)?total:0,locked,
      paidPhase:paid&&phase!=='confirmed'?'pending':state.paidPhase});
   }});
   if(g!==generation)throw Error('Registration cancelled');
   clearMarker();phrase='';voucher='';sponsor=null;
   patch({busy:false,locked:true,phase:'confirmed',requestAccepted:true,paidPhase:paid?'confirmed':state.paidPhase,hasVoucher:false});return result;
  }catch(error){
   if(g===generation){
    const locked=error?.definitelyRejected!==true&&(state.locked||state.resumeOnly||error?.outcomeUnknown===true);
    if(error?.definitelyRejected===true)clearMarker();
    if(locked){phrase='';voucher='';sponsor=null;}
    patch({busy:false,locked,phase:locked?'outcome-unknown':'failed',
     paidPhase:paid?(locked?'pending':'ready'):state.paidPhase,
     error:locked?'registration_pending':'registration_failed'});
   }
   throw error;
  }finally{options.username='';}
 }
 function failPrepared(){
  if(!request)return;
  request=null;
  patch({busy:false,error:'registration_failed',phase:'failed'});
 }
 function cancel(){if(state.locked&&state.phase!=='confirmed')return false;clear();return true;}
 // Leave only after polling has stopped. Keep the public journal intact.
 // Returning through Existing-account recovery is read-only until verified.
 function leave(){if(state.busy||!state.locked||state.phase==='reserved')return false;clear();return true;}
 return Object.freeze({snapshot,begin,discard,challenge,verify,reveal,displayWords,copy,
  acceptVoucher,reserveSponsored,prepare,submit,failPrepared,cancel,leave});
}
