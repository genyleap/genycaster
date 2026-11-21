/**
 * Private browser onboarding screen for a newly generated 24-word custody
 * wallet. The seed is held in a lexical JS scope and cleared from DOM on
 * exit, never crossing the Qt/WASM interface or HTTP boundary.
 */
import {generateNativePhrase} from './hypersnap_native_account.mjs';

const translations={
 en:{
  title:'Back up your new wallet',subtitle:'This recovery phrase controls your new identity on the selected protocol.',
  warning:'Write down all 24 words in order, offline. Never share them. Browser storage is not a safe backup.',
  challenge:'Verify your saved recovery phrase',complete:'I saved my recovery phrase in a safe place',
  consent:'I authorize the one-time registration of this wallet on the selected network.',
  create:'Create account',cancel:'Cancel',copy:'Copy words',copied:'Words copied — clear your clipboard afterward',
  failed:'The selected recovery words do not match. Confirm your backup.',
  busy:'Preparing secure registration…',progress:'Searching for a registration proof',
  stopping:'Stopping the proof before network submission…',
  submitting:'Submitting registration to HyperSnap. Do not submit it again.',
  checking:'Submission sent. Checking for an independently confirmed FID…',
  working:'Creating your account',
  progressNote:'Proof search is running. This percentage is the hash-search budget, not network confirmation.',
  pending:'Network result is uncertain. Do not repeat registration. Recover this wallet with your saved words to check its FID.',
  voucher:'Paid or sponsored enrollment voucher',username:'Choose a Farcaster username'
 },
 fa:{
  title:'پشتیبان‌گیری از کیف پول جدید',subtitle:'واژه‌های بازیابی، مالکیت هویت جدید شما در پروتکل انتخاب‌شده را تعیین می‌کنند.',
  warning:'هر ۲۴ واژه را به‌ترتیب و در محیط امن یادداشت کنید. آن‌ها را در اختیار هیچ‌کس قرار ندهید. ذخیره‌سازی مرورگر نسخهٔ پشتیبان امنی نیست.',
  challenge:'واژه‌های بازیابی ذخیره‌شده را تأیید کنید',complete:'عبارت بازیابی را در محل امن ذخیره کرده‌ام',
  consent:'ثبت یک‌بارهٔ این کیف پول در شبکهٔ انتخاب‌شده را تأیید می‌کنم.',
  create:'ساخت حساب',cancel:'انصراف',copy:'کپی واژه‌ها',copied:'واژه‌ها کپی شدند؛ پس از استفاده کلیپ‌بورد را پاک کنید',
  failed:'واژه‌های انتخاب‌شده درست نیستند. نسخهٔ پشتیبان را بررسی کنید.',
  busy:'در حال آماده‌سازی ثبت‌نام امن…',progress:'در حال جستجوی اثبات ثبت‌نام',
  stopping:'در حال توقف محاسبه پیش از ارسال شبکه…',
  submitting:'ثبت‌نام به هایپراسنپ ارسال می‌شود؛ دوباره ارسال نکنید.',
  checking:'درخواست ارسال شد. در حال بررسی مستقل ثبت FID…',
  working:'در حال ایجاد حساب',
  progressNote:'درصد، میزان جستجوی اثبات را نشان می‌دهد، نه تأیید ثبت در شبکه.',
  pending:'نتیجهٔ شبکه نامشخص است. ثبت‌نام را تکرار نکنید. با واژه‌های ذخیره‌شده، وضعیت FID را بررسی کنید.',
  voucher:'کد معتبر ثبت‌نام پولی یا حمایت‌شده',username:'نام کاربری فارکستر'
 }
};
const titleFor=network=>network==='hypersnap-v2'?'HyperSnap':'Farcaster';
const clearNode=node=>{if(node){node.textContent='';node.replaceChildren();}};
export async function promptCreateBrowserWallet({
 browser=globalThis,signal=null,protocol='hypersnap-v2',
 onRegister,onProgress=()=>{},generatePhrase=generateNativePhrase
}={}) {
 if(signal?.aborted)throw new Error('Account creation cancelled');
 if(!['hypersnap-v2','farcaster-snapchain'].includes(protocol))
   throw new Error('Invalid browser registration protocol');
 if(browser.isSecureContext!==true||!browser.document?.body)
   throw new Error('Account creation requires HTTPS');
 if(typeof onRegister!=='function')throw new Error('Registration service is missing');
 const document=browser.document,existing=document.getElementById('genycaster-browser-new-wallet');
 if(existing)throw new Error('Wallet registration is already open');
 const language=(browser.__genycasterPreferredLanguage||browser.navigator?.language||'en').split(/[-_]/)[0];
 const lang=translations[language]?language:'en',t=translations[lang];
 const isRtl=['fa','ar'].includes(lang);
 const light=document.documentElement?.dataset?.theme==='light'
   ||(!document.documentElement?.dataset?.theme&&browser.matchMedia?.('(prefers-color-scheme:light)').matches);
 const colors=light
  ?{bg:'#fff',fg:'#17181c',muted:'#777b86',border:'#dedfe4',cell:'#f8f8fa'}
  :{bg:'#101116',fg:'#f4f6ff',muted:'#adb1bb',border:'#323640',cell:'#191c23'};
 let phrase=await generatePhrase(browser),settled=false,processing=false;
 if(signal?.aborted){phrase='';throw new Error('Account creation cancelled');}
 const words=phrase.split(' ');
 const indices=[];
 const rand=new Uint32Array(2);
 browser.crypto.getRandomValues(rand);
 indices.push(rand[0]%24);
 indices.push((rand[1]%23+indices[0]+1)%24);
 const layer=document.createElement('div'),form=document.createElement('section');
 layer.id='genycaster-browser-new-wallet';
 layer.style.cssText='position:fixed;inset:0;z-index:2147483647;background:'+colors.bg+';font:15px system-ui,sans-serif;color:'+colors.fg+';box-sizing:border-box;';
 form.dir=isRtl?'rtl':'ltr';form.setAttribute('role','dialog');form.setAttribute('aria-modal','true');
 form.setAttribute('aria-label',t.title);
 form.style.cssText='max-width:740px;margin:auto;box-sizing:border-box;height:100%;display:flex;flex-direction:column;padding:max(30px,env(safe-area-inset-top)) 24px max(24px,env(safe-area-inset-bottom));gap:14px;';
 const content=document.createElement('div');
 content.style.cssText='overflow-y:auto;min-height:0;flex:1;padding:0 2px;overscroll-behavior:contain;';
 const heading=document.createElement('h1');heading.textContent=t.title;
 heading.style.cssText='font-size:clamp(22px,4vw,32px);margin:18px 0 10px;font-weight:700;';
 const subtitle=document.createElement('p');
 subtitle.textContent=t.subtitle+' '+titleFor(protocol);
 subtitle.style.cssText='color:'+colors.muted+';line-height:1.6;margin:0 0 16px';
 const warning=document.createElement('p');warning.textContent=t.warning;
 warning.style.cssText='line-height:1.6;color:'+colors.muted+';margin:0 0 16px';
 const grid=document.createElement('div');
 grid.style.cssText='display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px;direction:ltr;'
 const progressPanel=document.createElement('section');
 progressPanel.hidden=true;
 progressPanel.style.cssText='display:none;margin:22px 0;padding:20px;border:1px solid '+colors.border+';border-radius:18px;background:'+colors.cell+';';
 const progressHeading=document.createElement('h2');progressHeading.textContent=t.working;
 progressHeading.style.cssText='font-size:19px;font-weight:700;margin:0 0 12px;color:'+colors.fg;
 const progressCaption=document.createElement('p');progressCaption.textContent=t.busy;
 progressCaption.style.cssText='font-size:15px;line-height:1.6;color:'+colors.muted+';margin:0 0 12px;overflow-wrap:anywhere;';
 const progressTrack=document.createElement('div');progressTrack.style.cssText='height:6px;background:'+colors.border+';border-radius:9px;overflow:hidden;';
 const progressFill=document.createElement('div');progressFill.style.cssText='height:100%;width:0%;border-radius:9px;background:#008bff;transition:width 150ms linear;';
 progressTrack.appendChild(progressFill);
 const progressNote=document.createElement('p');progressNote.textContent=t.progressNote;
 progressNote.style.cssText='font-size:12px;line-height:1.6;color:'+colors.muted+';margin:10px 0 0';
 progressPanel.append(progressHeading,progressCaption,progressTrack,progressNote);;
 const cells=[];
 words.forEach((word,i)=>{
  const cell=document.createElement('div');
  cell.style.cssText='display:flex;align-items:center;min-width:0;gap:12px;padding:11px;border:1px solid '+colors.border+';border-radius:12px;background:'+colors.cell;
  const index=document.createElement('span');index.textContent=String(i+1).padStart(2,'0');
  index.style.cssText='color:'+colors.muted+';font:600 12px ui-monospace,monospace;';
  const value=document.createElement('strong');value.textContent=word;value.style.cssText='font-weight:600';
  cell.append(index,value);grid.appendChild(cell);cells.push(cell);
 });
 const backupAction=document.createElement('button');backupAction.type='button';backupAction.textContent=t.copy;
 backupAction.style.cssText='margin:16px 0;padding:12px 16px;border:1px solid '+colors.border+';background:'+colors.cell+';color:'+colors.fg+';border-radius:12px;font-weight:600;';
 const challengeHeading=document.createElement('h2');challengeHeading.textContent=t.challenge;
 challengeHeading.style.cssText='font-size:17px;margin:14px 0 12px;font-weight:600';
 const challenge=document.createElement('div');
 challenge.style.cssText='display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px';
 const checks=[];
 indices.forEach(index=>{
  const label=document.createElement('label');
  label.textContent='#'+String(index+1).padStart(2,'0');
  label.style.cssText='display:flex;align-items:center;gap:10px;';
  const input=document.createElement('input');
  input.type='password';input.autocomplete='off';input.autocorrect='off';
  input.autocapitalize='off';input.spellcheck=false;
  input.setAttribute('aria-label',t.challenge+' '+(index+1));
  input.style.cssText='width:100%;min-width:0;padding:14px;border:1px solid '+colors.border+';border-radius:12px;background:'+colors.cell+';color:'+colors.fg+';font:16px system-ui';
  label.appendChild(input);challenge.appendChild(label);checks.push(input);
 });
 const confirmation=(message)=>{
  const label=document.createElement('label');
  label.style.cssText='display:flex;align-items:flex-start;gap:12px;margin:16px 0;line-height:1.6';
  const input=document.createElement('input');input.type='checkbox';
  input.style.cssText='width:22px;height:22px;flex-shrink:0;accent-color:'+colors.fg;
  const span=document.createElement('span');span.textContent=message;
  label.append(input,span);content.appendChild(label);
  return input;
 };
 const result=document.createElement('p');
 result.setAttribute('aria-live','polite');
 result.style.cssText='min-height:25px;line-height:1.6;color:'+(light?'#a4530c':'#ffbd71')+';overflow-wrap:anywhere';
 const buttons=document.createElement('div');
 buttons.style.cssText='display:flex;gap:12px;padding-top:15px;border-top:1px solid '+colors.border;
 const button=(text,primary)=>{
  const b=document.createElement('button');b.type='button';b.textContent=text;
  b.style.cssText='flex:1;min-height:50px;padding:12px;border-radius:14px;font:600 16px system-ui;background:'+
    (primary?colors.fg:colors.bg)+';color:'+(primary?colors.bg:colors.fg)+
    ';border:1px solid '+colors.border+';cursor:pointer;';
  buttons.appendChild(b);return b;
 };
 content.append(heading,subtitle,warning,grid,progressPanel,backupAction,challengeHeading,challenge);
 const saved=confirmation(t.complete),consent=confirmation(t.consent);
 const credentialFields={};
 if(protocol==='farcaster-snapchain'){
  for(const [field,label,limit,secret] of [
    ['voucher',t.voucher,1100,true],['username',t.username,16,false]]) {
    const container=document.createElement('label');
    container.textContent=label;
    container.style.cssText='display:flex;flex-direction:column;gap:7px;margin:12px 0;font-weight:600;';
    const input=document.createElement('input');
    input.type=secret?'password':'text';input.maxLength=limit;
    input.autocomplete='off';input.autocapitalize='off';input.spellcheck=false;
    input.style.cssText='width:100%;box-sizing:border-box;padding:12px;border-radius:12px;border:1px solid '+colors.border+';background:'+colors.cell+';color:'+colors.fg+';font:16px system-ui;direction:ltr;';
    container.appendChild(input);content.appendChild(container);
    credentialFields[field]=input;
  }
 }
 content.appendChild(result);
 const cancel=button(t.cancel,false),submit=button(t.create,true);
 form.append(content,buttons);layer.appendChild(form);
 const oldFocus=document.activeElement;
 const oldOverflow=document.body.style.overflow;
 document.body.style.overflow='hidden';document.body.appendChild(layer);
 let attempts=0;
 const operationController=new AbortController();
 const cancelOperation=()=>operationController.abort(new Error('Account creation cancelled'));
 signal?.addEventListener('abort',cancelOperation,{once:true});
 const wipe=()=>{
  for(const input of checks)input.value='';
  for(const input of Object.values(credentialFields))input.value='';
  words.fill('');
  for(const cell of cells)clearNode(cell);
  saved.checked=false;consent.checked=false;phrase='';
  if(layer.isConnected)layer.remove();
  document.body.style.overflow=oldOverflow;
  oldFocus?.focus?.();
 };
 return new Promise((resolve,reject)=>{
  const finish=(error,value)=>{
   if(settled)return;
   settled=true;signal?.removeEventListener('abort',abort);
   signal?.removeEventListener('abort',cancelOperation);
   wipe();if(error)reject(error);else resolve(value);
  };
  let submissionStarted=false;
  const abort=()=>{
   if(!processing) { finish(new Error('Account creation cancelled'));return; }
   if(submissionStarted||operationController.signal.aborted)return;
   progressCaption.textContent=t.stopping;
   cancel.disabled=true;
   cancelOperation(); // Stop only before the irreversible network POST.
  };
  signal?.addEventListener('abort',abort,{once:true});
  cancel.onclick=()=>abort();
  backupAction.onclick=async()=>{
   try{
    await browser.navigator.clipboard.writeText(phrase);
    result.textContent=t.copied;
   }catch{result.textContent=t.warning;}
  };
  submit.onclick=async()=>{
   if(processing||settled)return;
   const match=indices.every((index,i)=>checks[i].value.trim().toLowerCase()===words[index]);
   if(!match||!saved.checked||!consent.checked){
    attempts++;
    result.textContent=t.failed;
    if(attempts>=5)finish(new Error('Backup verification attempt limit reached'));
    return;
   }
   // Hide secret words and their now-empty 24 cards entirely. Replace the
   // backup form with a responsive progress panel; no mnemonic enters logs,
   // URLs, QML, or the proof worker. Cancellation stays available until POST.
   for(const cell of cells)clearNode(cell);
   for(const element of [grid,backupAction,challengeHeading,challenge,warning])
     element.style.display='none';
   saved.parentElement.style.display='none';
   consent.parentElement.style.display='none';
   for(const field of Object.values(credentialFields))field.parentElement.style.display='none';
   progressPanel.hidden=false;progressPanel.style.display='block';
   content.scrollTop=0;
   processing=true;submit.disabled=true;cancel.disabled=false;
   result.textContent='';
   const update=(done,total,phase)=>{
    if(settled)return;
    if(phase==='submitting'||phase==='checking'||phase==='outcome-unknown') {
      submissionStarted=true;
      cancel.disabled=true;
      progressCaption.textContent=phase==='submitting'?t.submitting:
        phase==='checking'?t.checking:t.pending;
      progressTrack.style.display='none';
      progressNote.style.display='none';
    }else{
      const percent=Math.max(0,Math.min(100,Math.floor(done/Math.max(1,total)*100)));
      progressCaption.textContent=t.progress+' · '+percent+'%';
      progressFill.style.width=percent+'%';
    }
    onProgress(done,total,phase);
   };
   try{
    const voucher=credentialFields.voucher?.value||'';
    const username=(credentialFields.username?.value||'').trim().toLowerCase();
    const verified=await onRegister(phrase,{signal:operationController.signal,onProgress:update,
      confirmIrreversible:true,voucher,username});
    credentialFields.voucher && (credentialFields.voucher.value='');
    finish(null,verified);
   }catch(error){
    if(error.outcomeUnknown){
      progressCaption.textContent=t.pending;
      result.textContent=t.pending;
      progressTrack.style.display='none';progressNote.style.display='none';
      submit.disabled=true;cancel.disabled=false;
      processing=false;cancel.onclick=()=>finish(error);
    }else{
      finish(error);
    }
   }
  };
 });
}
