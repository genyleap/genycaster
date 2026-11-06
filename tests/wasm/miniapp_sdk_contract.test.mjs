import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

const read = path => readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const source=read('ui/wasm/miniapp-host-pre.js');
const bridge=read('ui/wasm/miniapp-host-bridge-entry.mjs');
const html=read('ui/wasm/genycaster-shell.html');
const packer=read('ci/package_wasm_assets.py');

function hostBrowser(sdkInitially=true) {
  const nodes=[], attached=[], detached=[];
  class Element {
    constructor(tag){this.tagName=tag;this.style={};this.attributes={};
      this.children=[];this.handlers={};this.isConnected=false;}
    setAttribute(key,value){this.attributes[key]=value;}
    append(...values){this.children.push(...values);}
    remove(){this.isConnected=false;}
    addEventListener(type,handler){this.handlers[type]=handler;}
    focus(){}
  }
  const document={createElement:name=>{const node=new Element(name);nodes.push(node);return node;},
    body:{append(node){node.isConnected=true;}}};
  const window={location:{origin:'https://caster.genyleap.com'},innerWidth:1200};
  const sdk={attach(frame,url,user,actions){
    const instance={frame,url,user,actions};attached.push(instance);
    const detach=()=>detached.push(instance);
    detach.updateUser=next=>{
      if(Number(next.fid)===Number(instance.user?.fid))
        instance.user=next;
    };
    return detach;
  }};
  if(sdkInitially)window.__genycasterMiniAppSdk=sdk;
  vm.runInNewContext(source,{window,document,URL,navigator:{language:'en'},
    setTimeout:()=>1,clearTimeout:()=>{}});
  return {sdk,window,nodes,attached,detached,host:window.__genycasterMiniAppOverlay};
}

test('Official host is served from the GenyCaster origin and packaged with release integrity',()=>{
  assert.match(bridge,/import \{exposeToIframe\} from '@farcaster\/miniapp-host'/);
  assert.match(bridge,/miniAppOrigin:url\.origin/);
  assert.match(bridge,/user: identity\(verifiedUser\)/);
  assert.match(bridge,/getCapabilities: async/);
  assert.match(bridge,/ethProviderRequest: unsupported/);
  assert.match(bridge,/signIn: unsupported/);
  assert.match(html,/type="module" src="miniapp-host-bridge\.mjs"/);
  assert.match(packer,/"miniapp-host-bridge\.mjs"/);
  assert.match(read('ui/wasm/host-sdk/package-lock.json'),/"@farcaster\/miniapp-host": "0\.4\.0"/);
});

test('An authenticated Mini App frame receives an origin-specific host and is cleaned on close',()=>{
  const {host,attached,detached}=hostBrowser();
  host.setUser({fid:42,username:'builder'});
  assert.equal(host.open('https://tasks.miniapps.genyleap.com/','GenyTask'),true);
  assert.equal(attached.length,1);
  assert.equal(attached[0].url,'https://tasks.miniapps.genyleap.com/');
  assert.equal(attached[0].user.fid,42);
  attached[0].actions.ready();
  host.close();
  assert.equal(detached.length,1);
  assert.equal(host.active(),false);
});

test('SDK initializes for frames opened before the host module arrived',()=>{
  const {host,attached,detached,window,sdk}=hostBrowser(false);
  assert.equal(host.open('https://tasks.miniapps.genyleap.com/','GenyTask'),true);
  assert.equal(attached.length,0);
  window.__genycasterMiniAppSdk=sdk;
  host.enableSdk();
  host.enableSdk();
  assert.equal(attached.length,1);
  attached[0].actions.close();
  assert.equal(detached.length,1);
  assert.equal(host.active(),false);
});

test('Cross-origin sessions are isolated and same-origin duplicate frames are not exposed',()=>{
  const {host,attached,detached}=hostBrowser();
  host.setLimit(4);
  assert.equal(host.open('https://tasks.miniapps.genyleap.com/a','GenyTask A'),true);
  assert.equal(host.open('https://tasks.miniapps.genyleap.com/b','GenyTask B'),true);
  assert.equal(attached.length,1);
  assert.equal(host.open('https://other.example.com/','Other'),true);
  assert.equal(attached.length,2);
  host.close();
  assert.equal(detached.length,2);
});


