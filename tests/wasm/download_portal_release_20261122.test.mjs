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
    this.hidden=false;this.textContent='';this.href='';this.clientWidth=145;this.clientHeight=77;
    this.classList={toggle(){},add(){},remove(){}};
  }
  setAttribute(k,v){this.attrs[k]=v;}
  querySelector(selector){return selector==="[data-version]"?this.versionLabel:null;}
  removeAttribute(k){delete this.attrs[k];if(k==='href')this.href='';}
  getAttribute(k){return this.attrs[k];}
  append(...elements){this.children.push(...elements);}
  prepend(...elements){this.children.unshift(...elements);}
  replaceChildren(...elements){this.children=[...elements];}
  addEventListener(name,handler){this.listeners[name]=handler;}
  closest(selector){return selector.startsWith('button[')?this:null;}
}

function harness({os='Windows NT 10.0',data=manifest(),fail=false,withShapes=false}={}) {
  const ids = Object.fromEntries([
    'selected-name','selected-icon','selected-description','selected-version','release-number','file-name','file-hash',
    'detected','detected-text','arch-block','architectures','arch-tip',
    'platform-list','release-bullet','release-label','format-text','arch-text',
    'download-primary','primary-text','download-pending','alternatives','trust-notice','trust-text'
  ].map(k=>[k,new Element()]));
  const tabs=Object.keys({windows:1,macos:1,linux:1,android:1,ios:1}).map(os=>{
    const e=new Element();e.dataset.platform=os;
    if(os!=="ios")e.versionLabel=new Element();
    return e;
  });
  const doc={
    documentElement:{lang:'en',dir:'ltr'},
    getElementById(id){return ids[id]||null;},
    querySelectorAll(selector) {
      if(selector==='[data-platform]' || (withShapes && selector==='.platform'))return tabs;
      if(withShapes && selector==='.arch-button')return ids.architectures.children;
      if(withShapes && selector==='.primary-download')return [ids['download-primary']];
      return [];
    },
    createElement(){return new Element();},
    ...(withShapes?{createElementNS(){return new Element();}}:{})
  };
  // English-only download page has no language storage.
  const navigator={userAgent:os,platform:os,language:'en',maxTouchPoints:0};
  const fetch=async()=>{if(fail)throw Error('offline');return {
    ok:true,headers:{get:()=>null},text:async()=>JSON.stringify(data)
  }};
  vm.runInNewContext(script,{document:doc,navigator,fetch,URL,console,window:{devicePixelRatio:2}});
  return {
    ids,tabs,doc,
    async ready(){await new Promise(resolve=>setImmediate(resolve));},
    choose(os){const target=tabs.find(t=>t.dataset.platform===os);
      ids['platform-list'].listeners.click({target});},
    arch(name){const target=ids.architectures.children.find(c=>c.dataset.arch===name);
      assert.ok(target, 'missing '+name);ids.architectures.listeners.click({target});},
  };
}

test('dedicated canonical page is compact, English-only, and uses native geometry',()=>{
  assert.match(html,/<html lang="en" dir="ltr">/);
  assert.match(html,/<link rel="canonical" href="https:\/\/genycaster.xyz\/download">/);
  assert.match(html,/id="download-primary"/);
  assert.match(html,/id="platform-list"/);
  assert.doesNotMatch(html,/id="language"/);
  assert.doesNotMatch(html,/data-i18n/);
  assert.doesNotMatch(html,/hero-orbit|hero-art|orbit-pip/);
  assert.match(html,/class="platform-version" data-version/);
  assert.match(html,/id="selected-version"/);
  assert.match(html,/https:\/\/github.com\/genyleap\/genycaster\/releases\/download\/v1.0.0\/SHA256SUMS/);
  assert.match(css,/@media\(max-width:690px\)/);
  assert.doesNotMatch(css,/html\[dir="rtl"\]/);
  assert.doesNotMatch(css,/radial-gradient|--glow|box-shadow:0 0/);
  assert.match(css,/\.gs-shaped > svg\.gs-shape/);
  assert.match(script,/function superellipsePath\(width, height, depth\)/);
  assert.match(script,/const diagonal = Math\.pow\(\.5, 1 \/ safeExponent\)/);
  assert.match(script,/\(8 \* diagonal - 4\) \/ 3/);
  assert.doesNotMatch(script,/storedLanguage|setLanguage\(|localStorage|fa:\s*\{/);
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
      assert.equal(h.ids['selected-version'].textContent,'v1.0.0');
      assert.match(h.ids['primary-text'].textContent,/v1.0.0/);
      assert.match(h.ids['file-name'].textContent,/^GenyCaster-v1.0.0-/);
      assert.match(h.ids['file-hash'].textContent,/^a{16}…$/);
      assert.equal(h.tabs.find(t=>t.dataset.platform===os).versionLabel.textContent,'v1.0.0');
    }
  }
  h.choose('macos');assert.equal(h.ids['trust-notice'].hidden,false);
  assert.match(h.ids['trust-text'].textContent,/notarized/);
  h.choose('linux');
  assert.equal(h.ids.alternatives.hidden,false);
  assert.ok(h.ids.alternatives.children.some(x=>x.href?.endsWith('.deb')));
  h.choose('ios');
  assert.equal(h.ids['selected-version'].textContent,'Not available');
  assert.equal(h.ids['download-primary'].hidden,true);
  assert.match(h.ids['download-pending'].textContent,/No iOS release/);
});

