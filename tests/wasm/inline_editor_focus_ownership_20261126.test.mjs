import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../../'+p,import.meta.url),'utf8');
const js=read('ui/wasm/native-text-editor-pre.js');
const menu=read('ui/components/composer/EditorContextMenu.qml');
test('inline editor stays in the active Qt window and does not trigger external window focus',()=>{
 assert.match(js,/focused\?\.closest\?\.\('\.qt-window'\)/);
 assert.match(js,/host.append\(input\)/);
 assert.doesNotMatch(js,/document.body.append\(input\)|showModal|createElement\(.dialog./);
 assert.match(js,/while\(node\?\.shadowRoot\?\.activeElement\)/);
 assert.match(js,/e.composedPath\(\).includes\(active.input\)/);
 assert.doesNotMatch(menu,/Qt.inputMethod.hide\(\)/);
});
test('Qt paint ownership is conditional on verified native focus, visibility and hit testing',()=>{
 assert.match(js,/deepFocus\(\)!==session.input/);
 assert.match(js,/getComputedStyle\(session.input\)/);
 assert.match(js,/root.elementFromPoint/);
 assert.match(js,/if\(!ready\(session\)\)\{close\(session,false\);return false;\}/);
 assert.match(js,/if\(!ready\(session\)\)\{close\(session\);return;\}/);
 assert.match(menu,/browserEditing = opened && pendingPaste === token/);
 assert.match(menu,/if \(!ok\) \{ root.cancelPaste\(\); return \}/);
 assert.match(js,/input.value='';\s*session.input.remove\(\)/);
});
test('steady geometry refresh cannot rewrite the caret or style on every timer tick',()=>{
 assert.match(js,/session.appliedStyle\[key\]===value/);
 assert.match(js,/clipPath:left\|\|top\|\|right\|\|bottom\?.*:'none'/);
 assert.match(js,/!options.geometryOnly&&!session.composing&&session.input.value!==value/);
 assert.match(menu,/onTriggered: root.syncBrowserEditor\(true\)/);
 assert.match(menu,/Qt.callLater\(function\(\) \{[\s\S]*?root.editor.activeFocus && root.usable\(\)/);
});