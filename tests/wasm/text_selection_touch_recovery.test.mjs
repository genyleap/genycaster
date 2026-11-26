import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

const read = file => readFileSync(new URL('../../'+file, import.meta.url), 'utf8');
const qml=read('ui/components/composer/EditorContextMenu.qml');
const apply=qml.slice(qml.indexOf('    function applyText('),qml.indexOf('    Connections {'));
function exercise(before,value,kind,selection=[0,0],handler=null){
 const calls=[];
 const editor={forceActiveFocus(){},clipboardPasteHandler:handler,clear(){calls.push(['clear']);}};
 const context={editor,browser:true,browserEditing:true,plainText:()=>before,pasteStart:selection[0],pasteEnd:selection[1],
    replaceRange:(start,end,text)=>calls.push([start,end,text]),value,kind};
 vm.runInNewContext(apply+'\napplyText(value,kind)',context);
 return calls;
}
test('native paste preserves the captured selection and inserts only its payload',()=>{
 assert.deepEqual(exercise('before','new','browser-paste',[1,4]),[[1,4,'new']]);
 assert.deepEqual(exercise('before','','browser-paste',[1,4]),[]);
});
test('native edit preserves unchanged text and does not split surrogate pairs',()=>{
 assert.deepEqual(exercise('hello world','hello Qt','browser-edit'),[[6,11,'Qt']]);
 assert.deepEqual(exercise('a😀b','a😁b','browser-edit'),[[1,3,'😁']]);
 assert.deepEqual(exercise('unchanged','unchanged','browser-edit'),[]);
});
test('recovery text adapter receives one explicit payload, with clear and cancel paths',()=>{
 const received=[];
 assert.deepEqual(exercise('','test fixture','browser-paste',[0,0],v=>received.push(v)),[]);
 assert.deepEqual(received,['test fixture']);
 assert.deepEqual(exercise('word','','browser-edit',[0,0],()=>{}),[[0,4,'']]);
 assert.match(qml,/requestId !== root.pendingPaste/);
 assert.match(qml,/if \(!root.usable\(\)\) return/);
 assert.match(qml,/root.applyText\(value, "browser-edit"\)/);
});

test('light installed-PWA chrome has a fixed top color surface, not a sampled gradient',()=>{
 const html=read('ui/wasm/genycaster-shell.html');
 assert.match(html,/<body>\s*<div id="ios-status-chrome" aria-hidden="true"><\/div>\s*<div id="qt-screen"/);
 assert.match(html,/#ios-status-chrome\{[^}]*position:fixed;top:0[^}]*background-color:var\(--app-chrome\)/);
 assert.match(html,/:root\[data-chrome-theme="light"\]\{[^}]*--app-chrome:#f1f1f3/);
 assert.doesNotMatch(html,/body::before\{/);
 assert.doesNotMatch(html,/html,body\{background:linear-gradient/);
});
