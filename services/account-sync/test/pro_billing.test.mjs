import test from 'node:test';
import assert from 'node:assert/strict';
import {createProBillingPolicy,decimalUnits,createProQuote,createProRateReader,
  createProTokenVerifier,subscriptionEntitlements,verifyProTransfer} from '../pro_billing.mjs';
import {createAccountApi} from '../server.mjs';

const treasury='0x'+'55'.repeat(20);
const wallet='0x'+'11'.repeat(20);
const token='0x2a3d6f8c1fc4acdcf3a75d19b445bae02f03676b';
const hash='0x'+'ab'.repeat(32);
const topic='0xddf252ad1be2c89b69c2b068fc378daa952ba7f163c4a11628f55a4df523b3ef';
const hex=n=>'0x'+BigInt(n).toString(16);
const topicAddress=a=>'0x'+'0'.repeat(24)+a.slice(2);
const env={GENYCASTER_PRO_TREASURY_ADDRESS:treasury};
const policy=createProBillingPolicy(env);
const rate={priceUsd:'0.001',priceUnits:decimalUnits('0.001'),source:'GeckoTerminal/Base'};
test('price is operator-controlled in USD cents; treasury must be explicitly configured',()=>{
  assert.equal(createProBillingPolicy({}).monthlyUsdCents,299);
  assert.equal(createProBillingPolicy({}).enabled,false);
  assert.equal(createProBillingPolicy({...env,GENYCASTER_PRO_MONTHLY_USD_CENTS:'499'})
    .monthlyUsdCents,499);
  for(const text of ['0','-1','2.99','1000000',' 299','299.0'])
    assert.throws(()=>createProBillingPolicy({...env,GENYCASTER_PRO_MONTHLY_USD_CENTS:text}));
  assert.throws(()=>createProBillingPolicy({GENYCASTER_PRO_TREASURY_ADDRESS:token}));
  assert.throws(()=>createProBillingPolicy({GENYCASTER_PRO_TREASURY_ADDRESS:'0x'+'00'.repeat(20)}));
});
test('manual-wallet quote uses at most six GENY decimals and never undercharges',()=>{
  const price='0.000727985424361289';
  const r={priceUsd:price,priceUnits:decimalUnits(price),source:'GeckoTerminal/Base'};
  const quote=createProQuote({policy,rate:r,now:Date.UTC(2026,10,9)});
  const units=BigInt(quote.amount_base_units);
  assert.equal(units%10n**12n,0n);
  assert.match(quote.amount_geny,/^[0-9]+(?:\.[0-9]{1,6})?$/);
  assert.ok(units*r.priceUnits*100n>=
    BigInt(policy.monthlyUsdCents)*10n**36n);
});
test('GENY invoice is rounded UP in ERC20 base units, not floating point',()=>{
  assert.equal(decimalUnits('0.000000000000000001'),1n);
  assert.throws(()=>decimalUnits('1e-9'));
  assert.throws(()=>decimalUnits('NaN'));
  const q=createProQuote({policy,rate,now:Date.UTC(2026,10,9)});
  assert.equal(q.price_usd_cents,299);
  assert.equal(q.amount_geny,'2990');
  assert.equal(q.amount_base_units,(2990n*10n**18n).toString());
  assert.equal(Date.parse(q.expires_at)-Date.parse(q.created_at),300000);
  const changed=createProQuote({policy:createProBillingPolicy({...env,
    GENYCASTER_PRO_MONTHLY_USD_CENTS:'499'}),rate,now:0});
  assert.equal(changed.amount_geny,'4990');
  assert.throws(()=>createProQuote({policy:createProBillingPolicy({}),rate}));
});
test('price oracle accepts exact canonical token key and fails closed on bad rate',async()=>{
  let time=0;let calls=0;
  const reader=createProRateReader({now:()=>time,fetchImpl:async url=>{
    assert.equal(String(url),'https://api.geckoterminal.com/api/v2/simple/networks/base/token_price/'+token);
    calls++;
    return new Response(JSON.stringify({data:{attributes:{
      token_prices:{[token]:'0.00059'}}}}));
  }});
  const [first,second]=await Promise.all([reader(),reader()]);
  assert.equal(first.priceUnits,590000000000000n);
  assert.equal(second.priceUsd,'0.00059');
  assert.equal(calls,1);
  time+=30000;await reader();assert.equal(calls,1);
  time+=31000;await reader();assert.equal(calls,2);
  const broken=createProRateReader({fetchImpl:async()=>new Response('{"data":{"attributes":{"token_prices":{}}}}')});
  await assert.rejects(broken(),/invalid_decimal/);
});
test('Base token contract, chain ID and decimals are checked before every new pricing window',async()=>{
  let clock=0;let calls=0;
  const fetchImpl=async(url,request)=>{
    assert.equal(url,'https://mainnet.base.org');
    const method=JSON.parse(request.body);
    calls++;
    const result=method.method==='eth_chainId'?'0x2105':
      method.method==='eth_getCode'?'0x60016000':
      method.method==='eth_call'?'0x12':null;
    return Response.json({jsonrpc:'2.0',id:1,result});
  };
  const verifier=createProTokenVerifier({fetchImpl,now:()=>clock});
  await Promise.all([verifier(policy),verifier(policy)]);
  assert.equal(calls,3);
  clock+=240000;
  await verifier(policy);assert.equal(calls,3);
  clock+=61000;
  await verifier(policy);assert.equal(calls,6);
  const wrong=createProTokenVerifier({fetchImpl:async(url,req)=>{
    const call=JSON.parse(req.body);
    return Response.json({jsonrpc:'2.0',id:1,result:call.method==='eth_chainId'?'0x2105':
      call.method==='eth_getCode'?'0x6000':'0x6'});
  }});
  await assert.rejects(wrong(policy),/unsupported_token_decimals/);
});
function receiptNetwork(overrides={}) {
  const quote=createProQuote({policy,rate,now:Date.UTC(2026,10,9)});
  const when=BigInt(Date.parse(quote.created_at)/1000+40);
  const receipt={transactionHash:hash,status:'0x1',blockNumber:'0x64',
    blockHash:'0x'+'cd'.repeat(32),logs:[{address:token,topics:[
      topic,topicAddress(wallet),topicAddress(treasury)],
      data:'0x'+BigInt(quote.amount_base_units).toString(16).padStart(64,'0')}]};
  const values={eth_chainId:'0x2105',eth_getTransactionReceipt:{...receipt,...overrides},
    eth_blockNumber:'0x69',eth_getBlockByNumber:{hash:receipt.blockHash,
      timestamp:hex(when)}};
  const fetchImpl=async(url,request)=>{
    assert.equal(url,'https://mainnet.base.org');
    const payload=JSON.parse(request.body);
    return new Response(JSON.stringify({jsonrpc:'2.0',id:1,
      result:values[payload.method]}));
  };
  return {quote:{...quote,status:'quoted'},fetchImpl,policy,owner:{fid:'42'},
    txHash:hash,verifyWallets:async()=>[wallet]};
}
test('only confirmed exact token Transfer from verified wallet to configured treasury pays',async()=>{
  const fixture=receiptNetwork();
  const result=await verifyProTransfer(fixture);
  assert.equal(result.tx_hash,hash);
  assert.equal(result.block_number,'100');
  assert.equal(result.payer,wallet);
  await assert.rejects(verifyProTransfer({...fixture,verifyWallets:async()=>[]}),
    /verified_wallet_required/);
  const wrong=receiptNetwork({logs:[{address:token,topics:[
    topic,topicAddress(wallet),topicAddress('0x'+'66'.repeat(20))],
    data:'0x'+BigInt(fixture.quote.amount_base_units).toString(16).padStart(64,'0')}]});
  await assert.rejects(verifyProTransfer(wrong),/payment_not_matched/);
  await assert.rejects(verifyProTransfer(receiptNetwork({status:'0x0'})),/payment_failed/);
  await assert.rejects(verifyProTransfer({...fixture,txHash:'0xdead'}),/invalid_payment_hash/);
});
test('renewal entitlements expire automatically without token holding',()=>{
  const start=Date.UTC(2026,10,9);
  const end=new Date(start+30*86400_000).toISOString();
  assert.equal(subscriptionEntitlements({expires_at:end},policy,start).eligible,true);
  assert.equal(subscriptionEntitlements({expires_at:end},policy,start+31*86400_000).eligible,false);
  assert.equal(subscriptionEntitlements(null,policy,start).eligible,false);
  assert.equal(subscriptionEntitlements(null,policy,start).monthly_usd_cents,299);
});
test('server checkout requires authenticated FID and never grants on unverified transaction',async()=>{
  const calls=[];const quotes=new Map();
  const store={
    proBillingReady:()=>true,
    async getProSubscription(){return null;},
    async createProQuote(owner,quote){
      calls.push(['quote',owner.fid]);const item={...quote,status:'quoted'};
      quotes.set(quote.id,item);return item;
    },
    async getProQuote(owner,id){calls.push(['get',owner.fid]);return quotes.get(id);},
    async confirmProPayment(owner,id,receipt){
      calls.push(['confirm',owner.fid]);return {status:'paid'};
    }
  };
  const fetchImpl=async(url,req={})=>{
    if(url.endsWith('/account/profile'))
      return new Response(JSON.stringify({identity_id:'farcaster:42'}));
    if(url.endsWith('/account/connections'))
      return new Response(JSON.stringify({connections:[{provider:'farcaster',fid:'42'}]}));
    return new Response('{}',{status:404});
  };
  const api=createAccountApi({catalogUrl:'https://id.example.test',issuer:'https://id.example.test',
    store,fetchImpl,billingPolicy:policy,readGenyRate:async()=>rate,
    checkProToken:async()=>true,
    resolveProPaymentWallets:async()=>[wallet],
    verifyBillingTransfer:async()=>{throw new Error('payment_not_matched');}});
  await new Promise(resolve=>api.listen(0,'127.0.0.1',resolve));
  const base='http://127.0.0.1:'+api.address().port+'/api/v1/cloud/';
  const req=(path,auth=true,body={},method='POST')=>fetch(base+path,{
    method,headers:{'content-type':'application/json',
      ...(auth?{authorization:'Bearer allowed'}:{})},body:JSON.stringify(body)});
  try{
    const anonymousPlan=await fetch(base+'pro/plan');
    assert.equal(anonymousPlan.status,200);
    const publishedPlan=(await anonymousPlan.json()).plan;
    assert.equal(publishedPlan.price_usd_cents,299);
    assert.equal(publishedPlan.period_days,30);
    assert.equal(publishedPlan.chain_id,8453);
    assert.equal(publishedPlan.payment_available,true);
    assert.equal('treasury' in publishedPlan,false);
    assert.equal((await req('pro/plan',false)).status,405);
    assert.equal((await req('pro/quote',false)).status,401);
    const q=await req('pro/quote');
    assert.equal(q.status,201);
    const quote=(await q.json()).quote;
    assert.equal(quote.price_usd_cents,299);
    assert.equal(quote.amount_geny,'2990');
    const invalid=await req('pro/verify',true,{quote_id:quote.id,tx_hash:hash});
    assert.equal(invalid.status,422);
    assert.equal(calls.some(([action])=>action==='confirm'),false);
  }finally {await new Promise(resolve=>api.close(resolve));}
});
