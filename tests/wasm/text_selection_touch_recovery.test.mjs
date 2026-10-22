import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

const read = file => readFileSync(new URL('../../'+file, import.meta.url), 'utf8');
const qml=read('ui/components/DirectionAwareTextArea.qml');

// Exercise the actual QML JavaScript handler instead of only checking source
// strings. Qt's native selection API can update selectedText while Safari
// forwards release to a different touch grabber.
function handler(name, after) {
 const start=qml.indexOf(name+': {');
 assert.ok(start>=0,name);
 const s=start+name.length+3;
 const end=qml.indexOf('\n    }\n    '+after, s);
 assert.ok(end>s,name+' terminator');
 const body=qml.slice(s,end);
 return '(function(){'+(name==='onTriggered' ? body.replace(/\n        }\s*$/,'') : body)+'})()';
}

test('selected text opens the WASM toolbar despite transient IME focus loss',()=>{
 const code=handler('onTriggered','onSelectedTextChanged');
 let opens=0, restarts=0;
 const root={
  customCanvasSelection:true, visible:true, activeFocus:false,
  selectedText:'Hello, world!',selectionPressActive:false,selectionGestureActive:false,
  selectionHandleDragging:false, selectionReleaseRetries:0,
  showSelectionActions(){opens++;}
 };
 const selectionDelay={restart(){restarts++}};
 vm.runInNewContext(code,{root,selectionDelay});
 assert.equal(opens,1);
 assert.equal(restarts,0);
 root.visible=false;
 vm.runInNewContext(code,{root,selectionDelay});
 assert.equal(opens,1,'a closed drawer must not leak its menu onto Home');
});

test('missing Safari touch release cannot indefinitely suppress Copy',()=>{
 const code=handler('onTriggered','onSelectedTextChanged');
 const root={customCanvasSelection:true,visible:true,selectedText:'Hello',
  selectionPressActive:true,selectionGestureActive:true,
  selectionHandleDragging:false,selectionReleaseRetries:0,
  showSelectionActions(){this.opens=(this.opens||0)+1;}};
 let restarts=0;
 const selectionDelay={restart(){restarts++}};
 for(let i=0;i<4;i++)vm.runInNewContext(code,{root,selectionDelay});
 assert.equal(restarts,3);
 assert.equal(root.opens,1);
 assert.equal(root.selectionPressActive,false);
 assert.equal(root.selectionGestureActive,false);
 assert.equal(root.selectionReleaseRetries,0);
});

test('selection change arms toolbar even while the original touch is pressed',()=>{
 const code=handler('onSelectedTextChanged','onPressAndHold');
 const root={customCanvasSelection:true,visible:true,selectionHandleDragging:false,
  selectionReleaseRetries:3};
 let restarts=0,closes=0;
 const selectionDelay={restart(){restarts++},stop(){}};
 const selectionBar={close(){closes++}};
 vm.runInNewContext(code,{root,selectionDelay,selectionBar,selectedText:'Hello',selectionHandleDragging:false});
 assert.equal(restarts,1);
 assert.equal(root.selectionReleaseRetries,0);
 assert.equal(closes,0);
});

test('light iOS PWA chrome has a real fixed top color surface, not a sampled gradient',()=>{
 const html=read('ui/wasm/genycaster-shell.html');
 assert.match(html,/<body>\s*<div id="ios-status-chrome" aria-hidden="true"><\/div>\s*<div id="qt-screen"/);
 assert.match(html,/#ios-status-chrome\{[^}]*position:fixed;top:0[^}]*background-color:var\(--app-chrome\)/);
 assert.match(html,/:root\[data-chrome-theme="light"\]\{[^}]*--app-chrome:#f1f1f3/);
 assert.doesNotMatch(html,/body::before\{/);
 assert.doesNotMatch(html,/html,body\{background:linear-gradient/);
});
