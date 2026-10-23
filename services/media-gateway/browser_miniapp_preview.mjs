import {lookup} from 'node:dns/promises';
import {isIP} from 'node:net';
import https from 'node:https';
import {isPublicAddress} from './browser_image_proxy.mjs';

const MAX_MANIFEST_BYTES = 128 * 1024;

export function validMiniAppTarget(raw) {
  if (typeof raw !== 'string' || raw.length > 2048) return null;
  let url;
  try { url = new URL(raw); } catch { return null; }
  if (url.protocol !== 'https:' || url.username || url.password || url.port && url.port !== '443'
      || url.hostname.length > 253 || isIP(url.hostname)
      || !/^[a-z0-9.-]+$/i.test(url.hostname)
      || !url.hostname.includes('.') || url.hostname.startsWith('-')
      || /(?:^|\.)(?:localhost|local|internal|test|invalid)$/.test(url.hostname)) return null;
  url.hash = '';
  return url;
}

async function pinnedJson(url, {lookupHost = lookup, requester = https.request, depth = 0} = {}) {
  const valid = validMiniAppTarget(url.href);
  if (!valid) return null;
  let addresses;
  try { addresses = await lookupHost(valid.hostname, {all:true,verbatim:true}); }
  catch { return null; }
  if (!Array.isArray(addresses) || !addresses.length ||
      addresses.some(value => !isPublicAddress(value.address,value.family))) return null;
  const selected = addresses.find(value => value.family === 4) ?? addresses[0];
  return new Promise(resolve => {
    let settled = false;
    const done = result => { if (!settled) {settled=true; resolve(result);} };
    const request = requester(valid, {
      method:'GET',timeout:7000,
      headers:{accept:'application/json'},
      lookup:(_host, options, cb)=>options.all ? cb(null,[selected])
        : cb(null,selected.address,selected.family),
    }, async response => {
      const status = Number(response.statusCode || 502);
      if ([301,302,303,307,308].includes(status)) {
        response.resume();
        if (depth !== 0 || !response.headers.location) return done(null);
        let next;
        try { next = new URL(response.headers.location,valid); } catch { return done(null); }
        // Farcaster-hosted manifests are the only cross-origin redirect we
        // permit; never relay arbitrary redirect targets.
        const sameHost = next.hostname === valid.hostname
          && next.pathname === '/.well-known/farcaster.json';
        const hosted = next.hostname === 'api.farcaster.xyz'
          && /^\/miniapps\/hosted-manifest\/[a-zA-Z0-9_-]{1,80}$/.test(next.pathname);
        if (!sameHost && !hosted) return done(null);
        return done(await pinnedJson(next,{lookupHost,requester,depth:depth+1}));
      }
      if (status !== 200 ||
          !String(response.headers['content-type'] || '').toLowerCase().includes('json')) {
        response.resume(); return done(null);
      }
      const len = Number(response.headers['content-length'] || 0);
      if (!Number.isFinite(len) || len > MAX_MANIFEST_BYTES) {
        response.destroy(); return done(null);
      }
      let size=0;
      const chunks=[];
      response.on('data',chunk=>{
        size+=chunk.length;
        if (size > MAX_MANIFEST_BYTES) {response.destroy();done(null);return;}
        chunks.push(chunk);
      });
      response.on('end',()=>{
        if (settled) return;
        try {done(JSON.parse(Buffer.concat(chunks,size).toString('utf8')));}
        catch {done(null);}
      });
      response.on('error',()=>done(null));
    });
    request.on('timeout',()=>request.destroy(new Error('miniapp timeout')));
    request.on('error',()=>done(null));
    request.end();
  });
}

// Per-cast Frames v2 metadata is embedded in the shared URL's HTML, not
// necessarily in the app-wide manifest. Framedl, for example, generates an
// image per game/result ID. Pin DNS, cap bytes, reject cross-origin redirects,
// and only interpret a valid JSON fc:miniapp/fc:frame tag from the page head.
async function pinnedEmbedHtml(url, {
  lookupHost=lookup, requester=https.request, depth=0,
} = {}) {
  const valid=validMiniAppTarget(url.href);
  if (!valid) return null;
  let addresses;
  try{addresses=await lookupHost(valid.hostname,{all:true,verbatim:true});}
  catch{return null;}
  if(!Array.isArray(addresses)||!addresses.length
      ||addresses.some(v=>!isPublicAddress(v.address,v.family)))return null;
  const selected=addresses.find(v=>v.family===4)??addresses[0];
  return new Promise(resolve=>{
    let settled=false;
    const done=value=>{if(!settled){settled=true;resolve(value);}};
    const req=requester(valid,{
      method:'GET',timeout:7000,
      headers:{accept:'text/html','user-agent':'Genycaster/1.0 MiniAppEmbed'},
      lookup:(_host,opts,cb)=>opts.all?cb(null,[selected])
        : cb(null,selected.address,selected.family),
    },async res=>{
      const status=Number(res.statusCode||502);
      if([301,302,303,307,308].includes(status)){
        res.resume();
        if(depth!==0 || !res.headers.location)return done(null);
        let next;
        try{next=new URL(res.headers.location,valid);}catch{return done(null);}
        if(next.hostname!==valid.hostname)return done(null);
        return done(await pinnedEmbedHtml(next,{lookupHost,requester,depth:depth+1}));
      }
      if(status!==200
          ||!String(res.headers['content-type']||'').toLowerCase().includes('text/html')){
        res.resume();return done(null);
      }
      const len=Number(res.headers['content-length']||0);
      if(!Number.isFinite(len)||len>192*1024){res.destroy();return done(null);}
      let count=0;const chunks=[];
      res.on('data',part=>{
        count+=part.length;
        if(count>192*1024){res.destroy();done(null);return;}
        chunks.push(part);
      });
      res.on('end',()=>done(Buffer.concat(chunks,count).toString('utf8')));
      res.on('error',()=>done(null));
    });
    req.on('timeout',()=>req.destroy(new Error('miniapp embed deadline')));
    req.on('error',()=>done(null));
    req.end();
  });
}

