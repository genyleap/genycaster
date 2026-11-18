import http from 'node:http';
import {serverDeployment} from '../deployment_config.mjs';
import {createCloudHandler} from './cloud_state.mjs';
import {fetchFirmQuote,createRpcClient,constants as swapContracts} from '../swap-quote/quote.mjs';
import {createMarketReader} from './market.mjs';
import {createNeynarRegistrationHttpHandler} from './neynar_registration_http.mjs';
import {createNeynarRegistrationRelay} from './neynar_registration_relay.mjs';
import {createOptimismIdRegistryReader} from './neynar_id_registry.mjs';
import {createPaidEnrollmentVerifier} from './neynar_enrollment_voucher.mjs';

const MAX_RESPONSE_BYTES = 16 * 1024;
const DISCOVERY_TTL_MS = 5 * 60_000;
const PROVIDER_ID = /^[a-z][a-z0-9-]{0,39}$/;

export function normalizeCatalog(raw) {
  if (!raw || !Array.isArray(raw.providers) || !Array.isArray(raw.challenge_providers))
    throw new Error('invalid-provider-catalog');
  const values = [...raw.providers, ...raw.challenge_providers]
    .filter(value => typeof value === 'string' && PROVIDER_ID.test(value))
    .filter(value => value !== 'local' && value !== 'passkey');
  return {providers: [...new Set(values)].sort()};
}

