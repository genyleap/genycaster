import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import test from 'node:test';

// Runtime binding functions live in a separate Qt composition unit to keep
// GCC C++26 module compilation bounded; test both halves as one read pipeline.
function readGenycasterMain() {
  return readFileSync(new URL('../../ui/app/main.cpp', import.meta.url), 'utf8') +
    '\n' + readFileSync(new URL('../../ui/app/runtime/bindings.cpp', import.meta.url), 'utf8');
}
const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path,root),'utf8');

test('log regressions: Avatar tolerates null resolver, GIF decoding has no obsolete Qt.atob',()=>{
 const avatar=read('ui/components/content/Avatar.qml');
 const image=read('ui/components/visual/SuperellipseImage.qml');
 assert.match(avatar,/typeof linkPreviewResolver !== "undefined"/);
 assert.match(avatar,/linkPreviewResolver !== null/);
 assert.doesNotMatch(image,/mediaUrl\s*=\s*Qt\.atob\(/);
 assert.match(image,/function decodeBase64Url\(input\)/);
 const match=/function decodeBase64Url\(input\) \{([\s\S]*?)\n    \}/.exec(image);
 assert.ok(match);
 const decoder=Function('input','const f=function(input) {'+match[1]+'};return f(input)');
 for(const url of ['https://atgif.com/funny.gif',
   'https://media-farcaster.genyleap.com/m/v1/logo.gif?x=1',
   'https://site.example/emoji-💠.gif']) {
   assert.equal(decoder(Buffer.from(url,'utf8').toString('base64url')),url);
 }
 assert.equal(decoder('@@@'),'');
});
test('browser native HLS modal and local hls.js work without third-party runtime CDN dependency',()=>{
 const qml=read('ui/components/media/VideoPreview.qml');
 const player=read('src/presentation/qt/media_playback_coordinator.cpp');
 const header=read('src/presentation/qt/media_playback_coordinator.hpp');
 const js=read('ui/wasm/browser-hls-pre.js');
 const cmake=read('ui/CMakeLists.txt');
 const packageScript=read('ci/package_wasm_assets.py');
 assert.match(qml,/mediaPlayback\.openBrowserHls\(candidate\)/);
 assert.match(header,/Q_INVOKABLE bool openBrowserHls\(const QString& source\) const/);
 assert.match(player,/__genycasterBrowserHls\?\.open/);
 assert.match(js,/video\.canPlayType\("application\/vnd\.apple\.mpegurl"\)/);
 assert.match(js,/new Hls\(/);
 assert.match(js,/vendor\/hls-1\.6\.15\.min\.js/);
 assert.match(cmake,/browser-hls-pre\.js/);
 assert.match(packageScript,/vendor\/hls-1\.6\.15\.min\.js/);
 const main=readGenycasterMain();
 assert.match(main,/path\(\)\.endsWith\(QLatin1StringView\{"\.m3u8"\}, Qt::CaseInsensitive\)/);
 assert.match(main,/dispatch\(true\)/);
});
test('home composer focus is visible and link artwork leads the responsive editorial card',()=>{
 const home=read('ui/components/home/HomeDashboard.qml');
 const link=read('ui/components/media/LinkPreview.qml');
 assert.match(home,/desktopComposerSurface[\s\S]*borderColor: desktopDraftEditor\.activeFocus \? Theme\.accent/);
 assert.match(home,/mobileComposerSurface[\s\S]*curveDepth: Theme\.curveModal/);
 assert.match(link,/localizationController\.isRightToLeftText\(root\.castCaption\)/);
 assert.match(link,/Layout\.preferredHeight: !shouldReserve \? 0[\s\S]*?Math\.min\(root\.compact \? 300 : Theme\.scaleUi\(192\)/);
});
test('single photos and browser galleries keep alpha masks across resizing',()=>{
 const gallery=read('ui/components/media/GalleryLayout.qml');
 const tile=read('ui/components/media/GalleryTile.qml');
 assert.match(gallery,/useLayeredMask: Qt\.platform\.os === "wasm"/);
 assert.match(gallery,/useLayeredMask: Qt\.platform\.os === "wasm"\s*\|\| root\.visibleCount === 1/);
 assert.match(tile,/clip: true/);
});


test('wallet approval stays steady after consent and stops polling on session end',()=>{
 const bridge=read('src/providers/compatibility/browser_wallet/bridge.cpp');
 assert.match(bridge,/box\.hidden=!rpc\.requiresApproval/);
 assert.match(bridge,/response\.status===404\|\|response\.status===410/);
 assert.match(bridge,/window\.addEventListener\('pagehide',stopPolling/);
 assert.match(bridge,/if\(!lastConsentResultShown\)/);
 assert.match(bridge,/known->second != announced/);
 // The bridge page's inline JavaScript is a C++ raw string with only the
 // base path injected by native code. Parse the actual client script as JS.
 const start=bridge.indexOf('const base=")html";');
 const finish=bridge.indexOf('</script></main></body></html>)html";',start);
 assert.ok(start>0 && finish>start);
 const js=bridge.slice(start,finish).replace(
   'const base=")html";\n    page += basePath;\n    page += R"html(";',
   'const base="/session/test";');
 assert.doesNotThrow(()=>new Function(js));
});

test('failed private media origins back off and tiny SVGs avoid uninitialized mipmaps',()=>{
 const image=read('ui/app/media/remote_image.cpp');
 assert.match(image,/originTemporarilyForbidden\(urlText\)/);
 // Force-download artwork shares the 'manual' descriptor, so authorization
 // refusal must be honored regardless of that flag.
 assert.match(image,/if \(originTemporarilyForbidden\(urlText\)\)/);
 assert.doesNotMatch(image,/if \(!manual && originTemporarilyForbidden\(urlText\)\)/);
 assert.match(image,/forbiddenOriginBackoff = std::chrono::minutes\{10\}/);
 assert.match(image,/result->statusCode == 401U \|\| result->statusCode == 403U/);
 assert.match(image,/!deniedByOrigin && !gatewayFallbackAttempt/);
 assert.match(image,/clearForbiddenOrigin\(urlText\)/);
 for(const file of ['ui/components/content/MarketAssetIcon.qml','ui/components/home/HomeDashboard.qml']){
   const qml=read(file);
   assert.doesNotMatch(qml,/mipmap:\s*true/);
 }
});


test('consent page keeps a confirmed approval steady through a follow-up read and session close',async()=>{
 const bridge=read('src/providers/compatibility/browser_wallet/bridge.cpp');
 const a=bridge.indexOf('const base=")html";');
 const b=bridge.indexOf('</script></main></body></html>)html";',a);
 assert.ok(a>=0 && b>a);
 const script=bridge.slice(a,b).replace(
   'const base=")html";\n    page += basePath;\n    page += R"html(";','const base="/session/test";');
 const elements={};
 const fakeNode=()=>({
   textContent:'',className:'',hidden:false,disabled:false,children:[],
   listeners:{},dataset:{},attrs:{},tabIndex:0,
   setAttribute(name,value){this.attrs[name]=String(value);},
   addEventListener(event,callback){this.listeners[event]=callback;},
   replaceChildren(){this.children=[];},
   append(...nodes){this.children.push(...nodes);},
   querySelectorAll(selector){
     return this.children.filter(node=>selector==='.wallet-option'
       && node.className==='wallet-option');
   },
   querySelector(selector){
     const index=selector.startsWith('[data-index="') && selector.endsWith('"]')
       ? selector.slice(13,-2) : null;
     return index!==null?this.children.find(node=>node.dataset.index===index):null;
   }
 });
 const element=id=>elements[id]??(elements[id]=Object.assign(fakeNode(),{
   hidden:id==='request'
 }));
 const doc={getElementById:element,createElement:fakeNode};
 const statuses=[];
 const status=element('status');
 Object.defineProperty(status,'textContent',{
   get(){return this._text;},
   set(value){this._text=value;statuses.push(value);}
 });
 const walletCalls=[];
 const wallet={request:async({method})=>{
     walletCalls.push(method);
     return method==='eth_chainId'?'0x1':
       ['0x1111111111111111111111111111111111111111'];
   },on(){},removeListener(){}};
 const eventHandlers=new Map();
 const win={ethereum:wallet,
   addEventListener(type,callback){eventHandlers.set(type,callback);},
   dispatchEvent(event){
     if(event.type==='eip6963:requestProvider')
       eventHandlers.get('eip6963:announceProvider')?.({
         detail:{info:{name:'Rabby Wallet'},provider:wallet}
       });
   }
 };
 const next=[
   {method:'eth_requestAccounts',requiresApproval:true,transportId:1,params:[]},
   {method:'eth_chainId',requiresApproval:false,transportId:2,params:[]}
 ];
 const posted=[];let queue=[],timerId=0;
 const schedule=callback=>{queue.push({id:++timerId,callback});return timerId;};
 const cancel=id=>{queue=queue.filter(item=>item.id!==id);};
 const fetch=async(url,options)=>{
   if(url.endsWith('/next'))
     return next.length?{ok:true,status:200,json:async()=>next.shift()}:
       {ok:false,status:410};
   if(url.endsWith('/result')){
     posted.push(JSON.parse(options.body));
     return {ok:true,status:200};
   }
   return {ok:true,status:200};
 };
 const execute=new Function('window','document','fetch','setTimeout','clearTimeout',
   'Event','TextDecoder',script);
 execute(win,doc,fetch,schedule,cancel,class{constructor(type){this.type=type;}},
   class{decode(){return '';}});
 const flush=async()=>{for(let i=0;i<16;i++)await Promise.resolve();};
 await flush();
 assert.equal(element('request').hidden,false);
 assert.equal(walletCalls.filter(method=>method==='eth_requestAccounts').length,0,
   'receiving an account request must never silently connect a wallet');
 await element('approve').listeners.click();
 assert.equal(walletCalls.filter(method=>method==='eth_requestAccounts').length,1,
   'the explicit Connect action must invoke the selected extension once');
 await flush();
 assert.equal(element('request').hidden,true);
 for(let i=0;i<18&&queue.length;i++){
   const task=queue.shift();
   task.callback();
   await flush();
 }
 assert.deepEqual(posted.map(entry=>entry.transportId),[1,2]);
 assert.equal(element('request').hidden,true);
 assert.equal(statuses.at(-1),'Wallet connected. Complete the next verification request here.');
 assert.equal(queue.length,0);
});
