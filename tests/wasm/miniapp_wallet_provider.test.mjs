import test from 'node:test';
import assert from 'node:assert/strict';
import {createMiniAppEthereumProvider,createDiscoveredBrowserWallet,walletApprovalCopy,mobileWalletBrowserLinks,requestMiniAppWalletApproval} from '../../ui/wasm/miniapp-wallet-provider.mjs';
import {readFileSync} from 'node:fs';

function fakeWallet(){
  const handlers=new Map();
  const calls=[];
  const provider={
    async request(payload){
      calls.push(payload.method);
      if(payload.method==='eth_chainId')return '0x2105';
      if(payload.method==='eth_requestAccounts')return ['0xAa00000000000000000000000000000000000001'];
      if(payload.method==='eth_accounts')return ['0xAa00000000000000000000000000000000000001'];
      if(payload.method==='personal_sign')return '0xsigned';
      return null;
    },
    on(event,fn){if(!handlers.has(event))handlers.set(event,new Set());handlers.get(event).add(fn);},
    removeListener(event,fn){handlers.get(event)?.delete(fn);},
    emit(event,...args){for(const handler of handlers.get(event)||[])handler(...args);}
  };
  return {provider,calls,handlers};
}

test('FID context is not wallet authorization and connected accounts start private',async()=>{
  const {provider,calls}=fakeWallet();
  let approvals=0;
  const app=createMiniAppEthereumProvider('https://tasks.miniapps.genyleap.com',
    provider,()=>{approvals++;return true;},undefined,()=>true);
  assert.deepEqual(await app.provider.request({method:'eth_accounts'}),[]);
  assert.equal(calls.length,0);
  assert.deepEqual(await app.chains(),['eip155:8453']);
  assert.equal(approvals,0,'read-only chain discovery must not trigger consent');
  await assert.rejects(
    app.provider.request({method:'personal_sign',params:['hello']}),
    e=>e.code===4100);
  assert.equal(calls.includes('personal_sign'),false);
  const accounts=await app.provider.request({method:'eth_requestAccounts'});
  assert.equal(accounts.length,1);
  assert.equal(approvals,1);
  assert.deepEqual(await app.provider.request({method:'eth_accounts'}),accounts);
  assert.equal(await app.provider.request({method:'personal_sign',params:['hello']}),
    '0xsigned');
  assert.equal(calls.includes('personal_sign'),true);
  app.dispose();
  await assert.rejects(app.provider.request({method:'eth_accounts'}),e=>e.code===4900);
});

test('Denial never forwards wallet access to injected EIP-1193 provider',async()=>{
  const {provider,calls}=fakeWallet();
  const app=createMiniAppEthereumProvider('https://tasks.miniapps.genyleap.com',
    provider,async()=>false);
  await assert.rejects(app.provider.request({method:'eth_requestAccounts'}),
    e=>e.code===4001);
  assert.equal(calls.includes('eth_requestAccounts'),false);
  assert.deepEqual(await app.provider.request({method:'eth_accounts'}),[]);
  app.dispose();
});

test('Wallet access does not cross Mini App origins or last past iframe cleanup',async()=>{
  const {provider,handlers}=fakeWallet();
  const approved=createMiniAppEthereumProvider('https://app-a.example',provider,()=>true);
  const unapproved=createMiniAppEthereumProvider('https://app-b.example',provider,()=>false);
  await approved.provider.request({method:'eth_requestAccounts'});
  assert.deepEqual(await unapproved.provider.request({method:'eth_accounts'}),[]);
  let notified=0;
  approved.provider.on('accountsChanged',()=>notified++);
  unapproved.provider.on('accountsChanged',()=>notified+=100);
  provider.emit('accountsChanged',['0xabc']);
  assert.equal(notified,1);
  approved.dispose();
  provider.emit('accountsChanged',['0xdef']);
  assert.equal(notified,1);
  assert.ok([...handlers.values()].every(set=>set.size===1),
    'disposal must detach one session while keeping the other');
  unapproved.dispose();
  assert.ok([...handlers.values()].every(set=>set.size===0));
});