export function createAccountApi({
  catalogUrl, fetchImpl = globalThis.fetch, now = Date.now,
  store = null, issuer = null,
  publicOrigin=serverDeployment().publicOrigin,
  browserGateway=serverDeployment().browserGateway,
  snapchainOrigin=serverDeployment().snapchainOrigin,
  hypersnapOrigin=serverDeployment().hypersnapOrigin,
  topicVerifier = undefined,
  proResolver = undefined,
  assetsResolver = undefined,
  billingPolicy = undefined,
  readGenyRate = undefined,
  checkProToken = undefined,
  resolveProPaymentWallets = undefined,
  verifyBillingTransfer = undefined,
  marketReader = undefined,
  registrationRelay = null,
  swapQuoteProvider = null,
  swapQuoteStatus = undefined,
} = {}) {
  const endpoint = new URL(catalogUrl);
  if (endpoint.protocol !== 'https:' || endpoint.username || endpoint.password
      || endpoint.hash || endpoint.port || endpoint.search)
    throw new Error('Account API requires a credential-free HTTPS catalog URL');

  const cloud = store && issuer ? createCloudHandler({
    store,issuer,fetchImpl,publicOrigin,browserGateway,snapchainOrigin,hypersnapOrigin,
    ...(topicVerifier ? {verifyTopicOwner:topicVerifier} : {}),
    ...(proResolver ? {resolvePro:proResolver} : {}),
    ...(assetsResolver ? {resolveGenyAssets:assetsResolver} : {}),
    ...(billingPolicy ? {billingPolicy} : {}),
    ...(readGenyRate ? {readGenyRate} : {}),
    ...(checkProToken ? {checkProToken} : {}),
    ...(resolveProPaymentWallets ? {resolveProPaymentWallets} : {}),
    ...(verifyBillingTransfer ? {verifyBillingTransfer} : {}),
    ...(swapQuoteProvider ? {swapQuoteProvider} : {})}) : null;
  const market = marketReader || createMarketReader({fetchImpl});
  const registration = registrationRelay
    ?createNeynarRegistrationHttpHandler({relay:registrationRelay,publicOrigin}):null;
  let cached = null;
  let pending = null;
  let swapStatusCached={expires:0,value:false};
  let swapStatusPending=null;
  async function readyToSwap() {
    if (!swapQuoteProvider || !store) return false;
    if (Date.now() < swapStatusCached.expires) return swapStatusCached.value;
    if (swapStatusPending) return swapStatusPending;
    swapStatusPending=Promise.resolve().then(
      ()=>swapQuoteStatus ? swapQuoteStatus() : true
    ).catch(()=>false).then(value=>{
      const ready=value===true;
      swapStatusCached={expires:Date.now()+(ready?90_000:15_000),value:ready};
      return ready;
    }).finally(()=>{swapStatusPending=null;});
    return swapStatusPending;
  }
  async function catalog() {
    if (cached && cached.until > now()) return cached.data;
    if (pending) return pending;
    pending = (async () => {
      const response = await fetchImpl(endpoint, {
        method:'GET', headers:{Accept:'application/json'},
        redirect:'error', signal:AbortSignal.timeout(5_000)
      });
      if (!response.ok || Number(response.headers.get('content-length') || 0) > MAX_RESPONSE_BYTES)
        throw new Error('catalog-fetch-failed');
      if (!response.body) throw new Error('missing-catalog-body');
      const chunks = [];
      let size = 0;
      for await (const chunk of response.body) {
        size += chunk.byteLength;
        if (size > MAX_RESPONSE_BYTES) {
          throw new Error('catalog-too-large');
        }
        chunks.push(chunk);
      }
      const parsed = JSON.parse(new TextDecoder().decode(
        Buffer.concat(chunks.map(chunk => Buffer.from(chunk)))));
      const value = normalizeCatalog(parsed);
      cached = {data:value, until:now() + DISCOVERY_TTL_MS};
      return value;
    })().finally(() => { pending = null; });
    return pending;
  }
  return http.createServer(async (req, res) => {
    res.setHeader('Cache-Control','no-store');
    res.setHeader('X-Content-Type-Options','nosniff');
    res.setHeader('Content-Type','application/json; charset=utf-8');
    const path = new URL(req.url, 'http://localhost').pathname;
    // Only direct Base on-chain quote readiness is public. Neither wallet
    // credentials nor the authenticated quote endpoint are exposed.
    if (path === '/api/v1/cloud/swap/status') {
      if (req.method !== 'GET') {
        res.writeHead(405,{'Allow':'GET'}).end(JSON.stringify({error:'method_not_allowed'}));
        return;
      }
      res.writeHead(200).end(JSON.stringify({
        available:await readyToSwap(), chainId:8453
      }));
      return;
    }
    // Market data is public, read-only and cached across every user and FID.
    // Existing Nginx cloud proxy routes this endpoint; no new public host is
    // introduced and account authorization cookies are neither needed nor read.
    if(path==='/api/v1/cloud/market') {
      if(req.method!=='GET') {
        res.writeHead(405,{'Allow':'GET'}).end(JSON.stringify({error:'method_not_allowed'}));
        return;
      }
      try {
        const data=await market();
        const status=Object.keys(data.metrics||{}).length?200:503;
        res.setHeader('Cache-Control','public, max-age=60');
        if(status!==200)res.setHeader('Retry-After','60');
        res.writeHead(status).end(JSON.stringify(data));
      }catch {
        res.writeHead(503,{'Retry-After':'60'}).end(
          JSON.stringify({error:'market_temporarily_unavailable'}));
      }
      return;
    }
    if (path.startsWith('/api/v1/registration/farcaster/')) {
      if (!registration) {
        res.writeHead(503).end(JSON.stringify({error:'registration_not_configured'}));
        return;
      }
      if (!(await registration(req,res,path)))
        res.writeHead(404).end(JSON.stringify({error:'not_found'}));
      return;
    }
    if (path.startsWith('/api/v1/cloud/')) {
      if (!cloud) {
        res.writeHead(503).end(JSON.stringify({error:'cloud_not_configured'}));
        return;
      }
      await cloud(req,res,path);
      return;
    }
    if (req.method !== 'GET') {
      res.writeHead(405,{'Allow':'GET'}).end(JSON.stringify({error:'method_not_allowed'}));
      return;
    }
    if (path === '/healthz') {
      res.writeHead(200).end(JSON.stringify({ready:true}));
      return;
    }
    if (path !== '/api/v1/connections/providers') {
      res.writeHead(404).end(JSON.stringify({error:'not_found'}));
      return;
    }
    try {
      const data = await catalog();
      res.writeHead(200).end(JSON.stringify(data));
    } catch {
      // Never expose upstream responses, secrets or bearer headers.
      res.writeHead(503).end(JSON.stringify({error:'provider_catalog_unavailable'}));
    }
  });
}

