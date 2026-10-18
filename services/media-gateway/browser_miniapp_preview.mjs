import {lookup} from 'node:dns/promises';
import {isIP} from 'node:net';
import https from 'node:https';
import {isPublicAddress, validImageUrl} from './browser_image_proxy.mjs';

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
  const imageUrl = validMiniAppTarget(String(app.imageUrl || ''));
  const iconUrl = validMiniAppTarget(String(app.iconUrl || ''));
  const imageSource = imageUrl && validImageUrl(imageUrl.href)
    ? 'image://remote/' + Buffer.from(imageUrl.href,'utf8').toString('base64url') : '';
  const iconSource = iconUrl && validImageUrl(iconUrl.href)
    ? 'image://remote/' + Buffer.from(iconUrl.href,'utf8').toString('base64url') : '';
  const data = {
    resolved:true,providerKind:'miniapp',
    miniAppLaunchUrl:rawUrl,
    title:String(app.name || target.hostname).slice(0,100),
    siteName:String(app.name || target.hostname).slice(0,100),
    summary:String(app.description || app.subtitle || '').slice(0,280),
    domain:target.hostname,
    previewImage:imageSource,
    favicon:iconSource,
    miniAppButtonTitle:String(app.buttonTitle || '').slice(0,48),
  };
  return {status:200,data};
}
