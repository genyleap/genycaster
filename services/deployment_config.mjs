// Server-only validation of first-party public endpoints. Secrets are never
// passed into build-generated C++ or browser configuration.
const HOST = /^(?=.{1,253}$)(?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.)*[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$/;

export function trustedOrigin(raw, name, {path = false} = {}) {
  if (typeof raw !== 'string' || raw.length > 512 || raw.length < 8
      || /[\x00-\x20\x7f\\\"'`$;{}<>|]/.test(raw))
    throw new Error(name + ': invalid deployment URL');
  let url;
  try { url = new URL(raw); }
  catch { throw new Error(name + ': invalid deployment URL'); }
  const pathname = url.pathname.replace(/\/+$/, '') || '/';
  if (url.protocol !== 'https:' || !url.hostname.includes('.')
      || /^(?:[0-9]+\.){3}[0-9]+$/.test(url.hostname) || !HOST.test(url.hostname)
      || url.host !== url.hostname || url.username || url.password
      || url.port || url.search || url.hash || (path ? pathname !== '/openproof' : pathname !== '/'))
    throw new Error(name + (path
      ? ': must be HTTPS /openproof with a credential-free canonical origin'
      : ': must use HTTPS and a credential-free canonical origin'));
  return path ? url.origin + '/openproof' : url.origin;
}

export function serverDeployment(env = process.env) {
  const publicOrigin = trustedOrigin(
    env.GENYCASTER_PUBLIC_ORIGIN || 'https://genycaster.xyz',
    'GENYCASTER_PUBLIC_ORIGIN');
  const customOrigin = publicOrigin !== 'https://genycaster.xyz';
  if (env.GENYCASTER_DEPLOYMENT_STRICT === '1' || customOrigin) {
    const required = [
      'GENYCASTER_PUBLIC_ORIGIN', 'GENYCASTER_OPENPROOF_ISSUER',
      'GENYCASTER_BROWSER_GATEWAY_ORIGIN',
    ];
    const aliases = [
      ['GENYCASTER_OPENPROOF_BROWSER_BASE', 'GENYCASTER_OPENPROOF_CONNECTION_BROWSER_BASE'],
      ['GENYCASTER_MEDIA_PUBLIC_ORIGIN', 'GENYLEAP_MEDIA_PUBLIC_BASE_URL'],
      ['GENYCASTER_SNAPCHAIN_ORIGIN','GENYCASTER_SNAPCHAIN_URL','GENYLEAP_MEDIA_SNAPCHAIN_BASE_URL'],
      ['GENYCASTER_HYPERSNAP_ORIGIN'],
    ];
    if (required.some(key => !env[key])
        || aliases.some(group => !group.some(key => !!env[key])))
      throw new Error('Self-hosted deployment requires complete first-party endpoint configuration');
  }
  const deployment = Object.freeze({
    publicOrigin,
    openProofIssuer: trustedOrigin(env.GENYCASTER_OPENPROOF_ISSUER || 'https://openproof.genyleap.com',
      'GENYCASTER_OPENPROOF_ISSUER'),
    openProofBrowserBase: trustedOrigin(
      env.GENYCASTER_OPENPROOF_CONNECTION_BROWSER_BASE
      || env.GENYCASTER_OPENPROOF_BROWSER_BASE || 'https://genyleap.com/openproof',
      'GENYCASTER_OPENPROOF_BROWSER_BASE',{path:true}),
    browserGateway: trustedOrigin(env.GENYCASTER_BROWSER_GATEWAY_ORIGIN || 'https://storage-farcaster.genyleap.com',
      'GENYCASTER_BROWSER_GATEWAY_ORIGIN'),
    mediaPublicOrigin: trustedOrigin(env.GENYCASTER_MEDIA_PUBLIC_ORIGIN || env.GENYLEAP_MEDIA_PUBLIC_BASE_URL || 'https://media-farcaster.genyleap.com',
      'GENYLEAP_MEDIA_PUBLIC_BASE_URL'),
    snapchainOrigin: trustedOrigin(
      env.GENYCASTER_SNAPCHAIN_ORIGIN || env.GENYLEAP_MEDIA_SNAPCHAIN_BASE_URL
      || env.GENYCASTER_SNAPCHAIN_URL || 'https://snapchain.genyleap.com',
      'GENYCASTER_SNAPCHAIN_URL'),
    hypersnapOrigin: trustedOrigin(env.GENYCASTER_HYPERSNAP_ORIGIN || 'https://haatz.quilibrium.com',
      'GENYCASTER_HYPERSNAP_ORIGIN'),
  });
  if (deployment.openProofIssuer === deployment.publicOrigin)
    throw new Error('The trusted issuer must be isolated from the public application origin');
  return deployment;
}
