// A per-Mini-App EIP-1193 facade. No accounts, signatures or transactions
// can cross the iframe boundary without an explicit host-origin grant.
const PUBLIC_METHODS = new Set(['eth_chainId','net_version','eth_blockNumber']);
// Public chain metadata can be read without requesting an account.
const READ_METHODS = new Set([
  'eth_call','eth_estimateGas','eth_getBalance','eth_getCode',
  'eth_getBlockByHash','eth_getBlockByNumber','eth_getLogs',
  'eth_getTransactionByHash','eth_getTransactionReceipt',
  'eth_getTransactionCount','eth_getStorageAt','eth_getProof',
  'eth_feeHistory','eth_gasPrice','eth_maxPriorityFeePerGas',
  'eth_getBlockTransactionCountByNumber','eth_getBlockTransactionCountByHash'
]);
// Only explicitly supported wallet methods cross the host boundary. Blind
// eth_sign and pre-signed raw transactions are deliberately not forwarded.
const SIGNING_METHODS = new Set([
  'personal_sign','eth_signTypedData','eth_signTypedData_v3',
  'eth_signTypedData_v4','eth_sendTransaction','eth_sendUserOperation',
  'wallet_switchEthereumChain','wallet_addEthereumChain','wallet_watchAsset',
  'wallet_requestPermissions','wallet_revokePermissions','wallet_grantPermissions',
  'wallet_sendCalls'
]);
const WALLET_READ_METHODS = new Set([
  'wallet_getPermissions','wallet_getCapabilities','wallet_getCallsStatus'
]);
const WALLET_EVENTS = new Set(['accountsChanged','chainChanged','connect','disconnect','message']);

function denied(message = 'Mini App wallet access was not authorized', code = 4100) {
  return Object.assign(new Error(message), {code});
}


