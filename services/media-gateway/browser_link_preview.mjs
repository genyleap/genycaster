import https from 'node:https';
import {lookup} from 'node:dns/promises';
import {isIP} from 'node:net';
import {isPublicAddress, validImageUrl} from './browser_image_proxy.mjs';
import {validMiniAppTarget} from './browser_miniapp_preview.mjs';

const MAX_HTML_BYTES = 192 * 1024;

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
  try {
    const candidate=new URL(media,pageUrl);
    if (validImageUrl(candidate.href))
      previewImage='image://remote/'+Buffer.from(candidate.href,'utf8').toString('base64url');
  }catch{}
  return {resolved:true,providerKind:'web',domain:pageUrl.hostname,title,summary,siteName,previewImage};
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
  const html=await getHtml(target,dependencies);
  if(html===null)return {status:404,error:'preview_unavailable'};
  const data=extractOpenGraph(html,target);
  if(!data.title&&!data.summary&&!data.previewImage)
    return {status:404,error:'preview_missing_metadata'};
  return {status:200,data};
}
