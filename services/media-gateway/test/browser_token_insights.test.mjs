import assert from 'node:assert/strict';
import test from 'node:test';
import { publicTokenSnapshot } from '../browser_token_insights.mjs';

test('token insight URL is restricted to a canonical EVM token identity', async () => {
  let calls = 0;
  const fetchImpl = async () => { calls++; throw Error('unexpected network request'); };
  for (const [network, address] of [
    ['sol', '0x' + 'a'.repeat(40)], ['base', 'localhost'], ['eth', '0x123'],
    ['https://evil.test', '0x' + 'a'.repeat(40)],
  ]) {
    assert.equal((await publicTokenSnapshot(network, address, fetchImpl)).status, 400);
  }
  assert.equal(calls, 0);
});

test('token insight returns factual GeckoTerminal fields, keeps missing metrics absent', async () => {
  const urls = [];
  const address = '0x' + 'a'.repeat(40);
  const fetchImpl = async url => {
    urls.push(url);
    if (url.endsWith('/pools?page=1')) return new Response(JSON.stringify({data:[{
      attributes: {
        reserve_in_usd: '2048.5', pool_created_at: '2025-12-13T09:16:13Z',
        price_change_percentage: { h24: '-1.25' },
        transactions: { h24: { buyers: 19 } },
      },
    }]}), { headers: { 'content-type': 'application/json' } });
    return new Response(JSON.stringify({data:{attributes:{
      name:'Example',symbol:'ABC',price_usd:'0.001',
      fdv_usd:'250000', market_cap_usd:null,
      image_url:'https://assets.geckoterminal.com/abc',
    }}}), { headers: { 'content-type': 'application/json' } });
  };
  const result = await publicTokenSnapshot('base', address, fetchImpl);
  assert.equal(result.status, 200);
  assert.equal(urls.length, 2);
  assert.ok(urls.every(url=>url.startsWith('https://api.geckoterminal.com/api/v2/networks/base/tokens/')));
  assert.equal(result.data.priceUsd, 0.001);
  assert.equal(result.data.fdvUsd, 250000);
  assert.equal(result.data.marketCapUsd, 0);
  assert.equal(result.data.liquidityUsd, 2048.5);
  assert.equal(result.data.buyers24h, 19);
  assert.equal(result.data.change24h, -1.25);
  assert.equal(result.data.imageSource,
    'https://storage-farcaster.genyleap.com/v1/browser/images?url=https%3A%2F%2Fassets.geckoterminal.com%2Fabc');
  assert.equal(result.data.source, 'GeckoTerminal');
});

test('missing pool metrics never become fabricated positive figures', async () => {
  const address = '0x' + 'b'.repeat(40);
  const result = await publicTokenSnapshot('eth', address, async url =>
    url.endsWith('/pools?page=1') ? new Response('', {status:429}) :
    new Response(JSON.stringify({data:{attributes:{name:'Minimal',symbol:'MIN',price_usd:'0.2'}}}),
      {headers:{'content-type':'application/json'}}));
  assert.equal(result.status, 200);
  assert.equal(result.data.priceUsd, 0.2);
  assert.equal(result.data.buyers24h, null);
  assert.equal(result.data.change24h, null);
  assert.equal(result.data.liquidityUsd, 0);
});
