import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {readFile,mkdtemp,rm} from 'node:fs/promises';
import {createServer} from 'node:http';
const root=new URL('../../',import.meta.url);
const adapter=await readFile(process.env.NATIVE_EDITOR_ADAPTER || new URL('ui/wasm/native-text-editor-pre.js',root),'utf8');
const loader=await readFile(new URL('ui/wasm/genycaster-loader.js',root),'utf8');
const viewport=loader.slice(loader.indexOf('const screen=$("qt-screen");'),loader.indexOf('retry.addEventListener("click"'));
assert.ok(viewport.includes('function fit()'));
const html='<!doctype html><meta name="viewport" content="width=device-width,initial-scale=1"><style>body{margin:0}#qt-screen{position:absolute;inset:0;touch-action:none}canvas{width:100%;height:100%;touch-action:none}button{position:absolute;left:10px;top:10px;width:100px;height:40px}</style><div id="qt-screen"><canvas class="qt-window-canvas"></canvas><button id="cancel">Cancel</button></div><script>const $=id=>document.getElementById(id);'+adapter+'\n'+viewport+'\n'+
'window.stats={down:0,mouse:0,click:0,prevented:0};const btn=$("cancel");btn.addEventListener("pointerdown",e=>{stats.down++;if(e.defaultPrevented)stats.prevented++;fit()});btn.addEventListener("mousedown",()=>stats.mouse++);btn.addEventListener("click",()=>{stats.click++;window.__genycasterSystemEditor.cancel("editor")});window.mount=()=>{const w=screen.clientWidth,h=screen.clientHeight;const options={geometry:{x:20,y:80,width:w-40,height:h-150,clipX:20,clipY:80,clipWidth:w-40,clipHeight:h-150,windowWidth:w,windowHeight:h},multiline:true,fontSize:16,ink:"#000",accent:"#00f",placeholder:"Compose",focusRequested:true};const a=window.__genycasterSystemEditor;a.open("editor","hello",options,()=>{},()=>{});a.sync("editor","hello",options);return document.activeElement?.className};window.fitState=()=>({pointers:canvasPointers.size,settling:!!canvasGestureSettle,deferred:deferredCanvasFit});</script>';
const server=createServer((req,res)=>{res.writeHead(200,{'content-type':'text/html'});res.end(html)});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const tmp=await mkdtemp('/tmp/genycaster-input-test-');
const browser=spawn(process.env.CHROMIUM_EXECUTABLE||'/root/.cache/ms-playwright/chromium_headless_shell-1194/chrome-linux/headless_shell',['--no-sandbox','--disable-dev-shm-usage','--remote-debugging-port=0','--user-data-dir='+tmp,'about:blank'],{stdio:['ignore','ignore','pipe']});
let ws;
const wait=ms=>new Promise(r=>setTimeout(r,ms));
try {
 const base=await new Promise((resolve,reject)=>{let b='';const timeout=setTimeout(()=>reject(Error('browser launch timeout')),10000);browser.stderr.on('data',x=>{b+=x;const m=b.match(/DevTools listening on ws:\/\/127\.0\.0\.1:(\d+)/);if(m){clearTimeout(timeout);resolve('http://127.0.0.1:'+m[1])}});browser.once('exit',c=>{clearTimeout(timeout);reject(Error('browser exited '+c))})});
 const pages=await(await fetch(base+'/json/list')).json();
 ws=new WebSocket(pages.find(p=>p.type==='page').webSocketDebuggerUrl);
 await new Promise((r,j)=>{ws.onopen=r;ws.onerror=j});
 let id=0;const pending=new Map();
 function send(method,params={}){return new Promise((r,j)=>{const n=++id;pending.set(n,[r,j]);setTimeout(()=>{if(pending.delete(n))j(Error('CDP timeout '+method))},5000).unref();ws.send(JSON.stringify({id:n,method,params}))})}
 ws.onmessage=e=>{const m=JSON.parse(e.data);if(m.id){const p=pending.get(m.id);pending.delete(m.id);if(p)m.error?p[1](m.error):p[0](m.result)}};
 async function evalJS(expression){const r=await send('Runtime.evaluate',{expression,returnByValue:true});if(r.exceptionDetails)throw Error(r.exceptionDetails.exception?.description||'evaluation failed');return r.result.value}
 await send('Page.enable');await send('Runtime.enable');await send('Emulation.setDeviceMetricsOverride',{width:390,height:700,deviceScaleFactor:2,mobile:true});await send('Emulation.setTouchEmulationEnabled',{enabled:true,maxTouchPoints:1});
 await send('Page.navigate',{url:'http://127.0.0.1:'+server.address().port+'/'});
 await wait(500);
 assert.equal(await evalJS('mount()'),'genycaster-native-input');
 // Actual trusted touch via CDP, not dispatchEvent or synthetic app clicks.
 await send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x:50,y:30}]});
 await wait(50);
 await send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
 await wait(200);
 const result=await evalJS('({stats,fields:document.querySelectorAll(".genycaster-native-input").length,fit:fitState()})');
 console.log('TOUCH_CANCEL',JSON.stringify(result));
 assert.deepEqual(result.stats,{down:1,mouse:1,click:1,prevented:0});
 assert.equal(result.fields,0);
 assert.deepEqual(result.fit,{pointers:0,settling:false,deferred:false});
 assert.equal(await evalJS('mount()'),'genycaster-native-input');
 assert.equal(await evalJS('document.querySelectorAll(".genycaster-native-input").length'),1);
 await send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x:50,y:30}]});
 await send('Input.dispatchTouchEvent',{type:'touchCancel',touchPoints:[]});
 await wait(200);
 assert.deepEqual(await evalJS('fitState()'),{pointers:0,settling:false,deferred:false});
 console.log('REOPEN_AND_CANCEL_PASS');
}finally{ws?.close();browser.kill('SIGTERM');server.close();await wait(250);await rm(tmp,{recursive:true,force:true})}
