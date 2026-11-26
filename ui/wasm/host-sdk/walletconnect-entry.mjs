// WalletConnect v2 universal transport. Avoid the unused Reown AppKit UI:
// its old fixed Zod 3 transitive dependency fails the current security gate.
// This transport is lazy, self-hosted, and only entered after a user gesture.
import UniversalProvider from '@walletconnect/universal-provider';
import {asEthereumProvider} from './walletconnect-eip1193.mjs';
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
const ethereumEvents=['accountsChanged','chainChanged','disconnect','connect'];
export const supportedChains=Object.freeze(optionalChains.map(id=>'eip155:'+id));
const optionalNamespaces={
  eip155:{
    chains:[...supportedChains],
    methods:[...optionalMethods],
    events:[...ethereumEvents]
  }
};
let initPromise=null;
let pairingInProgress=false;
// Reusing the same approved relay session must preserve the EIP-1193
// transport identity, event subscriptions and origin-grant lifecycle.
const ethereumFacades=new WeakMap();
const facadeFor=provider=>{
  let facade=ethereumFacades.get(provider);
  if(!facade){
    facade=asEthereumProvider(provider);
    ethereumFacades.set(provider,facade);
  }
  return facade;
};
const INITIALIZE_TIMEOUT_MS=20000;
const PAIRING_URI_TIMEOUT_MS=20000;
const WALLET_APPROVAL_TIMEOUT_MS=120000;

export function withWalletDeadline(promise, timeoutMs, signal, message){
  if(signal?.aborted)return Promise.reject(
    Object.assign(new Error('Wallet connection cancelled'),{code:4001}));
  return new Promise((resolve,reject)=>{
    let done=false;
    const finish=(error,value)=>{
      if(done)return;
      done=true;clearTimeout(timer);
      signal?.removeEventListener('abort',onAbort);
      if(error)reject(error);else resolve(value);
    };
    const onAbort=()=>finish(Object.assign(
      new Error('Wallet connection cancelled'),{code:4001}));
    const timer=setTimeout(()=>finish(
      Object.assign(new Error(message),{code:4900})),timeoutMs);
    signal?.addEventListener('abort',onAbort,{once:true});
    Promise.resolve(promise).then(value=>finish(null,value),error=>finish(error));
  });
}
const initialize=(metadataOrigin=window.location.origin)=>{
  if(!initPromise){
    initPromise=UniversalProvider.init({
      projectId,
      metadata:{
        name:'GenyCaster',
        description:'Your Farcaster Mini Apps, securely connected to your wallet.',
        url:metadataOrigin,
        icons:[new URL('/genycaster-192.png',metadataOrigin).href]
      }
    }).catch(error=>{initPromise=null;throw error;});
  }
  return initPromise;
};
export async function disconnectWalletConnect(){
  if(!initPromise)return;
  const provider=await initPromise;
  if(provider.session)await provider.disconnect();
  else provider.abortPairingAttempt?.();
}
export const createQrImage=uri=>QRCode.toDataURL(uri,{
  errorCorrectionLevel:'M',margin:2,width:240,color:{dark:'#111116',light:'#ffffff'}
});
export async function connectWalletConnect({onUri,onStatus,signal,freshSession=false,metadataOrigin=window.location.origin}={}){
  if(signal?.aborted)throw Object.assign(
    new Error('Wallet connection cancelled'),{code:4001});
  onStatus?.('initializing');
  let provider;
  try{
    provider=await withWalletDeadline(initialize(metadataOrigin),INITIALIZE_TIMEOUT_MS,
      signal,'WalletConnect initialization timed out. Check the relay connection and try again.');
  }catch(error){
    // Initialization may be stuck on an unavailable network. Allow the user
    // to retry rather than reusing that permanently pending promise.
    initPromise=null;
    throw error;
  }
  if(freshSession && provider.session){
    await withWalletDeadline(provider.disconnect(),INITIALIZE_TIMEOUT_MS,signal,
      'Could not revoke the previous wallet session.');
  }
  const eip1193=facadeFor(provider);
  if(eip1193.session){
    onStatus?.('connected');
    return eip1193;
  }
  if(pairingInProgress)throw Object.assign(
    new Error('Another wallet pairing is in progress'),{code:4900});
  pairingInProgress=true;
  let cancelled=false;
  let pairingUriReceived=false;
  let resolveUri=()=>{};
  const uriReady=new Promise(resolve=>{resolveUri=resolve;});
  const onAbort=()=>{
    cancelled=true;
    // Cancel a pending proposal, but do not disconnect any existing
    // authorized WalletConnect session.
    try{provider.abortPairingAttempt?.();}catch{}
  };
  const onDisplayUri=uri=>{
    if(cancelled||signal?.aborted||typeof uri!=='string'||
        !/^wc:[a-zA-Z0-9-]+@2\?/.test(uri)||uri.length>4096)return;
    if(pairingUriReceived)return;
    pairingUriReceived=true;
    onUri?.(uri);
    resolveUri();
  };
  signal?.addEventListener('abort',onAbort,{once:true});
  provider.on('display_uri',onDisplayUri);
  try{
    onStatus?.('creating');
    const connectTask=Promise.resolve().then(()=>
      provider.connect({optionalNamespaces}));
    await withWalletDeadline(
      Promise.race([uriReady,connectTask]),PAIRING_URI_TIMEOUT_MS,signal,
      'No WalletConnect pairing link was created. Check the relay and try again.');
    if(!pairingUriReceived&&!provider.session)
      throw Object.assign(new Error('No wallet pairing session was created'),{code:4900});
    if(pairingUriReceived)onStatus?.('approval');
    await withWalletDeadline(connectTask,WALLET_APPROVAL_TIMEOUT_MS,signal,
      'Wallet approval expired. Start a new pairing request.');
    if(cancelled||signal?.aborted)
      throw Object.assign(new Error('Wallet connection cancelled'),{code:4001});
    if(!eip1193.session)throw Object.assign(
      new Error('Wallet did not approve an EVM WalletConnect session'),{code:4900});
    onStatus?.('connected');
    return eip1193;
  }catch(error){
    if(!provider.session){
      try{provider.abortPairingAttempt?.();}catch{}
    }
    throw error;
  }finally{
    pairingInProgress=false;
    provider.removeListener?.('display_uri',onDisplayUri);
    signal?.removeEventListener('abort',onAbort);
  }
}