test('SDK user context preserves a safe public avatar and updates it only for the same FID', () => {
  const captures = [];
  class HTMLIFrameElement {
    constructor(src) { this.src = src; }
  }
  const window = {location:{origin:'https://caster.genyleap.com'}};
  // Substitute only the SDK adapter functions; execute the production
  // identity normalization and same-account refresh flow.
  const moduleSource = bridge.replace(/^import .*;\s*$/gm, '');
  const exposeToIframe = args => {
    captures.push(args.sdk);
    return {cleanup(){}};
  };
  vm.runInNewContext(moduleSource, {
    window, URL, HTMLIFrameElement,
    exposeToIframe, createMiniAppEthereumProvider: () => null
  }, {timeout:1000});
  const attach = window.__genycasterMiniAppSdk.attach;
  const frame = new HTMLIFrameElement('https://mini.example.com/');
  const close = attach(frame, frame.src, {
    fid:42, username:'builder',
    pfpUrl:'https://images.example.com/portrait.png'
  });
  assert.equal(captures.length, 1);
  assert.equal(captures[0].context.user.pfpUrl,
    'https://images.example.com/portrait.png');

  close.updateUser({fid:42, username:'builder',
    pfpUrl:'https://images.example.com/refreshed.png'});
  assert.equal(captures[0].context.user.pfpUrl,
    'https://images.example.com/refreshed.png');
  close.updateUser({fid:43, pfpUrl:'https://evil.example.com/other.png'});
  assert.equal(captures[0].context.user.fid,42);
  assert.equal(captures[0].context.user.pfpUrl,
    'https://images.example.com/refreshed.png');

  close.updateUser({fid:42,pfpUrl:'http://images.example.com/insecure.png'});
  assert.equal(captures[0].context.user.pfpUrl, undefined);
  close.updateUser({fid:42,pfpUrl:'https://private.local/avatar.png'});
  assert.equal(captures[0].context.user.pfpUrl, undefined);
  close.updateUser({fid:42,pfpUrl:'https://u:p@images.example.com/a.png'});
  assert.equal(captures[0].context.user.pfpUrl, undefined);
  close.updateUser({fid:42,pfpUrl:'https://images.example.com/new.png#fragment'});
  assert.equal(captures[0].context.user.pfpUrl,
    'https://images.example.com/new.png');
  close();
  close.updateUser({fid:42,pfpUrl:'https://images.example.com/after-close.png'});
  assert.equal(captures[0].context.user.pfpUrl,
    'https://images.example.com/new.png');
});

test('WASM overlays refresh avatar metadata without remounting the Mini App', () => {
  const {host,attached,detached} = hostBrowser();
  host.setUser({fid:42,username:'builder'});
  assert.equal(host.open('https://mini.example.com/','Portrait'),true);
  assert.equal(attached.length,1);
  assert.equal(detached.length,0);
  host.setUser({fid:42,username:'builder',
    pfpUrl:'https://images.example.com/avatar.png'});
  assert.equal(attached[0].user.pfpUrl,
    'https://images.example.com/avatar.png');
  assert.equal(detached.length,0);
  host.setUser({fid:43,username:'other',
    pfpUrl:'https://images.example.com/other.png'});
  assert.equal(attached[0].user.fid,42);
  assert.equal(attached[0].user.pfpUrl,
    'https://images.example.com/avatar.png');
  assert.match(source,/session\.detach\?\.updateUser\?\.\(next\)/);
  assert.match(bridge,/detach\.updateUser = nextValue =>/);
  assert.match(bridge,/next\.fid !== host\.context\.user\.fid/);
});


test('native SIWF is advertised only when a real wallet and verified account are available', () => {
  const runtime = read('ui/app/miniapp_runtime.cpp');
  const api = read('ui/app/miniapp_runtime.cppm');
  const composition = read('ui/app/main.cpp');
  const cmake = read('ui/CMakeLists.txt');
  const auth = read('src/providers/compatibility/farcaster_auth/auth.cpp');
  const contract = read('src/providers/compatibility/farcaster_auth/auth.cppm');
  const web = read('ui/wasm/miniapp-host-bridge-entry.mjs');

  assert.match(runtime, /if \(supportsSignIn && wallet != nullptr && wallet->available\(\)\)/);
  assert.match(runtime, /result\.push_back\(QStringLiteral\("actions\.signIn"\)\)/);
  assert.match(runtime, /m_controller\.user\(\)\.value\(QStringLiteral\("fid"\)\)/);
  assert.match(runtime, /capabilities\(m_wallet, signInAvailable\(\)\)/);
  assert.match(runtime, /beginSignIn\(session, id, argument, document\)/);
  assert.match(runtime, /"eth_requestAccounts"/);
  assert.match(runtime, /"personal_sign"/);
  assert.match(runtime, /QThreadPool::globalInstance\(\)->start/);
  assert.match(runtime, /verified && proof->authAddress/);
  assert.match(runtime, /generation != session->documentGeneration|session->documentGeneration != generation/);
  assert.match(runtime, /"rejected_by_user"/);
  assert.match(runtime, /"authMethod"/);
  assert.match(runtime, /"Chain ID: 10/);
  assert.match(runtime, /farcaster:\/\/fid\//);
  assert.match(api, /std::string optimismRpcUrl/);
  assert.match(composition, /optimismRpcUrl\.value_or/);
  assert.match(cmake, /GENYCASTER_MINIAPP_SIWF=1/);
  assert.match(cmake, /FARCASTER_ENABLE_COMPATIBILITY_PROVIDERS/);
  assert.match(contract, /verifyMiniAppSignIn/);
  assert.match(auth, /validateSiwe\(\*parsed, expectedDomain/);
  assert.match(auth, /verifier\.verify\(message, signature, parsed->address, fid/);
  assert.match(auth, /std::chrono::minutes\{10\}/);
  // A browser iframe must not receive a fabricated SIWF proof in a build
  // that has no verified signer authorization transport.
  assert.match(web, /signIn: unsupported/);
  assert.doesNotMatch(runtime, /snapchain|hypersnap/i);
});
