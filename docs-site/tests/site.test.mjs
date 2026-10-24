import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync,existsSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {fileURLToPath} from 'node:url';
const root=resolve(dirname(fileURLToPath(import.meta.url)),'..');
const paths=['/','/quickstart/','/architecture/','/providers/','/client-switching/','/build-your-own/','/operations/','/status/'];
const file=(p)=>resolve(root,p.replace(/^\//,''),'index.html');
const read=(p)=>readFileSync(file(p),'utf8');
test('all public documentation pages exist and have canonical URLs',()=>{
 for(const url of paths){
  const data=read(url);
  assert.match(data,/<main id="content" class="main">/);
  assert.ok(data.includes('href="https://docs.genycaster.xyz'+url+'"'),url);
  assert.match(data,/<meta name="description"/);
  assert.match(data,/<script defer src="\/assets\/site.js">/);
  assert.doesNotMatch(data,/\.wasm|window\.qtLoader|genycaster-loader\.js|farcaster_client\.js/);
 }
});
test('no broken internal documentation navigation links',()=>{
 for(const url of paths){
  const data=read(url);
  for(const m of data.matchAll(/href="(\/[^"#?]*)"/g)){
   const href=m[1];
   if(href==='/llms.txt' || href==='/robots.txt'||href==='/sitemap.xml'){assert.ok(existsSync(resolve(root,href.slice(1))),href);continue;}
   if(href.startsWith('/assets/')){assert.ok(existsSync(resolve(root,href.slice(1))),href);continue;}
   assert.ok(paths.includes(href)||existsSync(file(href)),url+' -> '+href);
  }
 }
});
test('architecture contains honest routing and write flow diagrams',()=>{
 for(const needle of ['Capability router','Raw Snapchain','Indexed social API','Validator','Canonical domain','Durable outbox'])
  assert.ok(read('/architecture/').includes(needle),needle);
 assert.match(read('/quickstart/'),/three-command.*not yet|three-command.*not yet|no audited, verified Genycaster command sequence/i);
 assert.match(read('/quickstart/'),/Balaji/);
 assert.match(read('/status/'),/Not yet verified/);
 assert.match(read('/client-switching/'),/Client.*protocol|protocol.*identit/i);
});
test('static docs do not rely on remote assets or expose server secrets',()=>{
 const script=readFileSync(resolve(root,'assets/site.js'),'utf8');
 const css=readFileSync(resolve(root,'assets/site.css'),'utf8');
 assert.doesNotMatch(script,/fetch\(|XMLHttpRequest|localStorage|sessionStorage|eval\(/);
 assert.match(css,/@media\(max-width:800px\)/);
 assert.match(css, /prefers-reduced-motion/);
 assert.ok(existsSync(resolve(root,'llms.txt')));
 assert.ok(existsSync(resolve(root,'sitemap.xml')));
});