// Discover EIP-6963 providers without inspecting browser profiles or extensions.
// Announcement names are self-reported: never present them as verified wallets.
// One registry belongs to one Mini App document and is disposed on teardown.
export function createDiscoveredBrowserWallet(win, options={}) {
  const wallets=[];
  const listeners=new Map();
  let active=null;
  let closed=false;
  let walletConnectClient=null;
  let userSelected=false;
  let pairing=null;
  const wcSubscriptions=new Map();
  // Client-side option only: no relay connection is made until explicit consent.
  const wcFactory=typeof options.walletConnectFactory==='function'
    ? options.walletConnectFactory:null;

  const add=(name,provider,id)=>{
    if(closed||!provider||typeof provider.request!=='function'||
        wallets.some(item=>item.provider===provider)||wallets.length>=12)return;
    const entry={id:id||'wallet-'+wallets.length,
      name:String(name||'Browser wallet').slice(0,64),provider};
    wallets.push(entry);
    if(!active || (active.id==='walletconnect'&&!userSelected&&id!=='walletconnect')) {
      const previous=active;
      for(const [event,callbacks] of listeners)for(const callback of callbacks)
        previous?.provider.removeListener?.(event,callback);
      active=entry;
      for(const [event,callbacks] of listeners)for(const callback of callbacks)
        active.provider.on?.(event,callback);
    }
    return entry;
  };
  const announce=event=>{
    const detail=event?.detail;
    if(detail?.info&&typeof detail.info.name==='string')
      add(detail.info.name,detail.provider);
  };
  const changeTo=id=>{
    if(closed)return false;
    const next=wallets.find(wallet=>wallet.id===id);
    if(!next)return false;
    userSelected=true;
    if(next===active)return true;
    for(const [event,callbacks] of listeners)for(const callback of callbacks)
      active?.provider.removeListener?.(event,callback);
    active=next;
    for(const [event,callbacks] of listeners)for(const callback of callbacks)
      active.provider.on?.(event,callback);
    // Switching providers invalidates any Mini App origin grant, including
    // when the wallet returns the same public account on a different chain.
    for(const callback of listeners.get('accountsChanged')||[])callback([]);
    return true;
  };
  const wcProxy={
    request(payload){
      if(!walletConnectClient || !walletConnectClient.session)
        return Promise.reject(denied('Connect WalletConnect before requesting accounts',4900));
      return walletConnectClient.request(payload);
    },
    on(event,callback){
      if(typeof callback!=='function')return;
      if(!wcSubscriptions.has(event))wcSubscriptions.set(event,new Set());
      wcSubscriptions.get(event).add(callback);
      walletConnectClient?.on?.(event,callback);
    },
    removeListener(event,callback){
      wcSubscriptions.get(event)?.delete(callback);
      walletConnectClient?.removeListener?.(event,callback);
    }
  };
  win.addEventListener?.('eip6963:announceProvider',announce);
  win.dispatchEvent?.(new Event('eip6963:requestProvider'));
  if(win.ethereum?.request)add('Browser wallet',win.ethereum);
  if(wcFactory)add('WalletConnect',wcProxy,'walletconnect');
  const connectWalletConnect=async ({onUri,onStatus,signal}={})=>{
    if(!wcFactory||closed)throw denied('WalletConnect is not available',4900);
    if(signal?.aborted)throw denied('Wallet connection cancelled',4001);
    if(pairing)return pairing;
    pairing=(async()=>{
      // A stalled script fetch cannot hold the Mini App indefinitely.
      // Abort/timeout ends the UI request even if the browser continues
      // resolving its network import in the background.
      onStatus?.('loading');
      const provider=await new Promise((resolve,reject)=>{
        let settled=false;
        const settle=(error,value)=>{
          if(settled)return;
          settled=true;
          clearTimeout(timer);
          signal?.removeEventListener('abort',abort);
          if(error)reject(error);else resolve(value);
        };
        const abort=()=>settle(denied('Wallet connection cancelled',4001));
        const timer=setTimeout(()=>settle(denied(
          'WalletConnect module did not load. Check your connection and retry.',4900)),25000);
        signal?.addEventListener('abort',abort,{once:true});
        Promise.resolve().then(()=>wcFactory({onUri,onStatus,signal}))
          .then(value=>settle(null,value),error=>settle(error));
      });
      if(closed||signal?.aborted)throw denied('Wallet connection cancelled',4001);
      if(!provider?.session || typeof provider.request!=='function')
        throw denied('WalletConnect connection was not approved',4900);
      if(walletConnectClient!==provider){
        // A renewed WalletConnect session cannot inherit a previous origin
        // approval. Forward only events subscribed by live Mini App facades.
        if(walletConnectClient){
          for(const [event,callbacks] of wcSubscriptions)for(const callback of callbacks)
            walletConnectClient.removeListener?.(event,callback);
        }
        walletConnectClient=provider;
        for(const [event,callbacks] of wcSubscriptions)for(const callback of callbacks)
          provider.on?.(event,callback);
      }
      return provider;
    })();
    try{return await pairing;}finally{pairing=null;}
  };
  return {
    available:()=>!closed&&!!active,
    choices:()=>closed?[]:wallets.map(({id,name})=>({id,name})),
    selectedId:()=>active?.id||'',
    supportsWalletConnect:()=>!closed&&!!wcFactory,
    walletConnectConnected:()=>!!walletConnectClient?.session,
    supportedChains:()=>wcFactory?(options.walletConnectChains||[]).slice():[],
    connectWalletConnect,
    select:changeTo,
    request(payload){
      if(closed)return Promise.reject(denied('Mini App wallet session closed',4900));
      if(!active)return Promise.reject(denied('No wallet installed in this browser',4900));
      return active.provider.request(payload);
    },
    on(event,callback){
      if(closed||typeof callback!=='function')return;
      if(!listeners.has(event))listeners.set(event,new Set());
      listeners.get(event).add(callback);
      active?.provider.on?.(event,callback);
    },
    removeListener(event,callback){
      listeners.get(event)?.delete(callback);
      active?.provider.removeListener?.(event,callback);
    },
    dispose(){
      if(closed)return;
      closed=true;
      win.removeEventListener?.('eip6963:announceProvider',announce);
      for(const [event,callbacks] of listeners)for(const callback of callbacks)
        active?.provider.removeListener?.(event,callback);
      listeners.clear();
      wcSubscriptions.clear();
      wallets.length=0;active=null;walletConnectClient=null;
    }
  };
}

