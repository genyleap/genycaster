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
const SIGNING_PREFIXES = ['personal_sign','eth_sign','eth_signTypedData',
    'eth_sendTransaction','eth_sendRawTransaction','wallet_','eth_sendUserOperation'];
const WALLET_EVENTS = new Set(['accountsChanged','chainChanged','connect','disconnect','message']);

function denied(message = 'Mini App wallet access was not authorized', code = 4100) {
  return Object.assign(new Error(message), {code});
}


// Discover EIP-6963 providers without inspecting browser profiles or extensions.
// Announcement names are self-reported: never present them as verified wallets.
// One registry belongs to one Mini App document and is disposed on teardown.
export function createDiscoveredBrowserWallet(win) {
  const wallets=[];
  const listeners=new Map();
  let active=null;
  let closed=false;
  const add=(name,provider)=>{
    if(closed||!provider||typeof provider.request!=='function'||
        wallets.some(item=>item.provider===provider)||wallets.length>=12)return;
    const entry={id:'wallet-'+wallets.length,name:String(name||'Browser wallet').slice(0,64),provider};
    wallets.push(entry);
    if(!active) {
      active=entry;
      // Announcements may arrive after the Mini App facade has subscribed.
      for(const [event,callbacks] of listeners)for(const callback of callbacks)
        active.provider.on?.(event,callback);
    }
  };
  const announce=event=>{
    const detail=event?.detail;
    if(detail?.info&&typeof detail.info.name==='string')
      add(detail.info.name,detail.provider);
  };
  const changeTo=id=>{
    if(closed) return false;
    const next=wallets.find(wallet=>wallet.id===id);
    if(!next)return false;
    if(next===active)return true;
    for(const [event,callbacks] of listeners)for(const callback of callbacks)
      active?.provider.removeListener?.(event,callback);
    active=next;
    for(const [event,callbacks] of listeners)for(const callback of callbacks)
      active.provider.on?.(event,callback);
    // A provider change immediately invalidates any prior account grant.
    for(const callback of listeners.get('accountsChanged')||[])callback([]);
    return true;
  };
  win.addEventListener?.('eip6963:announceProvider',announce);
  // A synchronous EIP-6963 announcement may arrive immediately when requested.
  win.dispatchEvent?.(new Event('eip6963:requestProvider'));
  if(win.ethereum?.request)add('Browser wallet',win.ethereum);
  return {
    available:()=>!closed&&!!active,
    choices:()=>closed?[]:wallets.map(({id,name})=>({id,name})),
    select:changeTo,
    request(payload){
      if(closed) return Promise.reject(denied('Mini App wallet session closed',4900));
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
      listeners.clear();wallets.length=0;active=null;
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
  if(typeof document === 'undefined' || !document.body)
    return Promise.resolve(false);
  return new Promise(resolve => {
    if(signal?.aborted) { resolve(false); return; }
    const language=typeof window!=='undefined'
      ? (window.__genycasterPreferredLanguage || document.documentElement?.lang
        || (typeof navigator!=='undefined'?navigator.language:'en'))
      : 'en';
    const copy=walletApprovalCopy(language);
    const scrim=document.createElement('div');
    Object.assign(scrim.style,{position:'fixed',inset:'0',
      zIndex:'2147483647',background:'rgba(0,0,0,.76)',
      display:'flex',alignItems:'center',justifyContent:'center',
      padding:'24px',boxSizing:'border-box'});
    const panel=document.createElement('section');
    Object.assign(panel.style,{width:'min(390px,100%)',background:'#202126',
      color:'#f4f4f5',border:'1px solid #44454d',borderRadius:'18px',
      padding:'20px',boxSizing:'border-box',fontFamily:'system-ui,sans-serif',
      boxShadow:'0 15px 60px rgba(0,0,0,.45)'});
    panel.setAttribute('role','dialog');
    panel.setAttribute('aria-modal','true');
    panel.setAttribute('dir',copy.direction);
    panel.setAttribute('aria-label',copy.aria);
    const title=document.createElement('h2');
    title.textContent=copy.title;
    Object.assign(title.style,{margin:'0 0 10px',fontSize:'20px'});
    const details=document.createElement('p');
    details.textContent=copy.question;
    const appOrigin=document.createElement('div');
    appOrigin.textContent=origin;
    appOrigin.setAttribute('dir','ltr');
    Object.assign(appOrigin.style,{fontSize:'12px',color:'#b7b3c3',
      overflowWrap:'anywhere',padding:'9px 11px',background:'#2b2b34',
      borderRadius:'9px',marginBottom:'12px',textAlign:'start'});
    Object.assign(details.style,{lineHeight:'1.5',fontSize:'14px',color:'#c2c3cc'});
    panel.append(title,appOrigin,details);
    const actions=document.createElement('div');
    Object.assign(actions.style,{display:'flex',gap:'10px',justifyContent:'flex-end'});
    const candidates=injected?.choices?.()||[];
    let walletSelection=null;
    if(candidates.length>1) {
      const selectLabel=document.createElement('label');
      selectLabel.textContent=copy.wallet;
      Object.assign(selectLabel.style,{display:'block',fontSize:'13px',
        color:'#c2c3cc',margin:'14px 0 9px'});
      walletSelection=document.createElement('select');
      walletSelection.setAttribute('aria-label',copy.wallet);
      Object.assign(walletSelection.style,{display:'block',width:'100%',
        padding:'12px',marginTop:'7px',borderRadius:'9px',
        border:'1px solid #52535b',background:'#33343c',color:'#f4f4f5'});
      for(const item of candidates){
        const option=document.createElement('option');
        option.value=item.id;option.textContent=item.name;
        walletSelection.append(option);
      }
      selectLabel.append(walletSelection);panel.append(selectLabel);
    }
    const cancel=document.createElement('button');
    cancel.textContent=copy.cancel;
    const approve=document.createElement('button');
    approve.textContent=copy.continue;
    for(const button of [cancel,approve]) {
      Object.assign(button.style,{padding:'11px 14px',font:'inherit',
        borderRadius:'9px',cursor:'pointer',border:'1px solid #52535b',
        background:'#33343c',color:'#f4f4f5'});
    }
    approve.style.background='#7359f5';
    approve.style.borderColor='#7359f5';
    if(injected?.available && !injected.available()) {
      approve.disabled=true;
      approve.style.opacity='.48';
      approve.style.cursor='not-allowed';
      approve.setAttribute('aria-disabled','true');
      details.textContent=copy.missing;
      const external=mobileWalletBrowserLinks(launchUrl,
        typeof navigator!=='undefined'?navigator.userAgent:'');
      if(external.length){
        const heading=document.createElement('p');
        heading.textContent=copy.externalTitle;
        Object.assign(heading.style,{margin:'14px 0 8px',fontSize:'13px',
          fontWeight:'600',color:'#f4f4f5'});
        panel.append(heading);
        const links=document.createElement('div');
        Object.assign(links.style,{display:'grid',gap:'8px',marginBottom:'10px'});
        for(const wallet of external){
          const anchor=document.createElement('a');
          anchor.href=wallet.href;
          anchor.target='_blank';
          anchor.rel='noopener noreferrer';
          anchor.textContent=wallet.name;
          anchor.setAttribute('aria-label',copy.externalTitle+': '+wallet.name);
          Object.assign(anchor.style,{display:'block',padding:'12px 14px',
            background:'#33343c',border:'1px solid #53515f',
            color:'#f5f3fc',borderRadius:'10px',textDecoration:'none',
            fontWeight:'600',textAlign:'center'});
          links.append(anchor);
        }
        const note=document.createElement('p');
        note.textContent=copy.externalExplanation;
        Object.assign(note.style,{fontSize:'12px',color:'#b7b3c3',
          lineHeight:'1.55',margin:'0 0 12px'});
        panel.append(links,note);
      }
    }
    let resolved=false;
    const cleanup=()=>{signal?.removeEventListener('abort',abort);
      scrim.removeEventListener('keydown',onKey);scrim.remove();};
    const finish=value=>{if(resolved)return;resolved=true;cleanup();resolve(value);};
    const abort=()=>finish(false);
    const onKey=event=>{if(event.key==='Escape')finish(false);};
    signal?.addEventListener('abort',abort,{once:true});
    cancel.addEventListener('click',()=>finish(false));
    approve.addEventListener('click',()=>{
      if(walletSelection&&!injected.select(walletSelection.value))return;
      finish(true);
    });
    scrim.addEventListener('keydown',onKey);
    actions.append(cancel,approve);panel.append(actions);
    scrim.append(panel);document.body.append(scrim);
    cancel.focus();
  });
}

export function createMiniAppEthereumProvider(origin, injected, authorize=requestMiniAppWalletApproval, launchUrl=origin) {
  if(!injected || typeof injected.request !== 'function')
    return null;
  const callbacks = new Map();
  const controller = new AbortController();
  let granted=false;
  let closed=false;
  let pendingApproval=null;
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
        if(event==='accountsChanged' && Array.isArray(args[0]) && args[0].length===0)
          granted=false;
        for(const callback of callbacks.get(event)||[]){
          try{callback(...args);}catch(error){console.warn('[GenyCaster] Wallet listener failed',error);}
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
        return accounts;
      }
      if(PUBLIC_METHODS.has(method))
        return injected.request(payload);
      if(!granted)throw denied('Connect your wallet before using this Mini App',4100);
      if(!READ_METHODS.has(method)
          && !SIGNING_PREFIXES.some(prefix=>method===prefix||method.startsWith(prefix)))
        throw denied('Wallet RPC method is not supported by GenyCaster',4200);
      // Bound untrusted RPC payload size before it crosses into the user's wallet.
      if(JSON.stringify(payload.params??[]).length>65_536)
        throw denied('Wallet RPC parameters exceed the safe limit',4200);
      // The actual injected wallet owns signature/transaction confirmation.
      // An already-confirmed Mini App grant is not a signing authorization.
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
      closed=true;granted=false;controller.abort();
      for(const [event,listener] of forwards)
        injected.removeListener?.(event,listener);
      forwards.clear();callbacks.clear();
    }
  };
}
