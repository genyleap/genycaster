// GenyCaster WASM Mini App host. Bundle the official
// @farcaster/miniapp-host@0.4.0 exposeToIframe implementation with esbuild.
// Only explicitly implemented, unprivileged capabilities are advertised.
// There is NO wallet, signer, or fabricated Farcaster authentication bridge.
import {exposeToIframe} from '@farcaster/miniapp-host';

const limitedCapabilities = Object.freeze([
  'actions.ready', 'actions.close', 'actions.openUrl',
  'actions.openMiniApp', 'actions.requestFullScreen', 'actions.exitFullScreen'
]);
const unsupported = () => {
  throw new Error('This Mini App action requires a GenyCaster native authorization flow');
};
function identity(record) {
  const value = record && typeof record === 'object' ? record : {};
  const fid = Number(value.fid);
  if (!Number.isSafeInteger(fid) || fid < 1) return {fid:0};
  const user = {fid};
  if (typeof value.username === 'string') user.username = value.username.slice(0, 100);
  if (typeof value.displayName === 'string') user.displayName = value.displayName.slice(0, 140);
  try {
    const url = new URL(value.pfpUrl);
    if (url.protocol === 'https:') user.pfpUrl = url.href;
  } catch {}
  return user;
}
function safeExternal(value) {
  const url = new URL(String(value || ''));
  if (!['https:', 'http:'].includes(url.protocol) || url.username ||
      url.password || url.host.length === 0) throw new Error('Unsafe destination');
  return url;
}
function attach(frame, address, verifiedUser, callbacks = {}) {
  if (!(frame instanceof HTMLIFrameElement)) throw new Error('Expected an iframe');
  const url = new URL(String(address || ''));
  if (url.protocol !== 'https:' || url.origin === window.location.origin ||
      url.username || url.password || url.port && url.port !== '443' ||
      new URL(frame.src).origin !== url.origin)
    throw new Error('Mini App origin is not approved');

  let active = true;
  const host = {
    context: {
      user: identity(verifiedUser),
      client: {platformType:'web', clientFid:0, added:false,
        safeAreaInsets:{top:0,bottom:0,left:0,right:0}},
      location: {type:'launcher'},
      features: {haptics:false}
    },
    ready: () => { if (active) callbacks.ready?.(); },
    close: () => { if (active) callbacks.close?.(); },
    openUrl: async value => {
      const external = safeExternal(value);
      const opened = window.open(external.href, '_blank', 'noopener,noreferrer');
      if (!opened) throw new Error('Browser prevented opening this URL');
    },
    openMiniApp: async options => {
      const dest = new URL(String(options?.url || ''));
      if (dest.protocol !== 'https:') throw new Error('Unsafe Mini App destination');
      if (!callbacks.openMiniApp?.(dest.href))
        throw new Error('Mini App session limit reached or URL rejected');
    },
    requestFullScreen: async () => callbacks.fullscreen?.(true),
    exitFullScreen: async () => callbacks.fullscreen?.(false),
    getCapabilities: async () => [...limitedCapabilities],
    getChains: async () => [],
    // Requests for unimplemented financial, identity or protocol writes fail.
    signIn: unsupported,
    signManifest: unsupported,
    addMiniApp: unsupported,
    composeCast: unsupported,
    viewCast: unsupported,
    viewProfile: unsupported,
    viewToken: unsupported,
    sendToken: unsupported,
    swapToken: unsupported,
    setPrimaryButton: unsupported,
    requestCameraAndMicrophoneAccess: unsupported,
    ethProviderRequest: unsupported,
    eip6963RequestProvider: unsupported,
    impactOccurred: unsupported,
    notificationOccurred: unsupported,
    selectionChanged: unsupported,
    updateBackState: unsupported
  };
  const exposed = exposeToIframe({
    iframe:frame, sdk:host, miniAppOrigin:url.origin, debug:false
  });
  return () => {
    active = false;
    exposed.cleanup();
  };
}
window.__genycasterMiniAppSdk = Object.freeze({attach});
window.__genycasterMiniAppOverlay?.enableSdk?.();