// The Qt locale is forwarded to the browser host as a preference. Keep this
// independent from provider names (wallet self-reported branding is not verified).
export function walletApprovalCopy(language) {
  const code=String(language||'en').toLowerCase().split(/[-_]/)[0];
  const messages={
    en:['Connect wallet','Allow this Mini App to see your wallet address?',
      'Wallet','Cancel','Continue','No browser wallet found. Enable a wallet extension.',
      'Connect wallet to Mini App',
      'Open in a wallet app','Opens the Mini App outside GenyCaster. This is a separate session, not a WalletConnect connection.'],
    fa:['اتصال کیف پول','به این مینی‌اپ اجازه می‌دهید نشانی کیف پولتان را ببیند؟',
      'کیف پول','انصراف','ادامه','کیف پولی در مرورگر پیدا نشد. افزونه کیف پول را فعال کنید.',
      'اتصال کیف پول به مینی‌اپ',
      'بازکردن در برنامه کیف پول','مینی‌اپ خارج از جنی‌کستر و در نشست جداگانه باز می‌شود؛ این اتصال WalletConnect نیست.'],
    ar:['ربط المحفظة','هل تسمح لهذا التطبيق المصغّر بعرض عنوان محفظتك؟',
      'المحفظة','إلغاء','متابعة','لم يتم العثور على محفظة في المتصفح. فعّل إضافة المحفظة.',
      'ربط المحفظة بالتطبيق المصغّر',
      'فتح في تطبيق المحفظة','سيفتح التطبيق المصغّر خارج GenyCaster في جلسة منفصلة. هذا ليس اتصال WalletConnect.'],
    de:['Wallet verbinden','Darf diese Mini-App Ihre Wallet-Adresse sehen?',
      'Wallet','Abbrechen','Weiter','Keine Browser-Wallet gefunden. Aktivieren Sie eine Wallet-Erweiterung.',
      'Wallet mit Mini-App verbinden'],
    ja:['ウォレットを接続','このミニアプリにウォレットアドレスの表示を許可しますか？',
      'ウォレット','キャンセル','続行','ブラウザーのウォレットが見つかりません。拡張機能を有効にしてください。',
      'ミニアプリにウォレットを接続'],
    ko:['지갑 연결','이 미니앱이 지갑 주소를 볼 수 있도록 허용하시겠습니까?',
      '지갑','취소','계속','브라우저 지갑을 찾을 수 없습니다. 지갑 확장 기능을 활성화하세요.',
      '미니앱에 지갑 연결'],
    zh:['连接钱包','是否允许此迷你应用查看您的钱包地址？',
      '钱包','取消','继续','未找到浏览器钱包。请启用钱包扩展程序。',
      '将钱包连接至迷你应用'],
    hi:['वॉलेट कनेक्ट करें','क्या इस मिनी ऐप को आपका वॉलेट पता देखने दें?',
      'वॉलेट','रद्द करें','जारी रखें','ब्राउज़र वॉलेट नहीं मिला। वॉलेट एक्सटेंशन सक्रिय करें।',
      'मिनी ऐप से वॉलेट कनेक्ट करें'],
    es:['Conectar cartera','¿Permitir que esta Mini App vea la dirección de tu cartera?',
      'Cartera','Cancelar','Continuar','No se encontró una cartera. Activa una extensión de cartera.',
      'Conectar cartera a Mini App'],
    fr:['Connecter un portefeuille','Autoriser cette mini-app à voir votre adresse de portefeuille ?',
      'Portefeuille','Annuler','Continuer','Aucun portefeuille détecté. Activez une extension de portefeuille.',
      'Connecter le portefeuille à la mini-app'],
    pt:['Conectar carteira','Permitir que este Mini App veja o endereço da sua carteira?',
      'Carteira','Cancelar','Continuar','Nenhuma carteira encontrada. Ative a extensão de carteira.',
      'Conectar carteira ao Mini App'],
    ru:['Подключить кошелёк','Разрешить этому мини-приложению видеть адрес кошелька?',
      'Кошелёк','Отмена','Продолжить','Браузерный кошелёк не найден. Включите расширение кошелька.',
      'Подключить кошелёк к мини-приложению']
  };
  const text=messages[code]||messages.en;
  return {
    direction:code==='fa'||code==='ar'?'rtl':'ltr',
    title:text[0],question:text[1],wallet:text[2],
    cancel:text[3],continue:text[4],missing:text[5],aria:text[6],
    externalTitle:text[7]||messages.en[7],
    externalExplanation:text[8]||messages.en[8]
  };
}


// A browser without an injected provider cannot sign an RPC on behalf of the
// GenyCaster-hosted Mini App. Offer a clearly labelled *external* browser
// handoff on phones, never a fictitious WalletConnect/approval button.
// The destination is the original HTTPS Mini App URL, not the local bridge or
// a fabricated wallet session. Wallet-browser support depends on that app.
export function mobileWalletBrowserLinks(launchUrl, userAgent='') {
  if(!/Android|iPhone|iPad|iPod/i.test(String(userAgent))) return [];
  let url;
  try { url=new URL(String(launchUrl)); } catch { return []; }
  if(url.protocol!=='https:' || url.username || url.password ||
     !url.hostname || url.port || url.hostname==='localhost' ||
     url.hostname.endsWith('.localhost') || url.hostname.endsWith('.local') ||
     /^(?:127\.|10\.|192\.168\.|169\.254\.)/.test(url.hostname) ||
     /^172\.(?:1[6-9]|2\d|3[01])\./.test(url.hostname) ||
     /^\d+\.\d+\.\d+\.\d+$/.test(url.hostname)) return [];
  return [
    {id:'metamask',name:'MetaMask',
     href:'https://link.metamask.io/dapp/'+encodeURIComponent(
       url.host+url.pathname+url.search+url.hash)},
    {id:'coinbase',name:'Coinbase Wallet',
     href:'https://go.cb-w.com/dapp?cb_url='+encodeURIComponent(url.href)}
  ];
}

