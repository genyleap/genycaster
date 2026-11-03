// A per-Mini-App EIP-1193 facade. No accounts, signatures or transactions
// can cross the iframe boundary without an explicit host-origin grant.
const PUBLIC_METHODS = new Set(['eth_chainId','net_version','eth_blockNumber']);
const ACCOUNT_METHODS = new Set(['eth_accounts','eth_requestAccounts']);
const SIGNING_PREFIXES = ['personal_sign','eth_sign','eth_signTypedData',
    'eth_sendTransaction','eth_sendRawTransaction','wallet_','eth_sendUserOperation'];
const WALLET_EVENTS = new Set(['accountsChanged','chainChanged','connect','disconnect','message']);

function denied(message = 'Mini App wallet access was not authorized', code = 4100) {
  return Object.assign(new Error(message), {code});
}

export function requestMiniAppWalletApproval(origin, signal) {
  if(typeof document === 'undefined' || !document.body)
    return Promise.resolve(false);
  return new Promise(resolve => {
    if(signal?.aborted) { resolve(false); return; }
    const scrim=document.createElement('div');
    Object.assign(scrim.style,{position:'fixed',inset:'0',
      zIndex:'2147483647',background:'rgba(0,0,0,.76)',
      display:'flex',alignItems:'center',justifyContent:'center',
      padding:'24px',boxSizing:'border-box'});
    const panel=document.createElement('section');
    Object.assign(panel.style,{width:'min(440px,100%)',background:'#202126',
      color:'#f4f4f5',border:'1px solid #44454d',borderRadius:'18px',
      padding:'24px',boxSizing:'border-box',fontFamily:'system-ui,sans-serif',
      boxShadow:'0 15px 60px rgba(0,0,0,.45)'});
    panel.setAttribute('role','dialog');
    panel.setAttribute('aria-modal','true');
    panel.setAttribute('aria-label','Connect wallet to Mini App');
    const title=document.createElement('h2');
    title.textContent='Connect wallet';
    Object.assign(title.style,{margin:'0 0 10px',fontSize:'20px'});
    const details=document.createElement('p');
    details.textContent='Allow '+origin+' to request your wallet account? '+
      'Your wallet will separately approve any signature or transaction. '+
      'Your Farcaster FID alone does not grant wallet access.';
    Object.assign(details.style,{lineHeight:'1.5',fontSize:'14px',color:'#c2c3cc'});
    const actions=document.createElement('div');
    Object.assign(actions.style,{display:'flex',gap:'10px',justifyContent:'flex-end'});
    const cancel=document.createElement('button');
    cancel.textContent='Cancel';
    const approve=document.createElement('button');
    approve.textContent='Continue to wallet';
    for(const button of [cancel,approve]) {
      Object.assign(button.style,{padding:'11px 14px',font:'inherit',
        borderRadius:'9px',cursor:'pointer',border:'1px solid #52535b',
        background:'#33343c',color:'#f4f4f5'});
    }
    approve.style.background='#7359f5';
    approve.style.borderColor='#7359f5';
    let resolved=false;
    const cleanup=()=>{signal?.removeEventListener('abort',abort);
      scrim.removeEventListener('keydown',onKey);scrim.remove();};
    const finish=value=>{if(resolved)return;resolved=true;cleanup();resolve(value);};
    const abort=()=>finish(false);
    const onKey=event=>{if(event.key==='Escape')finish(false);};
    signal?.addEventListener('abort',abort,{once:true});
    cancel.addEventListener('click',()=>finish(false));
    approve.addEventListener('click',()=>finish(true));
    scrim.addEventListener('keydown',onKey);
    actions.append(cancel,approve);panel.append(title,details,actions);
    scrim.append(panel);document.body.append(scrim);
    cancel.focus();
  });
}

export function createMiniAppEthereumProvider(origin, injected, authorize=requestMiniAppWalletApproval) {
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
            .then(()=>authorize(origin,controller.signal))
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
      if(!SIGNING_PREFIXES.some(prefix=>method===prefix||method.startsWith(prefix)))
        throw denied('Wallet RPC method is not supported by GenyCaster',4200);
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
