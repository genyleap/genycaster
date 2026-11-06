import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const qml=(name)=>readFileSync(new URL('../../ui/components/'+name+'.qml',import.meta.url),'utf8');
test('cast header preferred size must never depend on its own Layout width',()=>{
 const src=qml('DetailSurface');
 const part=src.slice(src.indexOf('id: detailNameAndChannel'),src.indexOf('id: parentContextLayout'));
 assert.doesNotMatch(part,/detailNameAndChannel\.width/);
 assert.doesNotMatch(part,/Layout\.preferredHeight: Layout\.preferredWidth/);
 assert.match(part,/Math\.max\(80, root\.width \* 0\.55\)/);
 assert.match(part,/Math\.max\(80, root\.width \* 0\.44\)/);
});
test('GenyPulse has a real icon token and not an undefined Theme property',()=>{
 const src=qml('GenyPulsePanel');
 const theme=readFileSync(new URL('../../ui/theme/Theme.qml',import.meta.url),'utf8');
 assert.doesNotMatch(src,/Theme\.iconChevronRight/);
 assert.match(src,/iconGlyph: Theme\.iconArrowRight/);
 assert.match(theme,/readonly property string iconArrowRight: "arrow-right"/);
});
test('source client attribution sanitizes only control characters, never the G in Genycaster',()=>{
 const badge=qml('SourceClientBadge');
 assert.match(badge,/replace\(\/\[\\x00-\\x1f\\x7f\]\/g/);
 const pattern=/[\x00-\x1f\x7f]/g;
 assert.equal('Genycaster'.replace(pattern,''),'Genycaster');
 assert.equal('Geny\x01caster'.replace(pattern,''),'Genycaster');
});
test('deep cast rich blocks do not retain orphaned pending loaders on a feed switch',()=>{
 const rich=qml('RichCastRenderer');
 assert.match(rich,/delegate: Loader \{[\s\S]*?asynchronous: false/);
 // Feed list remains async to avoid long stalls during rapid scrolling.
 assert.match(qml('FeedSurface'),/asynchronous: !sectionRow/);
});


test('HTML served with a misleading image Content-Type never reaches QImageReader', () => {
 const src=readFileSync(new URL('../../ui/app/remote_image_provider.cpp', import.meta.url),'utf8');
 const predicate=src.slice(src.indexOf('[[nodiscard]] bool responseMayContainImage('),
   src.indexOf('[[nodiscard]] int quantizedRequestedDimension('));
 assert.ok(predicate.length>0, 'image response gate is present');
 assert.match(predicate,/if \(recognizedImagePayload\(bytes\)\) \{\s*return true;\s*\}/);
 assert.match(predicate,/if \(looksLikeMarkupPayload\(bytes\)\) \{\s*return false;\s*\}/);
 assert.ok(predicate.indexOf('if (recognizedImagePayload(bytes))') < predicate.indexOf('if (looksLikeMarkupPayload(bytes))'));
 assert.ok(predicate.indexOf('if (looksLikeMarkupPayload(bytes))') < predicate.indexOf('if (contentType.startsWith('),
   'decoded content must take priority over a misleading image/* response header');
});
