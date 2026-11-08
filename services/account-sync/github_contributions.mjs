// Public GitHub contribution calendars, not REST public events.
// Fixed host, strictly bounded HTML, no credentials and no third-party proxy.
const LOGIN=/^[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?$/;
const DATE=/^\d{4}-\d{2}-\d{2}$/;
const MAX_HTML=768*1024;
const MAX_CACHE=120;
const MIN_YEAR=2014;

export function parseGithubCalendar(html,{login,year,now=Date.now()}={}) {
  if(typeof html!=='string' || Buffer.byteLength(html,'utf8')>MAX_HTML)
    throw new Error('github_calendar_too_large');
  if(!LOGIN.test(login) || !Number.isInteger(year) || year<MIN_YEAR)
    throw new Error('invalid_calendar_identity');
  // GitHub publishes explicit daily counts either in matching tool-tip
  // elements or as accessible cell labels. The color level is *not* a count.
  // Allow whitespace and nested span content inside calendar cells.
  function decodeCount(text) {
    const plain=String(text || '').replace(/<[^>]*>/g,'')
      .replace(/&nbsp;|&#160;/gi,' ').replace(/&amp;/gi,'&').trim();
    const positive=/^([\d,]+)\s+contributions?\b/i.exec(plain);
    if(positive) {
      const value=Number(positive[1].replaceAll(',',''));
      return Number.isSafeInteger(value)&&value>=0&&value<=100_000 ? value : null;
    }
    return /^No contributions\b/i.test(plain) ? 0 : null;
  }
  const tips=new Map();
  for(const match of html.matchAll(/<tool-tip\b([^>]*)>([\s\S]*?)<\/tool-tip>/gi)) {
    const id=/\bfor="([^"]{1,100})"/.exec(match[1])?.[1];
    const count=decodeCount(match[2]);
    if(id && count!==null)tips.set(id,count);
  }
  const days=[];
  const seen=new Set();
  const cutoff=new Date(now).toISOString().slice(0,10);
  for(const match of html.matchAll(/<td\b([^>]*)>([\s\S]*?)<\/td>/gi)) {
    const attrs=match[1];
    if(!/\bContributionCalendar-day\b/.test(attrs))continue;
    const date=/\bdata-date="([^"]+)"/.exec(attrs)?.[1];
    const id=/\bid="([^"]+)"/.exec(attrs)?.[1];
    const levelMatch=/\bdata-level="([0-4])"/.exec(attrs);
    if(!DATE.test(date||'') || !levelMatch || seen.has(date) ||
       !date.startsWith(String(year)+'-') || date>cutoff)continue;
    const accessible=/\baria-label="([^"]{1,256})"/.exec(attrs)?.[1];
    const explicitCount=id && tips.has(id) ? tips.get(id)
      : decodeCount(accessible || match[2]);
    if(explicitCount===null || explicitCount===undefined)continue;
    seen.add(date);
    days.push({date,count:explicitCount,level:Number(levelMatch[1])});
  }
  days.sort((a,b)=>a.date.localeCompare(b.date));
  // Do not silently publish a plausible-looking but incomplete history.
  // The GitHub fragment must expose the daily cells and matching count tips.
  const expected=Math.min(
    Math.round((Date.UTC(year+1,0,1)-Date.UTC(year,0,1))/86400000),
    year===Number(cutoff.slice(0,4))
      ?Math.round((Date.parse(cutoff+'T00:00:00Z')-Date.UTC(year,0,1))/86400000)+1
      :365+Number(year%4===0&&(year%100!==0||year%400===0))
  );
  if(days.length<Math.max(20,expected-5) || days.length>366)
    throw new Error('incomplete_github_calendar');
  return {
    status:'ok',source:'github-public-contributions',username:login,year,
    count:days.reduce((sum,day)=>sum+day.count,0),
    observedDays:days.filter(day=>day.count>0).length,
    days
  };
}

export function createGithubCalendarReader({fetchImpl=globalThis.fetch,now=Date.now}={}) {
  const cached=new Map();
  const pending=new Map();
  return async function read(login,year) {
    const currentYear=new Date(now()).getUTCFullYear();
    if(!LOGIN.test(login) || !Number.isInteger(year) || year<MIN_YEAR || year>currentYear)
      throw Object.assign(new Error('invalid_calendar_request'),{status:400});
    const key=login.toLowerCase()+':'+year;
    const old=cached.get(key);
    if(old && old.until>now())return old.value;
    if(pending.has(key))return pending.get(key);
    const task=(async()=>{
      const url='https://github.com/users/'+encodeURIComponent(login)+
        '/contributions?from='+year+'-01-01&to='+year+'-12-31';
      const reply=await fetchImpl(url,{
        method:'GET',redirect:'error',
        headers:{accept:'text/html','user-agent':'GenyCaster/1.0 (public contributions reader)'},
        signal:AbortSignal.timeout(10_000)
      });
      if(!reply.ok || !String(reply.headers.get('content-type')||'').startsWith('text/html'))
        throw new Error('github_calendar_unavailable');
      if(Number(reply.headers.get('content-length')||0)>MAX_HTML || !reply.body)
        throw new Error('github_calendar_too_large');
      const chunks=[];
      let size=0;
      for await(const chunk of reply.body) {
        size+=chunk.byteLength;
        if(size>MAX_HTML)throw new Error('github_calendar_too_large');
        chunks.push(Buffer.from(chunk));
      }
      const result=parseGithubCalendar(Buffer.concat(chunks).toString('utf8'),
        {login,year,now:now()});
      if(cached.size>=MAX_CACHE)cached.delete(cached.keys().next().value);
      cached.set(key,{value:result,until:now()+(year===currentYear?20*60_000:12*3600_000)});
      return result;
    })().finally(()=>pending.delete(key));
    pending.set(key,task);
    return task;
  };
}