export function requestMiniAppWalletApproval(origin, signal, injected, launchUrl=origin) {
  if(typeof document==='undefined'||!document.body)return Promise.resolve(false);
  return new Promise(resolve=>{
    if(signal?.aborted){resolve(false);return;}
    const language=typeof window!=='undefined'
      ? (window.__genycasterPreferredLanguage || document.documentElement?.lang
        || (typeof navigator!=='undefined'?navigator.language:'en')):'en';
    const copy=walletApprovalCopy(language);
    const fa=copy.direction==='rtl';
    const promptText=fa
      ? {connect:'اتصال با WalletConnect',connecting:'در انتظار تأیید کیف پول…',
         scan:'کد را با کیف پول اسکن کنید یا برنامه را باز کنید.',
         retry:'تلاش دوباره',failed:'اتصال برقرار نشد. اتصال اینترنت یا دسترسی به سرویس کیف پول را بررسی کنید و دوباره تلاش کنید.',
         loading:'در حال آماده‌سازی WalletConnect…',
         creating:'در حال ساخت لینک امن کیف پول…',
         approval:'منتظر تأیید در کیف پول…',
         showQr:'نمایش کد QR',
         onekey:'بازکردن OneKey',metamask:'بازکردن MetaMask'}
      : {connect:'Connect with WalletConnect',connecting:'Waiting for wallet approval…',
         scan:'Scan with your wallet, or open a wallet app.',
         retry:'Try again',failed:'Connection could not start. Check your network or wallet service, then retry.',
         loading:'Preparing WalletConnect…',
         creating:'Creating a secure wallet pairing link…',
         approval:'Waiting for approval in your wallet…',
         showQr:'Show QR code',
         onekey:'Open OneKey',metamask:'Open MetaMask'};
    const scrim=document.createElement('div');
    Object.assign(scrim.style,{position:'fixed',inset:'0',zIndex:'2147483647',
      background:'rgba(0,0,0,.75)',display:'flex',alignItems:'center',
      justifyContent:'center',padding:'16px',boxSizing:'border-box'});
    const panel=document.createElement('section');
    Object.assign(panel.style,{width:'min(390px,100%)',maxHeight:'92dvh',
      overflowY:'auto',background:'#202126',color:'#f4f4f5',
      border:'1px solid #45434d',borderRadius:'18px',padding:'20px',
      boxSizing:'border-box',fontFamily:'system-ui,sans-serif',
      boxShadow:'0 15px 60px rgba(0,0,0,.45)'});
    panel.setAttribute('role','dialog');
    panel.setAttribute('aria-modal','true');
    panel.setAttribute('dir',copy.direction);
    panel.setAttribute('aria-label',copy.aria);
    const title=document.createElement('h2');
    title.textContent=copy.title;
    Object.assign(title.style,{margin:'0 0 10px',fontSize:'20px'});
    const originText=document.createElement('div');
    originText.textContent=origin;
    originText.setAttribute('dir','ltr');
    Object.assign(originText.style,{fontSize:'12px',color:'#b7b3c3',
      overflowWrap:'anywhere',padding:'10px',background:'#2b2b34',
      borderRadius:'9px',textAlign:'start'});
    const details=document.createElement('p');
    details.textContent=copy.question;
    Object.assign(details.style,{lineHeight:'1.5',fontSize:'14px',
      color:'#c2c3cc',margin:'12px 0'});
    panel.append(title,originText,details);
    const choices=injected?.choices?.()||[];
    const select=choices.length>1?document.createElement('select'):null;
    if(select){
      select.setAttribute('aria-label',copy.wallet);
      Object.assign(select.style,{display:'block',width:'100%',padding:'12px',
        marginBottom:'12px',borderRadius:'10px',border:'1px solid #53525d',
        background:'#303039',color:'#f4f4f5',font:'inherit'});
      for(const choice of choices){
        const option=document.createElement('option');
        option.value=choice.id;option.textContent=choice.name;
        select.append(option);
      }
      const current=injected.selectedId?.();
      if(choices.some(item=>item.id===current))select.value=current;
      panel.append(select);
    }
    const qrContainer=document.createElement('section');
    qrContainer.hidden=true;
    Object.assign(qrContainer.style,{textAlign:'center',margin:'6px 0 12px'});
    const pairingText=document.createElement('p');
    pairingText.textContent=promptText.scan;
    Object.assign(pairingText.style,{fontSize:'13px',color:'#c2c3cc',
      lineHeight:'1.5',margin:'8px'});
    const qr=document.createElement('img');
    qr.alt='WalletConnect pairing QR code';
    Object.assign(qr.style,{display:'block',width:'min(220px,100%)',
      height:'auto',margin:'10px auto',borderRadius:'10px',
      background:'white',padding:'8px'});
    const walletLinks=document.createElement('div');
    Object.assign(walletLinks.style,{display:'flex',gap:'8px',
      justifyContent:'center',flexWrap:'wrap'});
    qrContainer.append(pairingText,qr,walletLinks);
    panel.append(qrContainer);
    const error=document.createElement('p');
    error.setAttribute('role','alert');
    Object.assign(error.style,{fontSize:'12px',color:'#ff9ca6',
      lineHeight:'1.5',margin:'0 0 10px'});
    panel.append(error);
    const actions=document.createElement('div');
    Object.assign(actions.style,{display:'flex',gap:'10px',
      justifyContent:'flex-end'});
    const cancel=document.createElement('button');
    cancel.type='button';cancel.textContent=copy.cancel;
    const approve=document.createElement('button');
    approve.type='button';approve.textContent=copy.continue;
    for(const button of [cancel,approve]){
      Object.assign(button.style,{flex:'1',padding:'11px 14px',
        font:'inherit',borderRadius:'10px',cursor:'pointer',
        border:'1px solid #52535b',background:'#33343c',color:'#f4f4f5'});
    }
    approve.style.background='#7359f5';
    approve.style.borderColor='#7359f5';
    if(!choices.length){
      approve.disabled=true;approve.style.opacity='.5';
      details.textContent=copy.missing;
    }else if(choices.length===1 && choices[0].id==='walletconnect'){
      approve.textContent=promptText.connect;
    }
    actions.append(cancel,approve);panel.append(actions);
    scrim.append(panel);document.body.append(scrim);
    let resolved=false,connecting=false,pairAbort=null;
    const cleanup=()=>{
      signal?.removeEventListener('abort',onAbort);
      scrim.removeEventListener('keydown',onKey);
      scrim.remove();
    };
    const finish=value=>{
      if(resolved)return;
      resolved=true;
      if(!value)pairAbort?.abort();
      cleanup();resolve(value);
    };
    const onAbort=()=>finish(false);
    const onKey=event=>{if(event.key==='Escape')finish(false);};
    signal?.addEventListener('abort',onAbort,{once:true});
    cancel.addEventListener('click',()=>finish(false));
    scrim.addEventListener('keydown',onKey);

    approve.addEventListener('click',async()=>{
      if(resolved||connecting)return;
      const id=select?.value||choices[0]?.id||'';
      if(!id||!injected?.select?.(id))return;
      if(id!=='walletconnect'){finish(true);return;}
      connecting=true;
      error.textContent='';
      approve.disabled=true;
      approve.textContent=promptText.loading;
      details.textContent=promptText.loading;
      pairAbort=new AbortController();
      const updateStatus=stage=>{
        if(resolved || pairAbort?.signal.aborted)return;
        const label=stage==='loading'||stage==='initializing'
          ? promptText.loading
          : stage==='creating'?promptText.creating
          : stage==='approval'?promptText.approval
          : promptText.connecting;
        details.textContent=label;
        approve.textContent=label;
      };
      const mobile=/Android|iPhone|iPad|iPod/i.test(
        typeof navigator!=='undefined'?navigator.userAgent:'');
      const onUri=data=>{
        if(resolved||pairAbort?.signal.aborted)return;
        // Only valid WalletConnect v2 pairing URIs may enter a QR or deep link.
        const uri=typeof data==='string'?data:data?.uri;
        if(typeof uri!=='string'||!/^wc:[a-zA-Z0-9-]+@2\?/.test(uri)
            ||uri.length>4096)return;
        // Show mobile wallet handoff immediately, without waiting for QR
        // generation. Mobile Safari cannot scan the code on its own screen.
        qrContainer.hidden=false;
        details.textContent=promptText.scan;
        approve.textContent=promptText.approval;
        qr.hidden=true;
        walletLinks.replaceChildren();
        if(mobile){
          qrContainer.replaceChildren(pairingText,walletLinks,qr);
          const showQr=document.createElement('button');
          showQr.type='button';
          showQr.textContent=promptText.showQr;
          Object.assign(showQr.style,{border:'0',background:'transparent',
            color:'#c1b7fd',padding:'8px',font:'inherit'});
          showQr.addEventListener('click',()=>{
            qr.hidden=!qr.hidden;
          });
          qrContainer.append(showQr);
          const links=[
            {text:promptText.metamask,
             url:'https://metamask.app.link/wc?uri='+encodeURIComponent(uri)},
            {text:promptText.onekey,
             url:'https://app.onekey.so/wc/connect/wc?uri='+encodeURIComponent(uri)}
          ];
          for(const link of links){
            const anchor=document.createElement('a');
            anchor.textContent=link.text;
            anchor.href=link.url;
            anchor.target='_blank';
            anchor.rel='noopener noreferrer';
            Object.assign(anchor.style,{display:'inline-block',fontSize:'13px',
              padding:'9px 10px',borderRadius:'9px',border:'1px solid #53525d',
              color:'#f4f4f5',background:'#303039',textDecoration:'none'});
            walletLinks.append(anchor);
          }
        }
        if(data?.qr){
          qr.src=data.qr;qr.hidden=!mobile;
        }else{
          import('./genycaster-walletconnect.mjs')
            .then(wc=>wc.createQrImage(uri))
            .then(url=>{
              if(!resolved&&!pairAbort.signal.aborted){
                qr.src=url;qr.hidden=!mobile;
              }
            }).catch(()=>{
              if(!resolved)
                error.textContent=promptText.failed;
            });
        }
      };
      try{
        await injected.connectWalletConnect({
          onUri,onStatus:updateStatus,signal:pairAbort.signal
        });
        if(!resolved)finish(true);
      }catch(e){
        if(resolved)return;
        connecting=false;
        qrContainer.hidden=true;
        approve.disabled=false;
        approve.textContent=promptText.retry;
        details.textContent=copy.question;
        error.textContent=promptText.failed;
      }
    });
    cancel.focus();
  });
}


