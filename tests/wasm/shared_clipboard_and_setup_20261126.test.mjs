import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const read=p=>readFileSync(p,'utf8');
const cpp=read('src/presentation/qt/presentation_bridge.cpp');
const menu=read('ui/components/composer/EditorContextMenu.qml');
const field=read('ui/components/controls/BidiTextField.qml');
const setup=read('ui/components/setup/InitialSetup.qml');
const shell=read('ui/components/shell/AppShell.qml');
test('browser paste uses an explicit read and zeros temporary UTF-8 bytes',async()=>{
 const section=cpp.slice(cpp.indexOf('void PresentationBridge::requestClipboardText'),cpp.indexOf('[[nodiscard]] QString shareHash'));
 const js=section.slice(section.indexOf('EM_ASM({')+'EM_ASM({'.length,section.indexOf('}, request.constData());'));
 const mem=new Uint8Array(2048),enc=new TextEncoder(),dec=new TextDecoder();
 const token='one-shot';let next=128;let payload;const freed=[];
 const readString=p=>dec.decode(mem.slice(p,mem.indexOf(0,p)));
 const ctx={window:{isSecureContext:true},navigator:{clipboard:{readText:async()=> 'test clipboard ✓'}},
  $0:32,UTF8ToString:readString,lengthBytesUTF8:s=>enc.encode(s).length,
  stringToUTF8:(s,p,n)=>{mem.set(enc.encode(s).slice(0,n-1),p);mem[p+Math.min(enc.encode(s).length,n-1)]=0;},
  _malloc:n=>{const p=next;next+=n;return p;},_free:p=>freed.push(p),HEAPU8:mem,
  Module:{_genycasterClipboardTextResult:(id,p,n,ok)=>{payload={id:readString(id),value:p?dec.decode(mem.slice(p,p+n)):'',ok,p,n};}}};
 ctx.stringToUTF8(token,32,32);vm.runInNewContext('(()=>{'+js+'})()',ctx);
 await new Promise(resolve=>setImmediate(resolve));
 assert.equal(payload.id,token);assert.equal(payload.value,'test clipboard ✓');assert.equal(payload.ok,1);
 assert.ok(mem.slice(payload.p,payload.p+payload.n+1).every(b=>b===0));
 assert.equal(freed.length,2);
 ctx.navigator.clipboard.readText=async()=>{throw Error('denied');};
 vm.runInNewContext('(()=>{'+js+'})()',ctx);await new Promise(resolve=>setImmediate(resolve));
 assert.equal(payload.ok,0);assert.equal(payload.value,'');
});
test('clipboard routing is one-shot, scoped to visible writable editor and guarded on dismissal',()=>{
 assert.match(menu,/requestId !== root.pendingPaste/);
 assert.match(menu,/root.cancelPaste\(\)/);
 assert.match(menu,/return !!editor && editor.enabled && editor.visible && !editor.readOnly/);
 assert.match(menu,/function onTextChanged\(\) \{[\s\S]*?root.directPaste = ""[\s\S]*?root.syncBrowserEditor\(\)/);
 assert.match(menu,/function onVisibleChanged\(\)/);
 assert.match(menu,/!root.sensitive/);
 assert.match(field,/onLongPressed: \{[\s\S]*?editorMenu\.requestSystemEditor\(\)/);
 for(const [source,name] of [[setup,'setupPasteRecoveryWords'],[shell,'signerPasteRecoveryWords']]){
  assert.match(source,new RegExp('objectName: "'+name+'"'));
  assert.match(source,/clipboardPasteHandler: value =>/);
  assert.match(source,/cancelClipboardPaste\(\)/);
 }
});
test('legal links explicitly use muted theme color and task stages own their CTA',()=>{
 assert.match(setup,/showsPrimaryCta: \[0,1,2,6,7\]\.includes\(currentStep\)/);
 assert.match(setup,/visible: root.showsPrimaryCta\s+height: !visible \? 0/);
 assert.match(setup,/href="terms" style="color:' \+ Theme.inkMuted.toString\(\)/);
 assert.match(setup,/href="privacy" style="color:' \+ Theme.inkMuted.toString\(\)/);
});
test('revoke disables write privilege synchronously and stale ready closes reauthorization',()=>{
 const revoke=cpp.slice(cpp.indexOf('void PresentationBridge::revokeSigner()'),cpp.indexOf('int PresentationBridge::utf8ByteCount'));
 assert.match(revoke,/m_signerBusy = true;[\s\S]*?m_writeReady = false;[\s\S]*?emit stateChanged\(\);[\s\S]*?emit signerRevokeRequested\(\);/);
 assert.match(shell,/if \(presentationBridge.writeReady\) \{\s*close\(\)\s*return/);
 assert.match(shell,/interactionAuthorizationDialog.visible\s*&& presentationBridge.writeReady\)\s*interactionAuthorizationDialog.close\(\)/);
});
