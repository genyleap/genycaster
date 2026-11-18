// EIP-1193 compatibility facade for the pinned WalletConnect v2 universal
// provider. The universal provider keeps EVM namespaces and relay sessions;
// this facade only normalizes chain IDs and rejects non-EVM sessions. No keys,
// origins, approval state or recovery phrases are stored here.
const denied=(message,code=4900)=>Object.assign(new Error(message),{code});

export function canonicalEvmChain(value){
  if(typeof value==='number'){
    if(!Number.isSafeInteger(value)||value<=0)return '';
    return '0x'+BigInt(value).toString(16);
  }
  if(typeof value!=='string')return '';
  const input=value.startsWith('eip155:')?value.slice(7):value;
  if(!/^(?:0x[0-9a-fA-F]+|[1-9][0-9]*)$/.test(input))return '';
  try{
    const chain=BigInt(input);
    return chain>0n&&chain<=BigInt(Number.MAX_SAFE_INTEGER)
      ? '0x'+chain.toString(16) : '';
  }catch{return '';}
}

const hasEvmAccounts=session=>
  !!session && !!session.namespaces &&
  Object.entries(session.namespaces).some(([key,namespace])=>
    (key==='eip155'||key.startsWith('eip155:')) &&
    Array.isArray(namespace?.accounts) &&
    namespace.accounts.some(account=>
      typeof account==='string' &&
      /^eip155:[1-9][0-9]*:0x[0-9a-fA-F]{40}$/.test(account)));

export function asEthereumProvider(universal){
  if(!universal||typeof universal.request!=='function'||
      typeof universal.on!=='function'||
      typeof universal.removeListener!=='function')
    throw denied('Invalid WalletConnect transport');
  const subscriptions=new Map();
  const session=()=>hasEvmAccounts(universal.session)?universal.session:null;
  return {
    get session(){return session();},
    async request(payload){
      if(!session())throw denied('WalletConnect EVM session is not connected');
      if(!payload||typeof payload.method!=='string')
        throw denied('Invalid WalletConnect RPC request',4200);
      const result=await universal.request(payload);
      if(payload.method!=='eth_chainId')return result;
      const chain=canonicalEvmChain(result);
      if(!chain)throw denied('WalletConnect returned an invalid EVM chain');
      return chain;
    },
    on(event,callback){
      if(typeof callback!=='function'||typeof event!=='string')return;
      if(!subscriptions.has(event))subscriptions.set(event,new Map());
      const callbacks=subscriptions.get(event);
      if(callbacks.has(callback))return;
      const forward=event==='chainChanged'
        ?value=>callback(canonicalEvmChain(value))
        :callback;
      callbacks.set(callback,forward);
      universal.on(event,forward);
    },
    removeListener(event,callback){
      const callbacks=subscriptions.get(event);
      const forward=callbacks?.get(callback);
      if(!forward)return;
      universal.removeListener(event,forward);
      callbacks.delete(callback);
      if(callbacks.size===0)subscriptions.delete(event);
    }
  };
}
