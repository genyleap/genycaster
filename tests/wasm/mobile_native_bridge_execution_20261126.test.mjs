import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const header=fs.readFileSync(new URL('../../ui/app/miniapp/browser_mobile_bridge.h',import.meta.url),'utf8');
const script=[...header.matchAll(/^"((?:\\.|[^"\\])*)"/gm)].map(m=>JSON.parse('"'+m[1]+'"')).join('');
function boot(platform,main=true){
  const sent=[], events=[];
  const window={};
  window.top=main?window:{};
  if(platform==='ios')window.webkit={messageHandlers:{genyMiniApp:{postMessage:m=>sent.push(m)}}};
  else window.genyMobileTransport={postMessage:m=>sent.push(JSON.parse(m))};
  const ctx={window,crypto:{randomUUID:()=> '01234567-89ab-cdef-0123-456789abcdef'},
    document:{dispatchEvent:e=>events.push(e)},MessageEvent:class {constructor(type,{data}){this.type=type;this.data=data;}},
    performance:{now:()=>0}};
  vm.runInNewContext(script,ctx);
  return {window,sent,events};
}
for(const platform of ['android','ios']){
  test(platform+' native document-start bridge delivers RPC and ignores stale replies',()=>{
    const {window,sent,events}=boot(platform);
    window.ReactNativeWebView.postMessage('{"id":1}');
    assert.equal(sent[0].message,'{"id":1}');
    assert.equal(sent[0].document,window.__genyMobileDocument);
    window.__genyMiniAppReply('wrong',{id:1});
    assert.equal(events.length,0);
    window.__genyMiniAppReply(window.__genyMobileDocument,{id:1});
    window.__genyMiniAppEthProviderEvent(window.__genyMobileDocument,{event:'accountsChanged'});
    window.__genyMiniAppEvent(window.__genyMobileDocument,{event:'back'});
    assert.deepEqual(events.map(e=>e.type),['FarcasterFrameCallback','FarcasterFrameEthProviderEvent','FarcasterFrameEvent']);
    assert.throws(()=>window.ReactNativeWebView.postMessage('x'.repeat(32769)),/request limit/);
    for(let i=1;i<120;i++)window.ReactNativeWebView.postMessage('{}');
    assert.throws(()=>window.ReactNativeWebView.postMessage('{}'),/rate limit/);
    assert.equal(boot(platform,false).window.ReactNativeWebView,undefined);
  });
}
test('mobile wallet approval is separate from the Mini App and uses scoped loopback',()=>{
  const read=p=>fs.readFileSync(new URL('../../'+p,import.meta.url),'utf8');
  const android=read('properties/Android/src/com/genyleap/genycaster/MiniAppBrowser.java');
  const ios=read('ui/app/miniapp/browser_ios.m');
  const host=read('ui/app/miniapp/host.cpp');
  const bridge=read('src/providers/compatibility/browser_wallet/bridge.cpp');
  assert.match(android,/if \(wallet\) return uri!=null && uri.toString\(\).equals\(origin\)/);
  assert.match(android,/wallet && request.hasGesture\(\)/);
  assert.match(ios,/return \[url isEqual:self.origin\] && self.callbacks.allowed_url/);
  assert.match(ios,/WKNavigationTypeLinkActivated/);
  assert.match(host,/BrowserRole::WalletApproval/);
  assert.match(host,/m_walletBrowserSessionUrl == url/);
  assert.match(bridge,/showNativeApproval && m_config.nativeApprovalSettled/);
  const network=read('properties/Android/res/xml/miniapp_network_security.xml');
  assert.match(network,/base-config cleartextTrafficPermitted="false"/);
  assert.match(network,/>127\.0\.0\.1<\/domain>/);
  const ats=read('properties/iOS/Configure.cmake');
  assert.match(ats,/<key>127\.0\.0\.1<\/key>/);
  assert.doesNotMatch(ats,/NSAllowsArbitraryLoads/);
});
