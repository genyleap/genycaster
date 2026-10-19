import https from 'node:https';
import {lookup} from 'node:dns/promises';
import {isIP} from 'node:net';
import {isPublicAddress, validImageUrl} from './browser_image_proxy.mjs';
import {validMiniAppTarget} from './browser_miniapp_preview.mjs';

const MAX_HTML_BYTES = 192 * 1024;
const MAX_NFT_JSON_BYTES = 96 * 1024;

// OpenSea blocks many server-side HTML preview requests, but its public
// read-only NFT endpoint exposes the original artwork. Only a canonical
// contract+token URL may use this fixed API target; user input never selects
// an outbound host, path prefix, credentials or network address.
export function parseOpenSeaAsset(pageUrl) {
  if (!['opensea.io','www.opensea.io'].includes(pageUrl.hostname)
      || pageUrl.username || pageUrl.password || pageUrl.port) return null;
  const match=/^\/(?:assets|item)\/(ethereum|base|polygon|arbitrum|optimism|avalanche|zora)\/(0x[0-9a-fA-F]{40})\/([0-9]{1,78})\/?$/.exec(pageUrl.pathname);
  if (!match) return null;
  return {
    chain:match[1],contract:match[2].toLowerCase(),tokenId:match[3],
    apiUrl:'https://api.opensea.io/api/v2/chain/'+match[1]+'/contract/'
      +match[2].toLowerCase()+'/nfts/'+match[3],
  };
}

export function openSeaNftPreview(payload,pageUrl) {
  const nft=payload?.nft;
  if (!nft || typeof nft !== 'object') return null;
  const image=String(nft.display_image_url || nft.image_url || '');
  let candidate;
  try{ candidate=new URL(image); }catch{return null;}
  if (!safePreviewPage(candidate.href) || image.length > 2048) return null;
  const name=String(nft.name || '').trim().slice(0,140);
  const fallbackToken=pageUrl.pathname.split('/').filter(Boolean).at(-1);
  const title=name || (fallbackToken ? '#'+fallbackToken : 'NFT artwork');
  return {
    resolved:true,providerKind:'opensea',domain:'opensea.io',
    siteName:'OpenSea',title,
    summary:String(nft.description || '').slice(0,240),
    previewImage:'image://remote/'+Buffer.from(candidate.href,'utf8').toString('base64url'),
    verifiedImageUrl:candidate.href,
  };
}

async function fetchOpenSeaArtwork(asset,pageUrl,{lookupHost=lookup,requester=https.request}={}) {
  const url=new URL(asset.apiUrl);
  let addresses;
  try{addresses=await lookupHost(url.hostname,{all:true,verbatim:true});}catch{return null;}
  if(!Array.isArray(addresses)||addresses.length===0
      ||addresses.some(entry=>!isPublicAddress(entry.address,entry.family)))return null;
  const selected=addresses.find(value=>value.family===4)??addresses[0];
  const payload=await new Promise(resolve=>{
    let finished=false;
    const finish=value=>{if(!finished){finished=true;resolve(value);}};
    const req=requester(url,{
      method:'GET',timeout:6500,
      headers:{accept:'application/json','user-agent':'Genycaster/1.0 NFTArtwork'},
      lookup:(_host,opts,cb)=>opts.all?cb(null,[selected]):cb(null,selected.address,selected.family),
    },res=>{
      if(res.statusCode!==200
          ||!String(res.headers['content-type']||'').toLowerCase().includes('application/json')){
        res.resume();return finish(null);
      }
      const declared=Number(res.headers['content-length']||0);
      if(!Number.isFinite(declared)||declared>MAX_NFT_JSON_BYTES){
        res.destroy();return finish(null);
      }
      let size=0;const chunks=[];
      res.on('data',chunk=>{
        size+=chunk.length;
        if(size>MAX_NFT_JSON_BYTES){res.destroy();finish(null);return;}
        chunks.push(chunk);
      });
      res.on('end',()=>{
        if(finished)return;
        try{finish(JSON.parse(Buffer.concat(chunks,size).toString('utf8')));}
        catch{finish(null);}
      });
      res.on('error',()=>finish(null));
    });
    req.on('timeout',()=>req.destroy(new Error('NFT metadata timeout')));
    req.on('error',()=>finish(null));
    req.end();
  });
  return payload ? openSeaNftPreview(payload,pageUrl) : null;
}


export function safePreviewPage(raw) {
  const parsed=validMiniAppTarget(raw);
  if (!parsed || !['','443'].includes(parsed.port) || isIP(parsed.hostname))return null;
  // The browser must not turn user-embedded local-service URL paths or
  // credentials into server-origin requests.
  if (/^\/(?:\.well-known\/|api\/private\/)/i.test(parsed.pathname)) return null;
  return parsed;
}

