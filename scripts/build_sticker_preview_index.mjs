import fs from 'node:fs';
import vm from 'node:vm';
import path from 'node:path';

const root=path.resolve(import.meta.dirname,'..');
const input=fs.readFileSync(path.join(root,'ui/components/composer/StickerCatalog.js'),'utf8')
  .replace(/^\.pragma library\s*/,'');
const sandbox=vm.createContext({});
vm.runInContext(input,sandbox,{timeout:1000});
const stickers=Array.from(sandbox.stickers||[]);
if(stickers.length!==276)throw new Error('Expected pinned 276 sticker entries');
const out={version:1,items:{}};
for(const item of stickers){
  if(!/^[a-z0-9-]{2,125}$/.test(item.id) || out.items[item.id]
    || !/^[0-9a-f]{64}$/.test(item.sha256))
    throw new Error('Invalid sticker metadata');
  const source=item.pack==='cream-cat-pro' ? item.thumbnailUrl : item.url;
  const url=new URL(source);
  if(url.protocol!=='https:' || url.search || url.hash || url.port
    || !['mssgs-stickers.com','red-geographical-clownfish-714.mypinata.cloud'].includes(url.hostname))
    throw new Error('Untrusted sticker host');
  out.items[item.id]={source,pack:item.pack,sha256:item.pack==='njal'?item.sha256:null};
}
const target=path.join(root,'services/media-gateway/sticker_preview_index.json');
fs.writeFileSync(target,JSON.stringify(out,null,2)+'\n');
console.log('Generated pinned, metadata-only sticker preview index',stickers.length);