if (process.argv[1] && import.meta.url === new URL('file://' + process.argv[1]).href) {
  const origin = process.env.GENYCASTER_OPENPROOF_PROVIDER_CATALOG_URL;
  if (!origin) throw new Error('GENYCASTER_OPENPROOF_PROVIDER_CATALOG_URL required');
  const address = process.env.GENYCASTER_API_BIND_ADDRESS || '127.0.0.1';
  const port = Number(process.env.GENYCASTER_API_PORT || 8797);
  if (!Number.isInteger(port) || port <= 0 || port > 65535)
    throw new Error('Invalid GENYCASTER_API_PORT');
  // Cloud storage is disabled unless an isolated Genycaster PostgreSQL database
  // has been provisioned and its restricted runtime role configured.
  const url=process.env.GENYCASTER_DATABASE_URL;
  const store=url ? await (await import('./pg_store.mjs')).createPgStore(url) : null;
  const enableRegistration=process.env.GENYCASTER_NEYNAR_REGISTRATION_ENABLED==='true';
  let registrationRelay=null;
  if(enableRegistration) {
    if(!store?.neynarRegistrationLedger
       ||!process.env.GENYCASTER_NEYNAR_API_KEY
       ||!process.env.GENYCASTER_NEYNAR_WALLET_ID
       ||!process.env.GENYCASTER_OPTIMISM_RPC_URL
       ||!process.env.GENYCASTER_NEYNAR_ENROLLMENT_SECRET)
      throw new Error('Neynar registration needs database schema v13 and all secure backend settings');
    const voucher=createPaidEnrollmentVerifier({
      secret:process.env.GENYCASTER_NEYNAR_ENROLLMENT_SECRET});
    registrationRelay=createNeynarRegistrationRelay({
      apiKey:process.env.GENYCASTER_NEYNAR_API_KEY,
      walletId:process.env.GENYCASTER_NEYNAR_WALLET_ID,
      registry:createOptimismIdRegistryReader({
        rpcUrl:process.env.GENYCASTER_OPTIMISM_RPC_URL}),
      ledger:store.neynarRegistrationLedger,
      authorizeEnrollment:voucher.authorize
    });
  }
  // A direct Base Ethereum JSON-RPC endpoint replaces the commercial 0x
  // aggregator. The public endpoint is rate-limited; self-hosted RPCs can
  // be selected with GENYCASTER_BASE_RPC_URL for production traffic.
  const baseRpcUrl=process.env.GENYCASTER_BASE_RPC_URL
    || process.env.GENYCASTER_GENY_RPC_URL || 'https://mainnet.base.org';
  const swapRpc=createRpcClient(baseRpcUrl);
  const swapQuoteProvider=(input)=>fetchFirmQuote(input,baseRpcUrl);
  // Read-only chain and deployed router checks. Do not advertise the SDK
  // capability solely because an RPC URL was configured.
  const swapQuoteStatus=async()=>{
    const [chain,routerCode]=await Promise.all([
      swapRpc('eth_chainId',[]),
      swapRpc('eth_getCode',[swapContracts.ROUTER,'latest'])
    ]);
    return BigInt(chain)===8453n
      && typeof routerCode==='string' && /^0x[0-9a-fA-F]{64,}$/.test(routerCode);
  };
  createAccountApi({catalogUrl: origin,store,registrationRelay,swapQuoteProvider,swapQuoteStatus,
    issuer:serverDeployment().openProofIssuer,
    publicOrigin:serverDeployment().publicOrigin,
    browserGateway:serverDeployment().browserGateway,
    snapchainOrigin:serverDeployment().snapchainOrigin,
    hypersnapOrigin:serverDeployment().hypersnapOrigin})
    .listen(port,address);
}
