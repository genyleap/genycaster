import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');
const script = read('ui/wasm/download/download.js');
const html = read('ui/wasm/download/index.html');
const css = read('ui/wasm/download/download.css');
const github = 'https://github.com/genyleap/genycaster/releases/download/v1.0.0/';
const releases = {
  windows:{x64:'windows-x86_64.exe',arm64:'windows-arm64.exe'},
  macos:{arm64:'macos-arm64-adhoc-UNNOTARIZED.dmg',x64:'macos-x86_64-adhoc-UNNOTARIZED.dmg'},
  linux:{x64:'linux-x86_64.AppImage',arm64:'linux-arm64.AppImage'},
  android:{arm64:'android-arm64-v8a.apk'}
};
const manifest = () => ({
  schema:1,
  platforms:Object.fromEntries(Object.entries(releases).map(([os,arches])=>
    [os,Object.fromEntries(Object.entries(arches).map(([arch,suffix])=>[arch,{
      sha256:'a'.repeat(64),version:'1.0.0',
      url:github+'GenyCaster-v1.0.0-'+suffix,
      ...(os==='macos'?{distribution:'ad-hoc-unnotarized'}:{})
    }]))]))
});

class Element {
  constructor() {
    this.dataset={};this.attrs={};this.children=[];this.listeners={};
    this.hidden=false;this.textContent='';this.href='';
    this.classList={toggle(){},add(){},remove(){}};
  }
  setAttribute(k,v){this.attrs[k]=v;}
  removeAttribute(k){delete this.attrs[k];if(k==='href')this.href='';}
  getAttribute(k){return this.attrs[k];}
  append(...elements){this.children.push(...elements);}
  replaceChildren(...elements){this.children=[...elements];}
  addEventListener(name,handler){this.listeners[name]=handler;}
  closest(selector){return selector.startsWith('button[')?this:null;}
}

function harness({os='Windows NT 10.0',data=manifest(),fail=false}={}) {
  const ids = Object.fromEntries([
    'language','selected-name','selected-icon','selected-description',
    'detected','detected-text','arch-block','architectures','arch-tip',
    'platform-list','release-bullet','release-label','format-text','arch-text',
    'download-primary','primary-text','download-pending','alternatives','trust-notice','trust-text'
  ].map(k=>[k,new Element()]));
  const tabs=Object.keys({windows:1,macos:1,linux:1,android:1,ios:1}).map(os=>{
    const e=new Element();e.dataset.platform=os;return e;
  });
  const doc={
    documentElement:{lang:'en',dir:'ltr'},
    getElementById(id){return ids[id]||null;},
    querySelectorAll(selector) {
      if(selector==='[data-platform]')return tabs;
      if(selector==='[data-i18n]')return [];
      return [];
    },
    createElement(){return new Element();}
  };
  const stored = new Map();
  const localStorage={getItem:k=>stored.get(k),setItem:(k,v)=>stored.set(k,v)};
  const navigator={userAgent:os,platform:os,language:'en',maxTouchPoints:0};
  const fetch=async()=>{if(fail)throw Error('offline');return {
    ok:true,headers:{get:()=>null},text:async()=>JSON.stringify(data)
  }};
  vm.runInNewContext(script,{document:doc,navigator,localStorage,fetch,URL,console});
  return {
    ids,tabs,doc,
    async ready(){await new Promise(resolve=>setImmediate(resolve));},
    choose(os){const target=tabs.find(t=>t.dataset.platform===os);
      ids['platform-list'].listeners.click({target});},
    arch(name){const target=ids.architectures.children.find(c=>c.dataset.arch===name);
      assert.ok(target, 'missing '+name);ids.architectures.listeners.click({target});},
    language(){ids.language.listeners.click();}
  };
}

test('dedicated canonical page is accessible, responsive, bilingual and standalone',()=>{
  assert.match(html,/<html lang="en" dir="ltr">/);
  assert.match(html,/<link rel="canonical" href="https:\/\/genycaster.xyz\/download">/);
  assert.match(html,/id="download-primary"/);
  assert.match(html,/id="platform-list"/);
  assert.match(html,/id="language"/);
  assert.match(html,/https:\/\/github.com\/genyleap\/genycaster\/releases\/download\/v1.0.0\/SHA256SUMS/);
  assert.match(css,/@media\(max-width:690px\)/);
  assert.match(css,/html\[dir="rtl"\]/);
  assert.match(css,/corner-shape:superellipse\(4\)/);
  assert.match(script,/fetch\("\/releases\/native.json"/);
  assert.match(script,/credentials:"omit",redirect:"error"/);
  assert.match(script,/record.url !== exact/);
  assert.match(script,/ad-hoc-unnotarized/);
  assert.doesNotMatch(html,/\.ipa\b/i);
});

test('all 7 native architecture links resolve to the exact release filenames', async()=>{
  const h=harness();await h.ready();
  for(const [os,arches] of Object.entries(releases)){
    h.choose(os);
    for(const [arch,suffix] of Object.entries(arches)){
      h.arch(arch);
      assert.equal(h.ids['download-primary'].hidden,false,`${os} ${arch}`);
      assert.equal(h.ids['download-primary'].href,
        github+'GenyCaster-v1.0.0-'+suffix);
    }
  }
  h.choose('macos');assert.equal(h.ids['trust-notice'].hidden,false);
  assert.match(h.ids['trust-text'].textContent,/notarized/);
  h.choose('linux');
  assert.equal(h.ids.alternatives.hidden,false);
  assert.ok(h.ids.alternatives.children.some(x=>x.href?.endsWith('.deb')));
  h.choose('ios');
  assert.equal(h.ids['download-primary'].hidden,true);
  assert.match(h.ids['download-pending'].textContent,/No iOS release/);
});

test('browser detection and RTL changes never suppress architecture controls',async()=>{
  const h=harness({os:'Android 15; Linux'});await h.ready();
  assert.equal(h.ids['selected-name'].textContent,'Android');
  assert.equal(h.ids['download-primary'].hidden,false);
  h.language();
  assert.equal(h.doc.documentElement.lang,'fa');
  assert.equal(h.doc.documentElement.dir,'rtl');
  assert.match(h.ids['primary-text'].textContent,/دریافت/);
  h.choose('windows');h.arch('arm64');
  assert.match(h.ids['download-primary'].href,/windows-arm64.exe$/);
});

test('malicious, absent or malformed catalogs never create installer actions',async()=>{
  const bad=manifest();
  bad.platforms.windows.x64.url='https://evil.example/download.exe';
  const h=harness({data:bad});await h.ready();
  assert.equal(h.ids['download-primary'].hidden,true);
  h.arch('arm64');
  assert.equal(h.ids['download-primary'].hidden,false);
  const offline=harness({fail:true});await offline.ready();
  assert.equal(offline.ids['download-primary'].hidden,true);
  assert.match(offline.ids['download-pending'].textContent,/verification unavailable/);
});
