import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../../'+p,import.meta.url),'utf8');
const js=read('ui/wasm/native-text-editor-pre.js');
const adapter=read('ui/components/composer/EditorContextMenu.qml');
const cpp=read('src/presentation/qt/presentation_bridge.cpp');
test('browser inputs are persistent siblings of Qt canvas before pointer interaction',()=>{
 assert.match(js,/const fields=new Map\(\)/);
 assert.match(js,/screen.after\(layer\)/);
 assert.match(js,/layer.append\(input\)/);
 assert.doesNotMatch(js,/showModal|createElement\(.dialog.|host.append\(input\)/);
 assert.match(adapter,/triggeredOnStart: true/);
 assert.match(adapter,/onTriggered: root.syncBrowserEditor\(true\)/);
 assert.match(adapter,/if \(!browserEditing\) \{ mountBrowserEditor\(\); return \}/);
});
test('visible editors are isolated by token and covered controls release their browser nodes',()=>{
 assert.match(js,/fields.get\(token\)/);
 assert.match(js,/fields.delete\(field.token\)/);
 assert.match(js,/field.input.value='';[\s\S]*?field.input.remove\(\)/);
 assert.match(js,/if\(!layout\(f\)\)\{remove\(f\);return false;\}/);
 assert.match(cpp,/sibling->property\("browserEditorOverlayHost"\).toBool\(\)/);
 for (const host of ['controls/GenyAdaptiveDialog','apps/TaskManagerPopup'])
  assert.match(read('ui/components/'+host+'.qml'),/readonly property bool browserEditorOverlayHost: true/);
 assert.match(cpp,/layoutRect\(sibling\).intersects\(clip\)/);
 assert.match(cpp,/QRectF\(0, 0, target->width\(\), target->height\(\)\)/);
 assert.match(cpp,/const auto rect = layoutRect\(item\)/);
 assert.match(adapter,/browserEditing = opened && pendingPaste === token/);
 assert.match(adapter,/if \(!ok\) \{ root.cancelPaste\(\); return \}/);
});
test('geometry refresh preserves values and browser focus belongs to real input',()=>{
 assert.match(js,/f.style\[key\]!==value/);
 assert.match(js,/!options.geometryOnly&&!f.composing&&f.input.value!==value/);
 assert.match(js,/if\(options.focusRequested&&document.activeElement!==f.input\)f.input.focus/);
 assert.match(adapter,/function onBrowserEditorFocusChanged/);
 assert.match(adapter,/root.browserFocused = focused/);
 assert.doesNotMatch(adapter,/onActiveFocusChanged|Qt.inputMethod.hide/);
});
