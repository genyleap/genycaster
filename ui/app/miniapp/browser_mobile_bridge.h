#pragma once
// Shared mobile bridge wire protocol, document-start and main-frame only.
#define GENY_MOBILE_BRIDGE_SCRIPT \
"(() => {\n" \
" if (window !== window.top) return;\n" \
" const token = crypto.randomUUID();\n" \
" const send = window.webkit?.messageHandlers?.genyMiniApp ? v => window.webkit.messageHandlers.genyMiniApp.postMessage(v) : v => window.genyMobileTransport.postMessage(JSON.stringify(v));\n" \
" let count=0,since=performance.now();\n" \
" const endpoint=Object.freeze({postMessage(value){\n" \
"  if(typeof value !== 'string' || value.length > 32768) throw new RangeError('Mini App request limit');\n" \
"  const now=performance.now();if(now-since>1000){since=now;count=0;}\n" \
"  if(++count>120) throw new Error('Mini App request rate limit');\n" \
"  send({document:token,message:value});\n" \
" }});\n" \
" Object.defineProperty(window,'ReactNativeWebView',{value:endpoint});\n" \
" Object.defineProperty(window,'__genyMobileDocument',{value:token});\n" \
" for(const [name,event] of [['__genyMiniAppReply','FarcasterFrameCallback'],['__genyMiniAppEthProviderEvent','FarcasterFrameEthProviderEvent'],['__genyMiniAppEvent','FarcasterFrameEvent']])\n" \
"  Object.defineProperty(window,name,{value:(documentToken,data)=>{if(documentToken===token)document.dispatchEvent(new MessageEvent(event,{data}));}});\n" \
"})();"
