import {readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import sharp from 'sharp';
import {retrieveBrowserImage} from './browser_image_proxy.mjs';

const index=JSON.parse(readFileSync(new URL('./sticker_preview_index.json',import.meta.url)));
if(index.version!==1 || Object.keys(index.items).length!==276)
  throw Error('Sticker preview index requires 276 pinned sticker IDs');

const allowedHosts=['mssgs-stickers.com','red-geographical-clownfish-714.mypinata.cloud'];
export function previewEntry(id){
  if(typeof id!=='string'||!/^[a-z0-9-]{2,125}$/.test(id))return null;
  return Object.hasOwn(index.items,id) ? index.items[id] : null;
}
// Only public catalogue previews are converted; never arbitrary remote URLs,
// HTML, SVG, credentials, or private-network addresses.
export function createStickerPreviews({
  retrieve=retrieveBrowserImage,
  managedMediaHost='',
  convert=async bytes=>sharp(bytes,{limitInputPixels:2_000_000,failOn:'error'})
    .rotate().resize({width:256,height:256,fit:'inside',
      withoutEnlargement:true}).png({compressionLevel:8}).toBuffer(),
  now=()=>Date.now(),
}={}){
  const cache=new Map(),pending=new Map();
  let totalBytes=0;
  const remember=(id,data)=>{
    while(cache.size>=96 || totalBytes+data.length>12*1024*1024) {
      const first=cache.keys().next().value;
      if(first===undefined)break;
      totalBytes-=cache.get(first).body.length;
      cache.delete(first);
    }
    cache.set(id,{body:data,expires:now()+3600_000});
    totalBytes+=data.length;
  };
  return Object.freeze({
    async get(id){
      const entry=previewEntry(id);
      if(!entry)return {status:404,error:'sticker_not_found'};
      const cached=cache.get(id);
      if(cached && cached.expires>now())return {status:200,body:cached.body,
        contentType:'image/png',cached:true};
      if(cached){totalBytes-=cached.body.length;cache.delete(id);}
      if(!pending.has(id)){
        const job=(async()=>{
          const fetched=await retrieve(entry.source,{
            allowedHosts,managedMediaHost});
          if(fetched.status!==200 || fetched.contentType!=='image/webp'
             || !Buffer.isBuffer(fetched.body) || fetched.body.length>1024*1024)
            return {status:502,error:'sticker_preview_unavailable'};
          if(entry.sha256 &&
            createHash('sha256').update(fetched.body).digest('hex')!==entry.sha256)
            return {status:502,error:'sticker_integrity_failed'};
          const png=await convert(fetched.body);
          if(!Buffer.isBuffer(png)||png.length===0||png.length>1024*1024
            || !png.subarray(0,8).equals(Buffer.from([137,80,78,71,13,10,26,10])))
            return {status:502,error:'sticker_preview_invalid'};
          remember(id,png);
          return {status:200,body:png,contentType:'image/png'};
        })().catch(()=>({status:502,error:'sticker_preview_unavailable'}))
          .finally(()=>pending.delete(id));
        pending.set(id,job);
      }
      return pending.get(id);
    }
  });
}