// This host-level acknowledgement is NOT an EIP-1193 wallet signature and
// never grants future writes. The wallet must independently confirm each RPC.
export function requestMiniAppWalletActionApproval(origin,method,params,signal) {
  if(signal?.aborted||typeof document==='undefined'||!document.body)
    return Promise.resolve(false);
  return new Promise(resolve=>{
    const language=typeof window==='undefined'?'en':
      (window.__genycasterPreferredLanguage||document.documentElement?.lang||
      (typeof navigator==='undefined'?'en':navigator.language));
    const rtl=/^(?:fa|ar)(?:[-_]|$)/i.test(language);
    const fa=/^fa(?:[-_]|$)/i.test(language);
    const ar=/^ar(?:[-_]|$)/i.test(language);
    const wording=fa?{
      title:'تأیید درخواست کیف پول',message:'امضای پیام',
      transaction:'ارسال تراکنش',permission:'تغییر دسترسی کیف پول',
      action:'درخواست حساس کیف پول',review:'جزئیات درخواست را در کیف پول هم بررسی و تأیید کنید.',
      origin:'درخواست از',method:'روش',destination:'مقصد',value:'مقدار (wei، هگز)',
      deny:'رد کردن',accept:'ادامه به کیف پول'
    }:ar?{
      title:'تأكيد طلب المحفظة',message:'توقيع رسالة',
      transaction:'إرسال معاملة',permission:'تغيير إذن المحفظة',
      action:'طلب حساس للمحفظة',review:'تحقق من التفاصيل ووافق في محفظتك بشكل منفصل.',
      origin:'طلب من',method:'الطريقة',destination:'الوجهة',value:'القيمة (wei، hex)',
      deny:'رفض',accept:'المتابعة إلى المحفظة'
    }:{
      title:'Review wallet request',message:'Sign a message',
      transaction:'Send a transaction',permission:'Change wallet permissions',
      action:'Sensitive wallet request',review:'Review the exact request and approve it separately in your wallet.',
      origin:'Requested by',method:'Method',destination:'To',value:'Value (hex wei)',
      deny:'Deny',accept:'Continue to wallet'
    };
    const type=method.startsWith('eth_sign')||method==='personal_sign'
      ?wording.message:
      method==='eth_sendTransaction'||method==='eth_sendUserOperation'
      ||method==='wallet_sendCalls'?wording.transaction:
      method.startsWith('wallet_')?wording.permission:wording.action;
    const scrim=document.createElement('div');
    Object.assign(scrim.style,{position:'fixed',inset:'0',zIndex:'2147483647',
      background:'rgba(0,0,0,.77)',display:'grid',placeItems:'center',
      padding:'16px',boxSizing:'border-box'});
    const panel=document.createElement('section');
    Object.assign(panel.style,{width:'min(390px,100%)',maxHeight:'90dvh',
      overflow:'auto',borderRadius:'18px',background:'#202126',color:'#f4f4f5',
      border:'1px solid #494650',padding:'20px',fontFamily:'system-ui,sans-serif',
      boxSizing:'border-box',boxShadow:'0 12px 56px #0008'});
    panel.setAttribute('role','dialog');panel.setAttribute('aria-modal','true');
    panel.setAttribute('aria-label',wording.title);
    panel.setAttribute('dir',rtl?'rtl':'ltr');
    const title=document.createElement('h2');
    title.textContent=wording.title;
    Object.assign(title.style,{fontSize:'19px',margin:'0 0 14px'});
    const kind=document.createElement('p');
    kind.textContent=type;
    Object.assign(kind.style,{fontSize:'15px',fontWeight:'650',margin:'0 0 12px'});
    const addLine=(key,value,ltr=true)=>{
      const item=document.createElement('div');
      Object.assign(item.style,{marginBottom:'10px'});
      const label=document.createElement('div');label.textContent=key;
      Object.assign(label.style,{fontSize:'12px',opacity:'.65',marginBottom:'3px'});
      const content=document.createElement('div');content.textContent=value;
      content.setAttribute('dir',ltr?'ltr':rtl?'rtl':'ltr');
      Object.assign(content.style,{fontSize:'13px',overflowWrap:'anywhere',
        padding:'9px',borderRadius:'8px',background:'#303038',
        textAlign:'start'});
      item.append(label,content);panel.append(item);
    };
    panel.append(title,kind);
    addLine(wording.origin,String(origin).slice(0,300));
    addLine(wording.method,method);
    const transaction=Array.isArray(params)&&params[0]&&
      typeof params[0]==='object'&&!Array.isArray(params[0])?params[0]:null;
    if(transaction&&typeof transaction.to==='string')
      addLine(wording.destination,transaction.to.slice(0,100));
    if(transaction&&typeof transaction.value==='string')
      addLine(wording.value,transaction.value.slice(0,100));
    const note=document.createElement('p');note.textContent=wording.review;
    Object.assign(note.style,{fontSize:'13px',color:'#bfc0ca',
      lineHeight:'1.6',margin:'10px 0 16px'});
    panel.append(note);
    const buttons=document.createElement('div');
    Object.assign(buttons.style,{display:'flex',gap:'10px'});
    const deny=document.createElement('button');
    deny.type='button';deny.textContent=wording.deny;
    const approve=document.createElement('button');
    approve.type='button';approve.textContent=wording.accept;
    for(const button of [deny,approve])
      Object.assign(button.style,{flex:'1',padding:'11px 8px',
        minHeight:'44px',borderRadius:'10px',font:'inherit',cursor:'pointer',
        background:'#34343c',color:'#f4f4f5',border:'1px solid #5b5963'});
    approve.style.background='#7359f5';approve.style.borderColor='#7359f5';
    buttons.append(deny,approve);panel.append(buttons);
    scrim.append(panel);document.body.append(scrim);
    let settled=false;
    const previous=document.activeElement;
    const finish=granted=>{
      if(settled)return;
      settled=true;
      signal?.removeEventListener('abort',onAbort);
      scrim.removeEventListener('keydown',onKey);
      scrim.remove();
      previous?.focus?.();
      resolve(granted);
    };
    const onAbort=()=>finish(false);
    const onKey=e=>{if(e.key==='Escape')finish(false);};
    signal?.addEventListener('abort',onAbort,{once:true});
    scrim.addEventListener('keydown',onKey);
    deny.addEventListener('click',()=>finish(false));
    approve.addEventListener('click',()=>finish(true));
    deny.focus();
  });
}

