import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

const read = p => readFileSync(new URL('../../'+p, import.meta.url), 'utf8');

test('closing a composer cancels its browser editing transaction',()=>{
 const editor=read('ui/components/controls/BidiTextArea.qml');
 const menu=read('ui/components/composer/EditorContextMenu.qml');
 const composer=read('ui/components/composer/ComposerDrawer.qml');
 assert.match(editor,/onVisibleChanged: if \(!visible\) dismissSelectionActions\(\)/);
 assert.match(editor,/function dismissSelectionActions\(\) \{ if \(editorMenu\) editorMenu.cancelPaste\(\) \}/);
 assert.match(menu,/presentationBridge.cancelBrowserTextEditor\(token\)/);
 assert.match(menu,/function onVisibleChanged\(\)/);
 assert.match(composer,/onClosed: \{[\s\S]*?mobilePlainEditor\.dismissSelectionActions\(\)/);
});

test('native mobile owns its editing toolbar and native desktop uses real MenuItems',()=>{
 const menu=read('ui/components/composer/EditorContextMenu.qml');
 const editor=read('ui/components/controls/BidiTextArea.qml');
 assert.match(menu,/platformMobile: Qt.platform.os === "ios" \|\| Qt.platform.os === "android"/);
 assert.match(menu,/browser: Qt.platform.os === "wasm"/);
 assert.match(editor,/ContextMenu\.menu: editorMenu\.platformMobile \|\| editorMenu\.browser \? null : editorMenu/);
 assert.match(menu,/popupType: Popup\.Native/);
 assert.match(menu,/onTriggered: root\.editor\.copy\(\)/);
 assert.match(menu,/onTriggered: root\.editor\.cut\(\)/);
 assert.match(menu,/editor\.paste\(\)/);
 assert.doesNotMatch(menu,/EditorMenuItem/);
});

test('iOS status style is install-time policy and page theme changes remain live',()=>{
 const html=read('ui/wasm/genycaster-shell.html');
 const loader=read('ui/wasm/genycaster-loader.js');
 assert.doesNotMatch(html,/<meta name="apple-mobile-web-app-status-bar-style"/);
 assert.match(html,/:root\[data-chrome-theme="light"\]\{[^}]*--app-chrome:#f1f1f3/);
 assert.match(html,/:root\[data-chrome-theme="dark"\]\{[^}]*--app-chrome:#0b0c0f/);
 assert.match(html,/#ios-status-chrome\{[^}]*background-color:var\(--app-chrome\)/);
 assert.match(html,/<div id="ios-status-chrome" aria-hidden="true"><\/div>/);
 assert.doesNotMatch(html,/html,body\{background:linear-gradient/);
 assert.match(loader,/meta\.setAttribute\("content",color\)/);
 assert.doesNotMatch(loader,/statusMeta\.setAttribute/);
});

test('WASM Clipboard API failures use legacy fallback and restore DOM focus',async()=>{
 const cpp=read('src/presentation/qt/presentation_bridge.cpp');
 const method=cpp.slice(cpp.indexOf('bool PresentationBridge::copyText('),cpp.indexOf('QString PresentationBridge::contextualReactionIcon'));
 const start=method.indexOf('EM_ASM({')+'EM_ASM({'.length;
 const end=method.indexOf('}, utf8.constData());',start);
 assert.ok(end>start,'EM_ASM JS must exist');
 const script=method.slice(start,end);
 const events=[];
 const prevFocus={focus:()=>events.push('restore')};
 let selected=null;
 const fakeSelection={rangeCount:0,removeAllRanges(){},addRange(){}};
 const body={appendChild(field){events.push('append');}};
 const document={
  activeElement:prevFocus,
  body,
  getSelection:()=>fakeSelection,
  createElement:()=>({
    value:'',readOnly:false,style:{},
    setAttribute(){},focus(){events.push('focus')},
    select(){events.push('select')},setSelectionRange(){events.push('range')},
    remove(){events.push('remove')}
  }),
  execCommand(cmd){selected=cmd;return true;}
 };
 const warnings=[];
 const sandbox={UTF8ToString:()=> 'متن Test 👋', $0:1, document,
  navigator:{clipboard:{writeText:()=>Promise.reject(Error('denied'))}},
  console:{warn:(...args)=>warnings.push(args)}};
 vm.runInNewContext(script,sandbox);
 await new Promise(resolve=>setImmediate(resolve));
 assert.equal(selected,'copy');
 assert.deepEqual(events,['append','focus','select','range','remove','restore']);
 assert.equal(warnings.length,0);
});