test('Invalid methods and unsupported wallets fail closed without fabricated signatures',async()=>{
  assert.equal(createMiniAppEthereumProvider('https://app.example',null),null);
  const {provider,calls}=fakeWallet();
  const app=createMiniAppEthereumProvider('https://app.example',provider,()=>true);
  for(const method of ['__proto__','eth.accounts','',null]) {
    await assert.rejects(app.provider.request({method}),e=>e.code===4200);
  }
  await assert.rejects(app.provider.request({method:'eth_getBlockByNumber'}),
    e=>e.code===4100);
  assert.equal(calls.length,0);
  app.dispose();
});

test('official SDK host exposes injected wallet only by explicit per-origin grant',()=>{
  const bridge=readFileSync(new URL('../../ui/wasm/miniapp-host-bridge-entry.mjs',import.meta.url),'utf8');
  assert.match(bridge,/createDiscoveredBrowserWallet\(window,\{/);
  assert.match(bridge,/createMiniAppEthereumProvider\(url\.origin, injected, undefined, url\.href\)/);
  assert.match(bridge,/ethProvider:wallet\?\.provider/);
  assert.match(bridge,/wallet\?\.dispose\(\)/);
  assert.match(bridge,/wallet\.getEthereumProvider/);
  assert.match(bridge,/signIn: unsupported/);
});

test('EIP-6963 registry lets the user choose among actual injected providers',async()=>{
  const first=fakeWallet();
  const second=fakeWallet();
  const win=new EventTarget();
  win.ethereum=first.provider;
  win.addEventListener('eip6963:requestProvider',()=>{
    const event=new Event('eip6963:announceProvider');
    Object.defineProperty(event,'detail',{value:{
      info:{name:'Rabby',uuid:'wallet-2'},provider:second.provider}});
    win.dispatchEvent(event);
  });
  const registry=createDiscoveredBrowserWallet(win);
  assert.equal(registry.available(),true);
  assert.equal(registry.choices().length,2);
  assert.equal(registry.choices()[0].name,'Rabby');
  const wallet=createMiniAppEthereumProvider('https://app.example',registry,
    (_origin,_signal,selection)=>{
      assert.equal(selection,registry);
      assert.equal(selection.select('wallet-0'),true);
      return true;
    });
  await wallet.provider.request({method:'eth_requestAccounts'});
  assert.deepEqual(second.calls,['eth_requestAccounts']);
  assert.deepEqual(first.calls,[]);
  wallet.dispose();
  registry.dispose();
  assert.equal(registry.available(),false);
});


test('wallet approval follows the active Persian and Arabic direction without changing origin',()=>{
  for(const lang of ['fa-IR','ar-SA']){
    const copy=walletApprovalCopy(lang);
    assert.equal(copy.direction,'rtl');
    assert.notEqual(copy.cancel,'Cancel');
    assert.notEqual(copy.title,'Connect wallet');
    assert.ok(copy.question.length>8);
  }
  assert.equal(walletApprovalCopy('en-US').direction,'ltr');
  assert.equal(walletApprovalCopy('zh-CN').direction,'ltr');
  assert.equal(walletApprovalCopy('xx').title,'Connect wallet');
  const source=readFileSync(new URL('../../ui/wasm/miniapp-wallet-provider.mjs',import.meta.url),'utf8');
  assert.match(source,/window\.__genycasterPreferredLanguage/);
  assert.match(source,/panel\.setAttribute\('dir',copy\.direction\)/);
  assert.match(source,/originText\.textContent=origin/);
  assert.match(source,/originText\.setAttribute\('dir','ltr'\)/);
  assert.match(source,/panel\.append\(title,originText,details\)/);
  assert.doesNotMatch(source,/WalletConnect.*enabled:\s*true/);
});

test('native Mini App toolbar titles take the remaining span instead of collapsing to ellipses',()=>{
  const native=readFileSync(new URL('../../ui/mobile/miniapp_browser_macos.mm',import.meta.url),'utf8');
  assert.match(native,/\[identity\.leadingAnchor constraintEqualToAnchor:m_back\.trailingAnchor constant:8\]/);
  assert.match(native,/\[identity\.trailingAnchor constraintEqualToAnchor:reload\.leadingAnchor constant:-8\]/);
  assert.doesNotMatch(native,/identity\.centerXAnchor constraintEqualToAnchor:toolbar\.centerXAnchor/);
});

test('wallet chooser uses translated IDs in all locales and direction-aware controls',()=>{
  const source=readFileSync(new URL('../../ui/components/WalletBrowserPicker.qml',import.meta.url),'utf8');
  const ids=['browser_title','default_browser','previous_browser','last_used',
    'approval_hint','cancel','browser_open_failed'];
  for(const id of ids){
    const full='genycaster.wallet.'+id;
    for(const locale of ['en_US','fa_IR','ar_SA','de_DE','ja_JP','ko_KR',
      'zh_CN','hi_IN','es_ES','fr_FR','pt_BR','ru_RU']){
      const catalog=readFileSync(new URL('../../ui/i18n/qml_'+locale+'.ts',import.meta.url),'utf8');
      assert.ok(catalog.includes('<message id="'+full+'">'),locale+' missing '+id);
    }
    if(id!=='browser_open_failed')assert.ok(source.includes(full));
  }
  assert.match(source,/layoutDirection: Theme\.uiDirection/);
  assert.match(source,/Theme\.rtl \? Theme\.iconArrowLeft : Theme\.iconArrowRight/);
  assert.match(source,/forceLeftToRight: true/);
});


test('mobile wallet-browser links use original HTTPS Mini App URL, never localhost',()=>{
 const source='https://universal-caster-income.vercel.app/claim?ref=GenyCaster';
 assert.deepEqual(mobileWalletBrowserLinks(source,'Mozilla/5.0 Desktop'),[]);
 const links=mobileWalletBrowserLinks(source,'Mozilla/5.0 (iPhone; CPU iPhone OS 18_0)');
 assert.equal(links.length,2);
 const meta=links.find(link=>link.id==='metamask');
 const coinbase=links.find(link=>link.id==='coinbase');
 assert.equal(new URL(meta.href).hostname,'link.metamask.io');
 assert.equal(new URL(coinbase.href).hostname,'go.cb-w.com');
 assert.equal(new URL(coinbase.href).searchParams.get('cb_url'),source);
 for(const bad of ['http://127.0.0.1:64141/session/test',
    'https://localhost/session/test','https://192.168.1.10',
    'javascript:alert(1)','https://example.com:8443/x',
    'https://username:pass@example.com','not-an-url']){
   assert.deepEqual(mobileWalletBrowserLinks(bad,'iPhone'),[]);
 }
 const copy=walletApprovalCopy('fa-IR');
 assert.equal(copy.direction,'rtl');
 assert.match(copy.externalExplanation,/جنی‌کستر/);
});

test('WalletConnect is explicitly offered, and missing providers fail closed',()=>{
 const source=readFileSync(new URL('../../ui/wasm/miniapp-wallet-provider.mjs',import.meta.url),'utf8');
 assert.match(source,/if\(!choices\.length\)/);
 assert.match(source,/approve\.disabled=true/);
 assert.match(source,/id==='walletconnect'/);
 assert.match(source,/injected\.connectWalletConnect\(\{\s*onUri,onStatus:updateStatus,signal:pairAbort\.signal\s*\}\)/);
 assert.match(source,/uri\.length>4096/);
 assert.doesNotMatch(source,/new WalletConnect\(/);
});


test('WalletConnect is offered without injected extensions and returns real signer results to its origin',async()=>{
 const win=new EventTarget();
 const seen=[];
 let connected=0;
 const handlers=new Map();
 const remote={
   session:{topic:'approved-pairing'},
   async request({method}){
     seen.push(method);
     if(method==='eth_requestAccounts'||method==='eth_accounts')
       return ['0x0000000000000000000000000000000000000001'];
     if(method==='eth_chainId')return '0xa';
     if(method==='personal_sign')return '0xsigned-from-wallet';
     return null;
   },
   on(event,cb){if(!handlers.has(event))handlers.set(event,new Set());handlers.get(event).add(cb);},
   removeListener(event,cb){handlers.get(event)?.delete(cb);}
 };
 const registry=createDiscoveredBrowserWallet(win,{
   walletConnectChains:['eip155:10','eip155:8453'],
   walletConnectFactory:async({onUri,signal})=>{
     connected++;
     assert.equal(signal.aborted,false);
     onUri('wc:topic@2?relay-protocol=irn&symKey=placeholder');
     return remote;
   }
 });
 assert.equal(registry.available(),true);
 assert.deepEqual(registry.choices(),[{id:'walletconnect',name:'WalletConnect'}]);
 assert.deepEqual(registry.supportedChains(),['eip155:10','eip155:8453']);
 const origins=[];
 const wallet=createMiniAppEthereumProvider('https://mini.example',registry,
   async(origin,signal,injected)=>{
     origins.push(origin);
     assert.equal(injected.select('walletconnect'),true);
     await injected.connectWalletConnect({signal,onUri:()=>{}});
     return true;
   },undefined,()=>true);
 assert.deepEqual(await wallet.provider.request({method:'eth_accounts'}),[]);
 assert.deepEqual(await wallet.provider.request({method:'eth_requestAccounts'}),
   ['0x0000000000000000000000000000000000000001']);
 assert.equal(await wallet.provider.request({method:'personal_sign',params:['0x1234']}),
   '0xsigned-from-wallet');
 assert.deepEqual(origins,['https://mini.example']);
 assert.equal(connected,1);
 assert.deepEqual(seen,['eth_requestAccounts','personal_sign']);
 wallet.dispose();
 registry.dispose();
 assert.ok([...handlers.values()].every(set=>set.size===0));
});

test('WalletConnect origin grant cannot be reused by another Mini App or after teardown',async()=>{
 const win=new EventTarget();
 const provider={session:{topic:'existing'},request:async({method})=>
   method==='eth_requestAccounts'?['0x0000000000000000000000000000000000000002']:'0xsigned',
   on(){},removeListener(){}};
 const registry=createDiscoveredBrowserWallet(win,{
   walletConnectFactory:async()=>provider
 });
 const approve=async(_origin,signal,injected)=>{
   injected.select('walletconnect');
   await injected.connectWalletConnect({signal});
   return true;
 };
 const first=createMiniAppEthereumProvider('https://a.example',registry,approve);
 const second=createMiniAppEthereumProvider('https://b.example',registry,async()=>false);
 await first.provider.request({method:'eth_requestAccounts'});
 await assert.rejects(second.provider.request({method:'personal_sign',params:[]}),
   error=>error.code===4100);
 await assert.rejects(second.provider.request({method:'eth_requestAccounts'}),
   error=>error.code===4001);
 first.dispose();
 await assert.rejects(first.provider.request({method:'eth_accounts'}),
   error=>error.code===4900);
 second.dispose();
 registry.dispose();
});

test('WalletConnect module is self-hosted, loaded lazily and does not overwrite the Farcaster custody signer',()=>{
 const host=readFileSync(new URL('../../ui/wasm/miniapp-host-bridge-entry.mjs',import.meta.url),'utf8');
 const build=readFileSync(new URL('../../ui/wasm/host-sdk/build.mjs',import.meta.url),'utf8');
 const driver=readFileSync(new URL('../../ui/wasm/host-sdk/walletconnect-entry.mjs',import.meta.url),'utf8');
 assert.match(host,/walletConnectFactory: async/);
 assert.match(host,/import\('\.\/genycaster-walletconnect\.mjs'\)/);
 assert.match(build,/external:\['\.\/genycaster-walletconnect\.mjs'\]/);
 assert.match(driver,/showQrModal:false/);
 assert.match(driver,/provider\.on\('display_uri'/);
 assert.match(driver,/provider\.session/);
 assert.doesNotMatch(driver,/privateKey|mnemonic|signerSecret/);
});


test('account switching or disconnecting revokes a previously granted wallet',async()=>{
 const events=new Map();
 const provider={
   async request({method}){
     if(method==='eth_requestAccounts')
       return ['0x0000000000000000000000000000000000000003'];
     if(method==='personal_sign')return '0xreal-wallet-result';
     return ['0x0000000000000000000000000000000000000003'];
   },
   on(event,cb){if(!events.has(event))events.set(event,new Set());events.get(event).add(cb);},
   removeListener(event,cb){events.get(event)?.delete(cb);},
   fire(event,value){for(const fn of events.get(event)||[])fn(value);}
 };
 const wallet=createMiniAppEthereumProvider('https://mini.example',provider,()=>true,undefined,()=>true);
 await wallet.provider.request({method:'eth_requestAccounts'});
 provider.fire('accountsChanged',['0x0000000000000000000000000000000000000003']);
 assert.equal(await wallet.provider.request({method:'personal_sign'}),
   '0xreal-wallet-result');
 provider.fire('accountsChanged',['0x0000000000000000000000000000000000000004']);
 await assert.rejects(wallet.provider.request({method:'personal_sign'}),
   e=>e.code===4100);
 await wallet.provider.request({method:'eth_requestAccounts'});
 provider.fire('disconnect',{code:4900});
 await assert.rejects(wallet.provider.request({method:'personal_sign'}),
   e=>e.code===4100);
 wallet.dispose();
});


test('host WalletConnect prompt stays open for QR pairing and resolves only after session approval',async()=>{
  const previousDoc=Object.getOwnPropertyDescriptor(globalThis,'document');
  const previousWindow=Object.getOwnPropertyDescriptor(globalThis,'window');
  class Element {
    constructor(tag){
      this.tag=tag;this.children=[];this.style={};this.listeners={};this.attrs={};
      this.textContent='';this.hidden=false;this.disabled=false;this.src='';
    }
    setAttribute(key,value){this.attrs[key]=value;}
    append(...children){this.children.push(...children);}
    replaceChildren(...children){this.children=[...children];}
    addEventListener(name,callback){this.listeners[name]=callback;}
    removeEventListener(name){delete this.listeners[name];}
    remove(){this.removed=true;}
    focus(){}
  }
  const body=new Element('body');
  const document={body,documentElement:{lang:'fa-IR'},
    createElement:tag=>new Element(tag)};
  const window={__genycasterPreferredLanguage:'fa-IR'};
  Object.defineProperty(globalThis,'document',{configurable:true,value:document});
  Object.defineProperty(globalThis,'window',{configurable:true,value:window});
  try{
    let finishPairing;
    let pairUri=false;
    const provider={session:{topic:'test'},request:async()=>[],
      on(){},removeListener(){}};
    const registry=createDiscoveredBrowserWallet(new EventTarget(),{
      walletConnectFactory:async({onUri})=>{
        onUri({uri:'wc:123456ab@2?relay-protocol=irn&symKey=example',
          qr:'data:image/png;base64,AA=='});
        return new Promise(resolve=>{finishPairing=()=>resolve(provider);});
      }
    });
    const approval=requestMiniAppWalletApproval('https://mini.example',new AbortController().signal,registry);
    const panel=body.children[0].children[0];
    assert.equal(panel.attrs.dir,'rtl');
    const actions=panel.children.find(c=>c.tag==='div' &&
      c.children.some(child=>child.tag==='button'));
    assert.ok(actions);
    const approve=actions.children.find(e=>e.tag==='button' &&
      e.textContent.includes('WalletConnect'));
    assert.ok(approve);
    const qr=panel.children.find(e=>e.tag==='section' && e.hidden);
    assert.ok(qr);
    const click=approve.listeners.click();
    // Pairing stays pending until the real wallet accepts the session.
    for(let i=0;i<6;i++)await Promise.resolve();
    assert.equal(qr.hidden,false);
    assert.equal(qr.children[1].src,'data:image/png;base64,AA==');
    assert.equal(approve.disabled,true);
    // Merely creating the pairing URI never grants the Mini App access.
    finishPairing();
    await click;
    assert.equal(await approval,true);
    assert.equal(body.children[0].removed,true);
    registry.dispose();
  } finally {
    if(previousDoc)Object.defineProperty(globalThis,'document',previousDoc);
    else delete globalThis.document;
    if(previousWindow)Object.defineProperty(globalThis,'window',previousWindow);
    else delete globalThis.window;
  }
});


test('WalletConnect relay and initialization deadlines are finite and abortable',async()=>{
 const source=readFileSync(new URL('../../ui/wasm/host-sdk/walletconnect-entry.mjs',import.meta.url),'utf8');
 const first=source.indexOf('export function withWalletDeadline(');
 const last=source.indexOf('const initialize=',first);
 assert.ok(first>=0 && last>first);
 const helper=source.slice(first,last).replace('export function ','function ');
 const bounded=new Function(helper+'\nreturn withWalletDeadline;')();
 const never=new Promise(()=>{});
 await assert.rejects(
   bounded(never,4,undefined,'Pairing URI not available'),
   e=>e.code===4900 && e.message==='Pairing URI not available');
 const controller=new AbortController();
 const pending=bounded(never,1000,controller.signal,'Network timed out');
 controller.abort();
 await assert.rejects(pending,e=>e.code===4001);
 assert.match(source,/PAIRING_URI_TIMEOUT_MS=20000/);
 assert.match(source,/INITIALIZE_TIMEOUT_MS=20000/);
 assert.match(source,/WALLET_APPROVAL_TIMEOUT_MS=120000/);
 assert.match(source,/provider\.on\('display_uri',onDisplayUri\)/);
 assert.match(source,/provider\.signer\?\.on\?\.\('display_uri',onDisplayUri\)/);
});

test('mobile wallet handoff is not blocked by asynchronous QR creation',()=>{
 const source=readFileSync(new URL('../../ui/wasm/miniapp-wallet-provider.mjs',import.meta.url),'utf8');
 const handler=source.slice(source.indexOf('const onUri=data=>{'),
   source.indexOf('      try{\n        await injected.connectWalletConnect',source.indexOf('const onUri=data=>{')));
 assert.ok(handler.includes('walletLinks.append(anchor)'));
 assert.ok(handler.includes("import('./genycaster-walletconnect.mjs')"));
 assert.ok(handler.indexOf('walletLinks.append(anchor)')<
   handler.indexOf("import('./genycaster-walletconnect.mjs')"),
   'mobile wallet links must be offered before awaiting QR generation');
 assert.ok(handler.includes('qrContainer.hidden=false'));
 assert.ok(source.includes("stage==='creating'?promptText.creating"));
});


test('every sensitive request receives a separate host confirmation and can be denied',async()=>{
 const {provider,calls}=fakeWallet();
 const requests=[];
 const app=createMiniAppEthereumProvider('https://one.example',provider,
   ()=>true,undefined,async(origin,method)=>{
     requests.push({origin,method});
     return requests.length!==2;
   });
 await app.provider.request({method:'eth_requestAccounts'});
 assert.equal(await app.provider.request({method:'personal_sign',params:['0x12']}),'0xsigned');
 await assert.rejects(app.provider.request({method:'personal_sign',params:['0x13']}),
   e=>e.code===4001);
 assert.equal(await app.provider.request({method:'personal_sign',params:['0x14']}),'0xsigned');
 assert.deepEqual(requests.map(r=>r.method),
   ['personal_sign','personal_sign','personal_sign']);
 assert.ok(requests.every(r=>r.origin==='https://one.example'));
 assert.deepEqual(calls,['eth_requestAccounts','personal_sign','personal_sign']);
 for(const method of ['eth_sign','eth_sendRawTransaction','wallet_unknownAction']){
   await assert.rejects(app.provider.request({method,params:[]}),e=>e.code===4200);
 }
 app.dispose();
});

test('changing accounts revokes grant without leaking the new address in events',async()=>{
 const {provider}=fakeWallet();
 const notifications=[];
 const app=createMiniAppEthereumProvider('https://one.example',provider,
   ()=>true,undefined,()=>true);
 await app.provider.request({method:'eth_requestAccounts'});
 app.provider.on('accountsChanged',value=>notifications.push(value));
 provider.emit('accountsChanged',['0x9999999999999999999999999999999999999999']);
 assert.deepEqual(notifications,[[]]);
 assert.deepEqual(await app.provider.request({method:'eth_accounts'}),[]);
 await assert.rejects(app.provider.request({method:'personal_sign',params:['0x12']}),
   e=>e.code===4100);
 app.dispose();
});

test('reopening a Mini App never inherits its previous iframe grant',async()=>{
 const {provider}=fakeWallet();
 const first=createMiniAppEthereumProvider('https://same.example',provider,()=>true);
 await first.provider.request({method:'eth_requestAccounts'});
 first.dispose();
 const second=createMiniAppEthereumProvider('https://same.example',provider,()=>false);
 assert.deepEqual(await second.provider.request({method:'eth_accounts'}),[]);
 await assert.rejects(second.provider.request({method:'eth_requestAccounts'}),
   e=>e.code===4001);
 second.dispose();
});

test('web startup gate waits for terminal browser account restore, including signed-out',()=>{
 const signer=readFileSync(new URL('../../ui/wasm/protocol-signer-pre.js',import.meta.url),'utf8');
 const boot=readFileSync(new URL('../../ui/wasm/genycaster-loader.js',import.meta.url),'utf8');
 const bridge=readFileSync(new URL('../../src/presentation/qt/presentation_bridge.cpp',import.meta.url),'utf8');
 const native=readFileSync(new URL('../../ui/mobile/browser_signer_bridge.hpp',import.meta.url),'utf8');
 assert.match(signer,/if\(signedOutLocally\(\)\)\{[\s\S]*?phase:'cancelled'/);
 assert.match(signer,/phase:account&&!owner\?'ready':'cancelled'/);
 assert.match(boot,/!accountRestoreSettled/);
 assert.match(bridge,/m_authenticationStateObserved && !m_browserAccountRestorePending/);
 assert.match(native,/presentation->finishBrowserAccountRestore\(\)/);
});

test('web Mini App loading protects iframe from the blank white first paint',()=>{
 const source=readFileSync(new URL('../../ui/wasm/miniapp-host-pre.js',import.meta.url),'utf8');
 assert.match(source,/background:surfaceColors\(\)\.bg,border:"0"/);
 assert.match(source,/visibility:"hidden"/);
 assert.match(source,/function makeLoadingOverlay\(\)/);
 assert.match(source,/function registerFrameLoad\(session\)/);
 assert.match(source,/clearTimeout\(session\.failureTimer\)/);
 assert.match(source,/session\.frame\.addEventListener\('load'/);
 assert.match(source,/previousFid>0 && previousFid!==nextFid/);
});


test('silent account rotation is rejected even without an accountsChanged event',async()=>{
 let active='0x000000000000000000000000000000000000000a';
 let signCalls=0;
 const provider={
   async request({method}){
     if(method==='eth_requestAccounts'||method==='eth_accounts')return [active];
     if(method==='personal_sign'){signCalls++;return '0xsigned';}
     return '0xa';
   },
   on(){},removeListener(){}
 };
 const app=createMiniAppEthereumProvider('https://mini.example',provider,()=>true,
   undefined,()=>true);
 await app.provider.request({method:'eth_requestAccounts'});
 active='0x000000000000000000000000000000000000000b';
 assert.deepEqual(await app.provider.request({method:'eth_accounts'}),[]);
 await assert.rejects(app.provider.request({method:'personal_sign',params:['0x12']}),
   e=>e.code===4100);
 assert.equal(signCalls,0);
 await app.provider.request({method:'eth_requestAccounts'});
 assert.equal(await app.provider.request({method:'personal_sign',params:['0x12']}),
   '0xsigned');
 assert.equal(signCalls,1);
 app.dispose();
});
