// Public Farcaster identity decoration for opted-in Streak ranks only.
// HyperSnap is a fixed, public profile read; no tokens or user-provided URLs.
const ORIGIN='https://haatz.quilibrium.com/v2/farcaster/user?fid=';
const FID=/^[1-9][0-9]{0,18}$/;
const MAX_BODY=16384;
const TTL_OK=5*60_000;
const TTL_ERROR=60_000;
const MAX_CACHE=256;

export function createLeaderboardProfileReader({fetchImpl=globalThis.fetch,now=Date.now}={}) {
  const cache=new Map();
  const pending=new Map();
  async function resolve(fid) {
    fid=String(fid||'');
    if(!FID.test(fid) || !Number.isSafeInteger(Number(fid)))return null;
    const saved=cache.get(fid);
    if(saved && saved.expires>now())return saved.value;
    if(pending.has(fid))return pending.get(fid);
    const work=(async()=>{
      let value=null;
      try {
        const response=await fetchImpl(ORIGIN+encodeURIComponent(fid),{
          method:'GET',redirect:'error',headers:{accept:'application/json'},
          signal:AbortSignal.timeout(2800)
        });
        if(response.status!==200 || !response.body ||
            Number(response.headers.get('content-length')||0)>MAX_BODY)return null;
        let size=0;const chunks=[];
        for await(const chunk of response.body) {
          size+=chunk.byteLength;
          if(size>MAX_BODY)break;
          chunks.push(Buffer.from(chunk));
        }
        if(size>MAX_BODY)return null;
        const user=JSON.parse(Buffer.concat(chunks).toString('utf8'))?.user;
        if(String(user?.fid||'')!==fid)return null;
        const display=String(user.display_name||'').trim().slice(0,96);
        const username=/^[\w.-]{1,96}$/.test(user.username||'')
          ?user.username:'';
        const rawImage=String(user.pfp_url||'');
        let avatar='';
        if(rawImage.length<=512) {
          try {
            const url=new URL(rawImage);
            if(url.protocol==='https:' && !url.username && !url.password &&
                !url.port && !/\s/.test(rawImage))avatar=url.href;
          }catch{}
        }
        value={display_name:display,username,avatar_url:avatar};
      }catch{}
      finally {
        if(cache.size>=MAX_CACHE)cache.delete(cache.keys().next().value);
        cache.set(fid,{value,expires:now()+(value?TTL_OK:TTL_ERROR)});
        pending.delete(fid);
      }
      return value;
    })();
    pending.set(fid,work);
    return work;
  }
  async function decorate(leaders) {
    if(!Array.isArray(leaders))return [];
    const rows=leaders.slice(0,100).map(item=>({...item}));
    let index=0;
    await Promise.all(Array.from({length:Math.min(8,rows.length)},async()=>{
      while(index<rows.length){
        const pos=index++;
        const identity=await resolve(rows[pos].fid);
        if(identity)Object.assign(rows[pos],identity);
      }
    }));
    return rows;
  }
  return {decorate};
}
