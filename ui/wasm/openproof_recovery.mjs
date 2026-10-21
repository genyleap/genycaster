/** OpenProof identity convergence for a verified Farcaster recovery.
 * The phrase and recovered custody key never leave the browser. Only
 * FID/address go to start, and one server-issued challenge signature to finish.
 * The gateway holds the cookie-bound OpenProof state (not the browser).
 */
export async function verifyRecoveredFarcasterIdentity({
  fid, custody, gatewayOrigin, fetchImpl = globalThis.fetch, signal
}) {
  const origin = new URL(gatewayOrigin);
  if (origin.protocol !== 'https:' || origin.username || origin.password ||
      origin.search || origin.hash || origin.pathname !== '/' ||
      origin.port && origin.port !== '443')
    throw new Error('OpenProof recovery requires configured HTTPS gateway');
  if (typeof fid !== 'string' || !/^[1-9][0-9]{0,19}$/.test(fid) ||
      typeof custody?.address !== 'string' ||
      !/^0x[0-9a-f]{40}$/i.test(custody.address) ||
      typeof custody.signMessage !== 'function')
    throw new Error('Invalid verified Farcaster custody identity');

  const request = async (path, body) => {
    if (signal?.aborted) throw new Error('Recovery cancelled');
    const response = await fetchImpl(origin.origin + path, {
      method:'POST',mode:'cors',credentials:'omit',redirect:'error',
      headers:{'content-type':'application/json',accept:'application/json'},
      body:JSON.stringify(body),signal:signal ?? AbortSignal.timeout(12_000),
    });
    if (!response.ok || !String(response.headers.get('content-type') || '')
        .toLowerCase().startsWith('application/json') ||
        Number(response.headers.get('content-length')||0)>4096)
      throw new Error('OpenProof recovery unavailable');
    const raw=await response.text();
    if (raw.length>4096)throw new Error('OpenProof response too large');
    return JSON.parse(raw);
  };
  const started=await request('/v1/browser/farcaster/recovery/start',{
    fid,address:custody.address,
  });
  if (!/^[A-Za-z0-9_-]{43}$/.test(started?.ticket || '') ||
      started.fid!==fid || typeof started.message!=='string' ||
      started.message.length<40 || started.message.length>8192 ||
      !started.message.includes('farcaster://fid/'+fid))
    throw new Error('Invalid OpenProof recovery challenge');
  if (signal?.aborted) throw new Error('Recovery cancelled');
  const signature=await custody.signMessage(started.message);
  if (!/^0x[0-9a-f]{130}$/i.test(signature) || signal?.aborted)
    throw new Error('OpenProof recovery signature invalidated');
  const result=await request('/v1/browser/farcaster/recovery/complete',{
    ticket:started.ticket,signature,
  });
  if (result?.authenticated!==true || result.fid!==fid ||
      typeof result.subject!=='string' || result.subject.length<1 ||
      result.subject.length>512 ||
      typeof result.issuer!=='string' ||
      new URL(result.issuer).protocol!=='https:' ||
      !/^recovery\.[A-Za-z0-9_-]{43}$/.test(result.account_ticket || '') ||
      !Number.isInteger(result.expires_in) || result.expires_in<60 ||
      result.expires_in>3600)
    throw new Error('OpenProof recovery identity mismatch');
  return {issuer:result.issuer,subject:result.subject,fid,
    accountTicket:result.account_ticket,expiresSeconds:result.expires_in};
}