test('browser detection and architecture controls work without language switching',async()=>{
  const h=harness({os:'Android 15; Linux'});await h.ready();
  assert.equal(h.ids['selected-name'].textContent,'Android');
  assert.equal(h.ids['download-primary'].hidden,false);
  assert.equal(h.doc.documentElement.lang,'en');
  assert.equal(h.doc.documentElement.dir,'ltr');
  assert.match(h.ids['primary-text'].textContent,/v1.0.0/);
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

test('legacy /download redirect forwards only the exact downloads fragment', () => {
  const forward = read('ui/wasm/download/about-forward.js');
  assert.match(forward,/location\.replace\("\/download\/index.html"\)/);
  const calls = [];
  const listeners = {};
  const location = {
    pathname:'/about/',hash:'#downloads',
    replace(url){calls.push(url);}
  };
  vm.runInNewContext(forward,{location,addEventListener(name,fn){listeners[name]=fn;}});
  assert.deepEqual(calls,['/download/index.html']);
  location.hash = '#explore';
  listeners.hashchange();
  assert.equal(calls.length,1);
  location.pathname = '/download/index.html';
  location.hash = '#downloads';
  listeners.hashchange();
  assert.equal(calls.length,1);
});

test('download sources survive WASM publication and preserve primary API routing',()=>{
  const packer=read('ci/package_wasm_assets.py');
  for(const file of ['download/index.html','download/download.css',
                     'download/download.js','download/about-forward.js'])
    assert.equal(packer.split('"'+file+'"').length,3);
  const nginx=read('ci/primary_genycaster_nginx.conf');
  assert.match(nginx,/location = \/download \{/);
  assert.match(nginx,/try_files \/download\/index.html =404/);
  assert.match(nginx,/location \^~ \/api\/v1\/cloud\//);
  const deploy=read('ci/deploy_download_page.py');
  assert.match(deploy,/deploy_legacy_route/);
  assert.match(deploy,/--legacy-route/);
  assert.match(deploy,/if path.parent == SITE \/ "download"/);
});

test('Qt exponent-4 SVG outline mounts once per platform and updates architecture controls',async()=>{
  const h=harness({withShapes:true});
  await h.ready();
  const tab=h.tabs.find(t=>t.dataset.platform==='windows');
  const outline=tab.children[0];
  assert.equal(outline.attrs['aria-hidden'],'true');
  assert.equal(outline.attrs['preserveAspectRatio'],'none');
  assert.equal(outline.children.length,1);
  const d=outline.children[0].attrs.d;
  assert.match(d,/^M /);
  assert.match(d,/ C /);
  assert.match(d,/ Z$/);
  assert.equal(outline.attrs.viewBox,'0 0 145 77');
  h.choose('macos');
  h.choose('windows');
  assert.equal(tab.children[0],outline,'SVG borders must not duplicate');
  assert.equal(tab.children.length,1);
  const arch=h.ids.architectures.children.find(t=>t.dataset.arch==='arm64');
  assert.ok(arch.children[0]?.attrs.viewBox,'new architecture options must receive paths');
});
