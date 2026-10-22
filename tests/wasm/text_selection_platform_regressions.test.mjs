import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

const read = p => readFileSync(new URL('../../'+p, import.meta.url), 'utf8');

test('canvas-only selection actions never bind Popup.visible open on Home', () => {
 const editor=read('ui/components/DirectionAwareTextArea.qml');
 const composer=read('ui/components/ComposerDrawer.qml');
 const popup=editor.slice(editor.indexOf('id: selectionBar'));
 assert.doesNotMatch(popup,/visible:\s*!textSelectionSettings\.systemSelectionEnabled/);
 assert.match(editor,/if \(!root\.customCanvasSelection \|\| !root\.activeFocus \|\| !root\.visible/);
 assert.match(editor,/onVisibleChanged: if \(!visible\) dismissSelectionActions\(\)/);
 assert.match(editor,/onActiveFocusChanged:[\s\S]*?selectionBar\.close\(\)/);
 assert.match(composer,/onClosed: \{[\s\S]*?mobilePlainEditor\.dismissSelectionActions\(\)/);
 assert.match(composer,/onClosed: \{[\s\S]*?markdownEditor\.dismissSelectionActions\(\)/);
 assert.match(editor,/selectionTop = Math\.min\(first\.y, last\.y\)/);
 assert.match(editor,/selectionBottom = Math\.max\(/);
});

test('native platform menus use actual MenuItems; custom Items remain canvas-only',()=>{
 const editor=read('ui/components/DirectionAwareTextArea.qml');
 assert.match(editor,/Qt\.platform\.os === "ios"/);
 assert.match(editor,/Qt\.platform\.os === "android"/);
 assert.match(editor,/Qt\.platform\.os === "wasm"/);
 assert.match(editor,/ContextMenu\.menu: root\.platformNativeMenu \? nativeContextMenu : canvasContextMenu/);
 assert.match(editor,/id: nativeContextMenu[\s\S]*?popupType: Popup\.Native/);
 assert.match(editor,/id: canvasContextMenu[\s\S]*?nativePresentation: false/);
 assert.match(editor,/onTriggered: root\.copy\(\)/);
 assert.match(editor,/onTriggered: root\.cut\(\)/);
 assert.match(editor,/onTriggered: root\.paste\(\)/);
 assert.match(editor,/visible: root\.selectAllOnDoubleTap && root\.customCanvasSelection/);
});

test('iOS status style is install-time policy and page theme changes remain live',()=>{
 const html=read('ui/wasm/genycaster-shell.html');
 const loader=read('ui/wasm/genycaster-loader.js');
 assert.doesNotMatch(html,/<meta name="apple-mobile-web-app-status-bar-style"/);
 assert.match(html,/:root\[data-chrome-theme="light"\]\{[^}]*--app-chrome:#f1f1f3/);
 assert.match(html,/:root\[data-chrome-theme="dark"\]\{[^}]*--app-chrome:#0b0c0f/);
 assert.match(html,/body::before\{[^}]*background:var\(--app-chrome\)/);
 assert.match(loader,/meta\.setAttribute\("content",color\)/);
 assert.doesNotMatch(loader,/statusMeta\.setAttribute/);
});

test('WASM Clipboard API failures use legacy fallback and restore DOM focus',async()=>{
 const cpp=read('src/presentation/qt/presentation_bridge.cpp');
 const method=cpp.slice(cpp.indexOf('void PresentationBridge::copyText('),cpp.indexOf('QString PresentationBridge::contextualReactionIcon'));
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
