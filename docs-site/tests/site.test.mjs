import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync, existsSync} from 'node:fs';
import {resolve, dirname} from 'node:path';
import {fileURLToPath} from 'node:url';

const root=resolve(dirname(fileURLToPath(import.meta.url)),'..');
const paths=['/','/quickstart/','/architecture/','/providers/','/client-switching/','/build-your-own/','/operations/','/status/'];
const file=(p)=>resolve(root,p.replace(/^\//,''),'index.html');
const read=(p)=>readFileSync(file(p),'utf8');

test('all documentation pages have their own working canonical URLs',()=>{
 for(const url of paths){
  const data=read(url);
  assert.match(data,/<main id="content" class="main">/);
  assert.ok(data.includes('href="https://docs.genycaster.xyz'+url+'"'),url);
  assert.match(data,/<meta name="description"/);
  assert.match(data,/<script defer src="\/assets\/site.js">/);
  assert.doesNotMatch(data,/\.wasm|window\.qtLoader|genycaster-loader\.js|farcaster_client\.js/);
 }
});
test('navigation links target existing documentation content',()=>{
 for(const url of paths){
  for(const m of read(url).matchAll(/href="(\/[^"#?]*)"/g)){
   const href=m[1];
   if(['/llms.txt','/robots.txt','/sitemap.xml'].includes(href)||href.startsWith('/assets/')){
    assert.ok(existsSync(resolve(root,href.slice(1))),href);continue;
   }
   assert.ok(paths.includes(href)||existsSync(file(href)),url+' -> '+href);
  }
 }
});
test('architecture diagrams cover local state, protocol reads and validator writes',()=>{
 const arch=read('/architecture/');
 for(const needle of ['Capability router','Raw Snapchain','Indexed social API','Validator','Canonical domain','Durable outbox']){
  assert.ok(arch.includes(needle),needle);
 }
 assert.match(read('/providers/'),/snapchain-v1/);
 assert.match(read('/client-switching/'),/FID/);
});
test('getting started and status are focused on Genycaster, not third-party competitions',()=>{
 const guide=read('/quickstart/'),status=read('/status/');
 assert.match(guide,/Settings → Node management/);
 assert.match(guide,/Add read node/);
 assert.match(status,/Implementation matrix/);
 for(const html of paths.map(read)){
  assert.doesNotMatch(html,/Balaji|bounty|prize eligibility|\$2048|three-command challenge/i);
 }
});
test('documentation is static and separate from the app',()=>{
 const script=readFileSync(resolve(root,'assets/site.js'),'utf8');
 const css=readFileSync(resolve(root,'assets/site.css'),'utf8');
 assert.doesNotMatch(script,/fetch\(|XMLHttpRequest|localStorage|sessionStorage|eval\(/);
 assert.match(css,/@media\(max-width:800px\)/);
 assert.match(css,/prefers-reduced-motion/);
 for(const name of ['llms.txt','robots.txt','sitemap.xml']){
  assert.ok(existsSync(resolve(root,name)));
 }
});