function pageFrameMetadata(html, pageUrl) {
  if(typeof html!=='string')return null;
  // Meta tags belong in the head; never parse arbitrary content/body text.
  const head=html.split(/<\/head\s*>/i,1)[0];
  for(const tag of head.match(/<meta\b[^>]*>/gi)||[]){
    const attributes=Object.create(null);
    for(const match of tag.matchAll(/([a-z-]+)\s*=\s*(["'])(.*?)\2/gi)){
      attributes[match[1].toLowerCase()]=match[3];
    }
    if(!['fc:miniapp','fc:frame'].includes(String(attributes.name||attributes.property||'').toLowerCase()))continue;
    const raw=String(attributes.content||'').replace(/&quot;/gi,'"').replace(/&#34;/g,'"')
      .replace(/&apos;/gi,"'").replace(/&#39;/g,"'").replace(/&amp;/gi,'&')
      .replace(/&lt;/gi,'<').replace(/&gt;/gi,'>');
    let embed;
    try{embed=JSON.parse(raw);}catch{continue;}
    if(!embed||!['next','1'].includes(embed.version))continue;
    const button=embed.button;
    const action=button?.action;
    const title=String(button?.title||'').trim();
    if(!['launch_miniapp','launch_frame'].includes(action?.type)
       ||!title||title.length>32)return null;
    const image=validMiniAppTarget(String(embed.imageUrl||''));
    const launch=validMiniAppTarget(String(action.url||''));
    // A page must not be able to present a fake same-domain game card
    // that launches an unrelated service.
    if(!image || !launch || launch.hostname!==pageUrl.hostname)return null;
    return {imageUrl:image.href,launchUrl:launch.href,buttonTitle:title,
      appName:String(action.name||'').slice(0,100)};
  }
  return null;
}

export async function publicMiniAppPreview(rawUrl, dependencies = {}) {
  const target = validMiniAppTarget(rawUrl);
  if (!target) return {status:400,error:'invalid_miniapp_url'};
  const manifestUrl = new URL('/.well-known/farcaster.json',target);
  const payload = await pinnedJson(manifestUrl,dependencies);
  const app = payload?.miniapp || payload?.frame;
  if (!app || typeof app !== 'object') return {status:404,error:'miniapp_manifest_unavailable'};
  const home = validMiniAppTarget(String(app.homeUrl || ''));
  if (!home || home.hostname.toLowerCase() !== target.hostname.toLowerCase())
    return {status:404,error:'miniapp_origin_mismatch'};
  // Discover images from *any* public Mini App manifest, regardless of
  // publisher or asset CDN. The gateway grants exact short-lived URL leases
  // after validating the manifest; image retrieval independently enforces
  // pinned public DNS, HTTPS, size limits and raster-only payloads.
  const posters = [app.imageUrl, app.ogImageUrl, app.heroImageUrl,
    ...(Array.isArray(app.screenshotUrls) ? app.screenshotUrls.slice(0, 3) : []),
    app.splashImageUrl];
  const imageUrl = posters.map(value => validMiniAppTarget(String(value || '')))
    .find(Boolean);
  const iconUrl = validMiniAppTarget(String(app.iconUrl || ''));
  const splashUrl = validMiniAppTarget(String(app.splashImageUrl || ''));
  const gifUrl = [imageUrl, splashUrl].find(value => value
    && /\.gif$/i.test(value.pathname));
  const toImageSource = value => value
    ? 'image://remote/' + Buffer.from(value.href,'utf8').toString('base64url') : '';
  const frame=pageFrameMetadata(await pinnedEmbedHtml(target,dependencies),target);
  const frameImage=frame ? validMiniAppTarget(frame.imageUrl) : null;
  const imageSource = toImageSource(frameImage || imageUrl);
  const iconSource = toImageSource(iconUrl);
  const screenshots = (Array.isArray(app.screenshotUrls) ? app.screenshotUrls.slice(0,3) : [])
    .map(value=>validMiniAppTarget(String(value || ''))).filter(Boolean);
  const verifiedImageUrls = [...new Set([frameImage,imageUrl,iconUrl,splashUrl,gifUrl,...screenshots]
    .filter(Boolean).map(value=>value.href))];
  const data = {
    resolved:true,providerKind:'miniapp',
    miniAppLaunchUrl:frame?.launchUrl || rawUrl,
    title:String(app.name || frame?.appName || target.hostname).slice(0,100),
    siteName:String(app.name || target.hostname).slice(0,100),
    summary:String(app.description || app.subtitle || '').slice(0,280),
    domain:target.hostname,
    previewImage:imageSource,
    animatedPoster:gifUrl ? gifUrl.href : '',
    favicon:iconSource,
    miniAppButtonTitle:frame?.buttonTitle || String(app.buttonTitle || '').slice(0,48),
    verifiedImageUrls,
  };
  return {status:200,data};
}