export function createMiniAppEthereumProvider(origin, injected, authorize=requestMiniAppWalletApproval, launchUrl=origin, approveAction=requestMiniAppWalletActionApproval) {
  if(!injected || typeof injected.request !== 'function')
    return null;
  const callbacks = new Map();
  const controller = new AbortController();
  let granted=false;
  let grantedAddress='';
  let closed=false;
  let pendingApproval=null;
  let sensitiveApprovalTail=Promise.resolve();
  let pendingSensitiveCount=0;
  const forwards=new Map();
  const guard=()=>{if(closed)throw denied('Mini App session is closed',4900);};
  const listen=(event,callback)=>{
    if(!WALLET_EVENTS.has(event) || typeof callback!=='function')return;
    if(!callbacks.has(event))callbacks.set(event,new Set());
    callbacks.get(event).add(callback);
  };
  const unlisten=(event,callback)=>callbacks.get(event)?.delete(callback);
  if(typeof injected.on === 'function'){
    for(const event of WALLET_EVENTS){
      const forward=(...args)=>{
        if(!granted||closed)return;
        let safeArgs=args;
        if(event==='disconnect'){
          granted=false;grantedAddress='';
        }else if(event==='accountsChanged' && Array.isArray(args[0])){
          const account=typeof args[0][0]==='string'?args[0][0].toLowerCase():'';
          if(!account || account!==grantedAddress){
            granted=false;grantedAddress='';
            // Never expose an unapproved account identity to this Mini App.
            safeArgs=[[]];
          }
        }
        for(const callback of callbacks.get(event)||[]){
          try{callback(...safeArgs);}catch(error){console.warn('[GenyCaster] Wallet listener failed',error);}
        }
      };
      injected.on(event,forward);
      forwards.set(event,forward);
    }
  }
  const facade={
    on:listen,
    removeListener:unlisten,
    async request(payload){
      guard();
      const method=payload?.method;
      if(typeof method!=='string'||method.length>80||
          !/^[A-Za-z][A-Za-z0-9_]*$/.test(method))
        throw denied('Invalid wallet RPC method',4200);
      if(method==='eth_accounts')
        return granted ? injected.request(payload) : [];
      if(method==='eth_requestAccounts'){
        if(!granted){
          if(!pendingApproval)pendingApproval=Promise.resolve()
            .then(()=>authorize(origin,controller.signal,injected,launchUrl))
            .finally(()=>{pendingApproval=null;});
          if(!await pendingApproval)throw denied('Wallet access was rejected by the user',4001);
          guard();
        }
        const accounts=await injected.request(payload);
        guard();
        if(!Array.isArray(accounts)||accounts.length===0)
          throw denied('Wallet returned no connected account',4001);
        granted=true;
        grantedAddress=typeof accounts[0]==='string'
          ? accounts[0].toLowerCase():'';
        return accounts;
      }
      if(PUBLIC_METHODS.has(method))
        return injected.request(payload);
      if(!granted)throw denied('Connect your wallet before using this Mini App',4100);
      if(!READ_METHODS.has(method)&&!WALLET_READ_METHODS.has(method)
          && !SIGNING_METHODS.has(method))
        throw denied('Wallet RPC method is not supported by GenyCaster',4200);
      // Bound untrusted RPC payload size before it crosses into the user's wallet.
      let size;
      try{size=JSON.stringify(payload.params??[]).length;}
      catch{throw denied('Invalid wallet RPC parameters',4200);}
      if(size>65_536)
        throw denied('Wallet RPC parameters exceed the safe limit',4200);
      if(SIGNING_METHODS.has(method)){
        // One visibly separate host prompt for EVERY sensitive operation.
        // Bound the queue so an untrusted iframe cannot flood pending dialogs.
        if(pendingSensitiveCount>=2)
          throw denied('Another wallet operation requires attention',4290);
        pendingSensitiveCount++;
        const request=sensitiveApprovalTail.then(async()=>{
          guard();
          return approveAction(origin,method,payload.params,controller.signal);
        });
        sensitiveApprovalTail=request.then(()=>{},()=>{});
        let confirmed=false;
        try{confirmed=await request;}
        finally{pendingSensitiveCount--;}
        if(!confirmed)throw denied('Wallet operation rejected by the user',4001);
        guard();
      }
      // The real wallet owns its own independent signature confirmation.
      return injected.request(payload);
    },
  };
  return {
    provider:facade,
    async chains(){
      try{
        const chain=await facade.request({method:'eth_chainId'});
        const id=typeof chain==='string'?BigInt(chain):null;
        return id!==null && id>0 && id<=BigInt(Number.MAX_SAFE_INTEGER)
          ? ['eip155:'+String(id)] : [];
      }catch{return [];}
    },
    dispose(){
      if(closed)return;
      closed=true;granted=false;grantedAddress='';controller.abort();
      for(const [event,listener] of forwards)
        injected.removeListener?.(event,listener);
      forwards.clear();callbacks.clear();
    }
  };
}
