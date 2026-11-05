// Reown WalletConnect v2 transport. Bundled on the GenyCaster origin and
// imported only after explicit user action; never injected into Mini App iframes.
import { EthereumProvider } from '@walletconnect/ethereum-provider';
import QRCode from 'qrcode';

// Project IDs are public dApp identifiers, not wallet secrets.
const projectId='020a33e91f6fc664a32d50fa41ed16b2';
const optionalChains=[10,8453,1,42161,137,7777777];
const optionalMethods=[
  'eth_accounts','eth_requestAccounts','personal_sign',
  'eth_sign','eth_signTypedData','eth_signTypedData_v3','eth_signTypedData_v4',
  'eth_sendTransaction','wallet_switchEthereumChain','wallet_addEthereumChain',
  'wallet_getPermissions','wallet_requestPermissions','wallet_watchAsset',
  'wallet_getCapabilities','wallet_sendCalls','wallet_getCallsStatus'
];
let initPromise=null;
let pairingInProgress=false;
const initialize=()=>{
  if(!initPromise){
    initPromise=EthereumProvider.init({
      projectId,optionalChains,optionalMethods,
      optionalEvents:['accountsChanged','chainChanged','disconnect','connect'],
      showQrModal:false,
      metadata:{
        name:'GenyCaster',
        description:'Your Farcaster Mini Apps, securely connected to your wallet.',
        url:window.location.origin,
        icons:[new URL('/genycaster-192.png',window.location.origin).href]
      }
    }).catch(error=>{initPromise=null;throw error;});
  }
  return initPromise;
};
export const supportedChains=Object.freeze(optionalChains.map(id=>'eip155:'+id));
export const createQrImage=uri=>QRCode.toDataURL(uri,{
  errorCorrectionLevel:'M',margin:2,width:240,color:{dark:'#111116',light:'#ffffff'}
});
export async function connectWalletConnect({onUri,signal}={}){
  if(signal?.aborted)throw Object.assign(new Error('Wallet request cancelled'),{code:4001});
  const provider=await initialize();
  if(signal?.aborted)throw Object.assign(new Error('Wallet request cancelled'),{code:4001});
  if(provider.session)return provider;
  if(pairingInProgress)
    throw Object.assign(new Error('Another wallet pairing is in progress'),{code:4900});
  pairingInProgress=true;
  let cancelled=false;
  let abortReject=()=>{};
  const onAbort=()=>{cancelled=true;
    // Do not disconnect any already-authorized session owned by another
    // Mini App. Cancel only the still-pending pairing handshake.
    try { provider.signer?.abortPairingAttempt?.(); } catch {}
  };
  const onDisplayUri=uri=>{
    if(!cancelled && typeof uri==='string'&&uri.startsWith('wc:')&&uri.length<=4096)
      onUri?.(uri);
  };
  signal?.addEventListener('abort',onAbort,{once:true});
  provider.on('display_uri',onDisplayUri);
  try {
    const aborted=new Promise((_,reject)=>{
      abortReject=()=>reject(Object.assign(
        new Error('Wallet connection cancelled'),{code:4001}));
      if(signal?.aborted)abortReject();
      else signal?.addEventListener('abort',abortReject,{once:true});
    });
    await Promise.race([provider.connect(),aborted]);
    if(cancelled||signal?.aborted)
      throw Object.assign(new Error('Wallet request cancelled'),{code:4001});
    if(!provider.session)throw new Error('Wallet did not approve a WalletConnect session');
    return provider;
  } finally {
    pairingInProgress=false;
    provider.removeListener?.('display_uri',onDisplayUri);
    signal?.removeEventListener('abort',onAbort);
    signal?.removeEventListener('abort',abortReject);
  }
}
