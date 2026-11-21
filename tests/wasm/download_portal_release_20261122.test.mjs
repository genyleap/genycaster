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

function harness({os='Windows NT 10.0',data=manifest(),fail=false,withShapes=false,route=null}={}) {
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
  const navigation=[];
  const location=route===null?undefined:{pathname:route,search:'?source=site',hash:'#platform'};
  const history=route===null?undefined:{
    state:{view:'download'},
    replaceState(state,title,path){navigation.push({state,title,path});}
  };
  vm.runInNewContext(script,{document:doc,navigator,fetch,URL,console,
    window:{devicePixelRatio:2},location,history});
  return {
    ids,tabs,doc,navigation,
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
  assert.match(html,/src="\/download\/download\.js\?v=3"/);
  assert.match(script,/history\.replaceState\(history\.state/);
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

test('Superellipse SVG must never cover bare labels, architecture text or badges',()=>{
  // The SVG must be painted below unwrapped text nodes, unlike the previous
  // zero-layer overlay that hid hero chips and the Apple Silicon/Intel labels.
  const rule=css.match(/\.gs-shaped > svg\.gs-shape \{([^}]+)\}/);
  assert.ok(rule, 'missing shared shape rule');
  assert.match(rule[1],/z-index:\s*-1\s*;/);
  assert.doesNotMatch(rule[1],/z-index:\s*0\s*;/);
  assert.match(css,/\.gs-shaped\s*\{[^}]*isolation:isolate/);
  assert.match(css,/\.gs-shaped > :not\(svg\.gs-shape\) \{position:relative;z-index:1\}/);
  assert.match(html,/class="version">v1\.0\.0 · Beta/);
  assert.match(html,/Version 1\.0\.0<\/span>/);
  assert.match(script,/button\.textContent = label/);
  assert.match(script,/label\.textContent = !loaded/);
});

test('download palette matches Theme.qml built-in Geny day and night colors',()=>{
  const theme=read('ui/theme/Theme.qml');
  const matches=[
    ['dayCanvas','--bg','#F1F1F3'],
    ['daySurface','--surface','#FFFFFF'],
    ['daySurfaceMuted','--soft','#F7F7F8'],
    ['dayInk','--ink','#17181B'],
    ['dayInkMuted','--muted','#60636A'],
    ['dayBorder','--border','#DFE1E5'],
    ['accentGenyDay','--accent','#315CF5'],
    ['nightCanvas','--bg','#0B0C0F'],
    ['nightSurface','--surface','#121317'],
    ['nightSurfaceMuted','--soft','#17191E'],
    ['nightInk','--ink','#F7F7F8'],
    ['nightInkMuted','--muted','#AFB2BA'],
    ['nightBorder','--border','#292C33'],
    ['accentGenyNight','--accent','#829AFF']
  ];
  for (const [token,variable,value] of matches) {
    assert.ok(theme.includes(token+': "'+value+'"'),token+' changed upstream');
    assert.ok(css.includes(variable+':'+value+';'),variable+' does not match '+token);
  }
  assert.match(html,/download\.css\?v=3/);
});

test('download page always presents the canonical clean URL without index.html', async()=>{
  for (const route of ['/download/index.html','/download/','/download']) {
    const h=harness({route});
    await h.ready();
    const expected=route==='/download'?[]:[{
      state:{view:'download'}, title:'', path:'/download?source=site#platform'
    }];
    assert.deepEqual(h.navigation,expected,route);
    assert.equal(h.ids['download-primary'].hidden,false,'download must keep working');
  }
  const nginx=read('ci/primary_genycaster_nginx.conf');
  assert.match(nginx,/location = \/download \{/);
  assert.match(nginx,/location = \/download\/ \{ return 308 \/download; \}/);
  assert.match(nginx,/location = \/download\/index\.html \{ return 308 \/download; \}/);
});

test('legacy redirect mounts download without painting About or navigating again',async()=>{
  const forward = read('ui/wasm/download/about-forward.js');
  assert.match(forward,/root\.style\.visibility = "hidden"/);
  assert.match(forward,/document\.body\.replaceWith/);
  assert.match(forward,/history\.replaceState\(history\.state, "", "\/download"\)/);
  const navigations=[];
  const replaceCalls=[];
  const root={
    style:{visibility:''},
    setAttribute(){},
    removeAttribute(){}
  };
  const listener={};
  const imported={};
  let replaced=false;
  const previousStyle={remove(){}};
  const doc={
    readyState:'loading',documentElement:root,
    addEventListener(name,fn){listener[name]=fn;},
    querySelectorAll(){return [previousStyle];},
    querySelector(){return null;},
    createElement(tag){return {tag};},
    importNode(value){return value;},
    body:{replaceWith(value){replaced=value===imported;}},
    head:{appendChild(element){
      if(element.tag==='link')queueMicrotask(()=>element.onload?.());
      if(element.tag==='script')queueMicrotask(()=>element.onload?.());
    }}
  };
  const parsed={
    body:imported,title:'Download GenyCaster',
    documentElement:{lang:'en'},
    getElementById(id){return ['download-primary','platform-list'].includes(id)?{}:null;},
    querySelector(selector){
      if(selector==='link[rel="stylesheet"]')return {getAttribute:()=>'/download/download.css?v=3'};
      if(selector.startsWith('script['))return {getAttribute:()=>'/download/download.js?v=3'};
      return null;
    }
  };
  const location={
    pathname:'/about/',hash:'#downloads',
    replace(url){replaceCalls.push(url);}
  };
  const history={
    state:{view:'download'},
    replaceState(state,title,url){navigations.push(url);}
  };
  vm.runInNewContext(forward,{
    location,document:doc,history,DOMParser:class{parseFromString(){return parsed;}},
    fetch:async()=>({ok:true,text:async()=>'<html id="download-primary"></html>'}),
    queueMicrotask,console
  });
  assert.equal(root.style.visibility,'hidden','hide About before first render');
  assert.equal(root.style.opacity,'0','never allow child styles to override hidden route');
  assert.deepEqual(navigations,['/download'],'rewrite URL before any paint or DOM fetch');
  assert.equal(typeof listener.DOMContentLoaded,'function');
  listener.DOMContentLoaded();
  await new Promise(resolve=>setImmediate(resolve));
  assert.equal(replaced,true,'download body mounted');
  assert.equal(root.style.visibility,'','download made visible');
  assert.equal(root.style.opacity,'','download became opaque');
  assert.deepEqual(navigations,['/download'],'only history rewrite, no second navigation');
  assert.deepEqual(replaceCalls,[],'should not open /download/index.html');
  assert.equal(doc.title,'Download GenyCaster');
  assert.equal(root.dir,'ltr');

  const untouched={pathname:'/about/',hash:'#experience'};
  vm.runInNewContext(forward,{location:untouched});
  assert.equal(untouched.hash,'#experience','ordinary About unaffected');
});

test('legacy bootstrap loads in head before About scripts and never defers',()=>{
  const deploy=read('ci/deploy_download_page.py');
  assert.match(deploy,/script_tag = '<script src="\/download\/about-forward\.js\?v=3"><\/script>'/);
  assert.match(deploy,/after = after\.replace\(marker, marker \+ chr\(10\)/);
  assert.match(deploy,/before\.replace\(legacy_tag, ""\)\.replace\(prior_tag, ""\)/);
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
