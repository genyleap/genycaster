// Public read-only token insights for the browser's restricted WASM build.
import {serverDeployment,trustedOrigin} from '../deployment_config.mjs';
// Inputs are token identities, never arbitrary URLs or user-controlled hosts.
const upstream = 'https://api.geckoterminal.com/api/v2';
const evmAddress = /^0x[0-9a-f]{40}$/i;
const finite = value => {
  const number = Number(value);
  return Number.isFinite(number) && number >= 0 ? number : 0;
};
const positive = value => {
  const number = Number(value);
  return Number.isFinite(number) && number > 0 ? number : 0;
};

async function boundedJson(response, maximumBytes = 256 * 1024) {
  if (!response.ok || !String(response.headers.get('content-type') || '').includes('application/json')) {
    throw new Error('Token upstream unavailable');
  }
  const reported = Number(response.headers.get('content-length') || 0);
  if (reported > maximumBytes) throw new Error('Token upstream payload too large');
  const chunks = [];
  let size = 0;
  if (response.body) {
    for await (const chunk of response.body) {
      size += chunk.byteLength;
      if (size > maximumBytes) throw new Error('Token upstream payload too large');
      chunks.push(Buffer.from(chunk));
    }
  }
  return JSON.parse(Buffer.concat(chunks).toString('utf8'));
}

export async function publicTokenSnapshot(network, address, fetchImpl = globalThis.fetch,
  browserGateway=serverDeployment().browserGateway) {
  const trustedGateway=trustedOrigin(browserGateway,'GENYCASTER_BROWSER_GATEWAY_ORIGIN');
  const chain = String(network || '').trim().toLowerCase();
  const contract = String(address || '').trim().toLowerCase();
  if (!['base', 'eth'].includes(chain) || !evmAddress.test(contract)) {
    return { status: 400, error: 'invalid_token_reference' };
  }
  const stem = upstream + '/networks/' + chain + '/tokens/' + contract;
  try {
    const tokenResponse = await fetchImpl(stem, {
      headers: { accept: 'application/json' },
      signal: AbortSignal.timeout(8000),
      redirect: 'error',
    });
    if (tokenResponse.status === 404) return { status: 404, error: 'token_not_found' };
    const token = await boundedJson(tokenResponse);
    const attributes = token?.data?.attributes;
    if (!attributes || typeof attributes !== 'object') {
      return { status: 502, error: 'invalid_token_response' };
    }

    // Pool enrichment is optional; the token price must remain usable when a
    // secondary request is rate-limited. Do not invent unavailable statistics.
    let pool = {};
    try {
      const poolsResponse = await fetchImpl(stem + '/pools?page=1', {
        headers: { accept: 'application/json' },
        signal: AbortSignal.timeout(7000),
        redirect: 'error',
      });
      const pools = await boundedJson(poolsResponse);
      pool = pools?.data?.[0]?.attributes || {};
    } catch { /* Missing pool fields remain absent in the response. */ }

    const logo = String(attributes.image_url || '');
    const imageSource = /^https:\/\/assets\.geckoterminal\.com\/[a-zA-Z0-9/_-]+$/.test(logo)
      ? trustedGateway+'/v1/browser/images?url=' + encodeURIComponent(logo)
      : '';
    const transactions = pool.transactions?.h24 || {};
    const createdAt = Date.parse(String(pool.pool_created_at || ''));
    const snapshot = {
      available: true,
      name: String(attributes.name || '').slice(0, 100),
      symbol: String(attributes.symbol || '').slice(0, 24),
      network: chain,
      address: contract,
      imageSource,
      priceUsd: positive(attributes.price_usd),
      marketCapUsd: positive(attributes.market_cap_usd),
      fdvUsd: positive(attributes.fdv_usd),
      liquidityUsd: positive(pool.reserve_in_usd || attributes.total_reserve_in_usd),
      volume24hUsd: finite(pool.volume_usd?.h24 || attributes.volume_usd?.h24),
      change24h: Number.isFinite(Number(pool.price_change_percentage?.h24))
        ? Number(pool.price_change_percentage.h24) : null,
      buyers24h: Number.isSafeInteger(Number(transactions.buyers))
        ? Number(transactions.buyers) : null,
      poolCreatedAtMs: Number.isFinite(createdAt) && createdAt > 0 ? createdAt : null,
      source: 'GeckoTerminal',
    };
    return { status: 200, data: snapshot };
  } catch {
    return { status: 502, error: 'token_source_unavailable' };
  }
}