function decodeEntities(input) {
  return String(input || '').replace(/&(#x[0-9a-f]+|#[0-9]+|amp|quot|apos|lt|gt|nbsp);/gi,
    (_all, code)=>{
      const token=code.toLowerCase();
      if(token[0]==='#'){
        const hex=token[1]==='x';
        const n=Number.parseInt(token.slice(hex?2:1),hex?16:10);
        return Number.isInteger(n)&&n>0&&n<=0x10ffff&&!(n>=0xd800&&n<=0xdfff)
          ? String.fromCodePoint(n):'';
      }
      return {amp:'&',quot:'"',apos:"'",lt:'<',gt:'>',nbsp:' '}[token]||'';
    }).replace(/\s+/g,' ').trim();
}

export function extractOpenGraph(html, pageUrl) {
  const meta=new Map();
  // No script evaluation, styles, dynamic HTML, or DOM parsing. Only a small
  // bounded response and quoted metadata attributes are inspected.
  const head=String(html).split(/<\/head\s*>/i,1)[0];
  for (const match of head.matchAll(/<meta\b[^>]{0,4096}>/gi)) {
    const attrs=new Map();
    for(const attribute of match[0].matchAll(/([\w:-]+)\s*=\s*(?:"([^"]*)"|'([^']*)')/g))
      attrs.set(attribute[1].toLowerCase(),decodeEntities(attribute[2]??attribute[3]));
    const key=String(attrs.get('property') || attrs.get('name') || '').toLowerCase();
    if(key && attrs.has('content') && !meta.has(key))meta.set(key,attrs.get('content'));
  }
  const titleMatch=/<title(?:\s+[^>]*)?>([\s\S]{0,500}?)<\/title\s*>/i.exec(head);
  const title=String(meta.get('og:title')||meta.get('twitter:title')||(titleMatch?decodeEntities(titleMatch[1]):'')).slice(0,160);
  const summary=String(meta.get('og:description')||meta.get('twitter:description')||meta.get('description')||'').slice(0,320);
  const siteName=String(meta.get('og:site_name')||pageUrl.hostname).slice(0,100);
  const media=String(meta.get('og:image:secure_url')||meta.get('og:image')||meta.get('twitter:image')||'');
  let previewImage='';
  let verifiedImageUrl='';
  try {
    const candidate=new URL(media,pageUrl);
    // Image sources embedded in a publicly fetched HTML page are verified
    // exact URLs, not an invitation for arbitrary image proxy requests. The
    // gateway grants a short-lived, URL-exact lease and rechecks DNS safety.
    if (media.length > 0 && safePreviewPage(candidate.href)) {
      verifiedImageUrl=candidate.href;
      previewImage='image://remote/'+Buffer.from(candidate.href,'utf8').toString('base64url');
    }
  }catch{}
  return {resolved:true,providerKind:'web',domain:pageUrl.hostname,title,summary,siteName,previewImage,verifiedImageUrl};
}

async function getHtml(url,{lookupHost=lookup,requester=https.request,depth=0}={}) {
  if(!safePreviewPage(url.href))return null;
  let addresses;
  try{addresses=await lookupHost(url.hostname,{all:true,verbatim:true});}catch{return null;}
  if(!Array.isArray(addresses)||addresses.length===0||
     addresses.some(entry=>!isPublicAddress(entry.address,entry.family)))return null;
  const selected=addresses.find(value=>value.family===4)??addresses[0];
  return await new Promise(resolve=>{
    let finished=false;
    const finish=(value)=>{if(!finished){finished=true;resolve(value);}};
    const req=requester(url,{
      method:'GET',timeout:6500,
      headers:{accept:'text/html,application/xhtml+xml;q=0.8',
        'user-agent':'Genycaster/1.0 LinkPreview'},
      lookup:(_host,opts,cb)=>opts.all?cb(null,[selected]):cb(null,selected.address,selected.family),
    },async response=>{
      const status=Number(response.statusCode||502);
      if([301,302,303,307,308].includes(status)){
        response.resume();
        if(depth>=1||!response.headers.location)return finish(null);
        let next;
        try{next=new URL(response.headers.location,url);}catch{return finish(null);}
        if(next.origin!==url.origin||!safePreviewPage(next.href))return finish(null);
        return finish(await getHtml(next,{lookupHost,requester,depth:depth+1}));
      }
      if(status!==200||!String(response.headers['content-type']||'').toLowerCase().match(/text\/html|application\/xhtml/)){
        response.resume();return finish(null);
      }
      const declared=Number(response.headers['content-length']||0);
      if(!Number.isFinite(declared)||declared>MAX_HTML_BYTES){response.destroy();return finish(null);}
      let size=0;const chunks=[];
      response.on('data',chunk=>{
        size+=chunk.length;
        if(size>MAX_HTML_BYTES){response.destroy();finish(null);return;}
        chunks.push(chunk);
      });
      response.on('end',()=>{if(!finished)finish(Buffer.concat(chunks,size).toString('utf8'));});
      response.on('error',()=>finish(null));
    });
    req.on('timeout',()=>req.destroy(new Error('Link preview timed out')));
    req.on('error',()=>finish(null));
    req.end();
  });
}

export async function publicLinkPreview(rawUrl,dependencies={}) {
  const target=safePreviewPage(rawUrl);
  if(!target)return {status:400,error:'invalid_preview_url'};
  const openSeaAsset=parseOpenSeaAsset(target);
  if(openSeaAsset){
    const artwork=await fetchOpenSeaArtwork(openSeaAsset,target,dependencies);
    if(artwork)return {status:200,data:artwork};
  }
  const html=await getHtml(target,dependencies);
  if(html===null)return {status:404,error:'preview_unavailable'};
  const data=extractOpenGraph(html,target);
  if(!data.title&&!data.summary&&!data.previewImage)
    return {status:404,error:'preview_missing_metadata'};
  return {status:200,data};
}
