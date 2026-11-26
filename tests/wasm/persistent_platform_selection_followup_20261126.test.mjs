import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../../'+p,import.meta.url),'utf8');
const js=read('ui/wasm/native-text-editor-pre.js');
const loader=read('ui/wasm/genycaster-loader.js');
const cpp=read('src/presentation/qt/presentation_bridge.cpp');
const cast=read('ui/components/content/RichInlineText.qml');
const composer=read('ui/components/composer/ComposerDrawer.qml');
test('DOM editor keeps Qt embedded fonts and visible size instead of browser default',()=>{
 assert.match(cpp,/":\/fonts\/DejaVuSans.ttf"/);
 assert.match(cpp,/registerFont\(UTF8ToString\(\$0\)/);
 assert.match(js,/new FontFace\(family,bytes/);
 assert.match(js,/fontFamily:fontStack\(o.fontFamily\)/);
 assert.match(js,/fontSize:size\/scale/);
 assert.match(js,/transformOrigin:'top left'/);
});
test('native editor geometry uses actual Qt canvas and overlay coordinate space',()=>{
 assert.match(js,/querySelectorAll\(".qt-window-canvas"\)/);
 assert.match(js,/origin=layer.getBoundingClientRect\(\)/);
 assert.match(js,/b.top-origin.top\+y\*sy/);
 assert.match(loader,/viewport\?viewport.width:window.innerWidth/);
});
test('outside gestures finish before the viewport reflows and focus releases',()=>{
 assert.match(loader,/if\(canvasPointers.size\)\{deferredCanvasFit=true;return;\}/);
 assert.match(loader,/window.addEventListener\("pointerup",finishCanvasGesture,true\)/);
 assert.match(js,/requestAnimationFrame\(\(\)=>\{if\(document.activeElement===active\)active.blur\(\)/);
 assert.match(composer,/initialComposerFocus.stop\(\)[\s\S]*?mobilePlainEditor.dismissSelectionActions\(\)[\s\S]*?root.close\(\)/);
});
test('cast text is a non-editable DOM article with native selection and structured spans',()=>{
 assert.match(js,/options.readOnlyCast\?'div'/);
 assert.match(js,/node.setAttribute\('role','article'\)/);
 assert.match(js,/node.textContent=/);
 assert.doesNotMatch(js,/\.innerHTML\s*=/);
 assert.match(js,/window.getSelection\(\)/);
 assert.match(js,/send\('scroll',\{delta\}\)/);
 assert.match(cast,/readOnlyCast: true/);
 assert.match(cast,/browserEditorGeometry\(browserCastGeometry/);
 assert.match(cast,/root.browserCastHeight = height/);
 assert.match(cast,/!root.browserCastMounted/);
 assert.match(cast,/Component.onDestruction: releaseBrowserCast\(\)/);
});

test('persistent browser targets cannot paint or hit through shell chrome',()=>{
 assert.match(cpp,/sibling->property\("browserEditorOccludes"\).toBool\(\)/);
 assert.match(cpp,/clip.setBottom\(obstruction.top\(\)\)/);
 for(const f of ['navigation/BottomNavigation','navigation/FloatingComposeButton','feed/FeedSurface'])
  assert.match(read('ui/components/'+f+'.qml'),/readonly property bool browserEditorOccludes: true/);
 assert.match(read('ui/components/shell/AppShell.qml'),/onAboutToHide: \{[\s\S]*?nativeSignerPhraseInput.dismissSelectionActions\(\)/);
});
