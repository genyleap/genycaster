import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

// Execute the QML JavaScript binding; avoid a second test-only parser.
const qml=readFileSync(new URL('../../ui/components/RichCastRenderer.qml',import.meta.url),'utf8');
function bodyBetween(start,end) {
  const a=qml.indexOf(start);
  const b=qml.indexOf(end,a+start.length);
  assert.ok(a>=0 && b>a,'Renderer JavaScript expression boundaries missing');
  return qml.slice(a+start.length,b);
}
const inlineBody=bodyBetween('    function inlineSpansFor(value) {\n',
  '\n    }\n\n    // Protocol Markdown');
const fallbackBody=bodyBetween('    readonly property var fallbackBlocks: {\n',
  '\n    }\n\n    function canonicalUrl(value)');
const inline=new Function('Theme','value',inlineBody);
const fallback=new Function('root',fallbackBody);
function render(plainText) {
 const root={plainText,enhanced:false};
 root.inlineSpansFor=value=>inline({accent:'#cc5511'},value);
 return fallback(root);
}
test('fenced code becomes a native code block without a Rich sidecar',()=>{
 const rows=render(['Introduction','```js','const x = 42;',
  '```','After'].join('\n'));
 assert.deepEqual(rows.map(x=>x.kind),['paragraph','code','paragraph']);
 assert.equal(rows[1].language,'js');
 assert.equal(rows[1].text,'const x = 42;');
 assert.equal(rows[0].text,'Introduction');
 assert.equal(rows[2].text,'After');
});
test('inline backticks become styled inline-code without literal delimiters',()=>{
 const rows=render('Use `std::span` instead of a raw buffer.');
 assert.equal(rows.length,1);
 assert.deepEqual(rows[0].inlines,[
   {kind:'text',text:'Use '},
   {kind:'inline-code',text:'std::span'},
   {kind:'text',text:' instead of a raw buffer.'}
 ]);
});
test('unclosed fences remain visible and do not swallow ordinary text',()=>{
 const rows=render('Start\n```cpp\nreturn 0;');
 assert.equal(rows.length,1);
 assert.equal(rows[0].kind,'paragraph');
 assert.match(rows[0].text,/return 0/);
});
test('fences accept tilde and reject mismatched delimiter types',()=>{
 const rows=render('~~~python\nprint(1)\n~~~');
 assert.deepEqual(rows.map(x=>x.kind),['code']);
 assert.equal(rows[0].language,'python');
 assert.equal(rows[0].text,'print(1)');
 const malformed=render('~~~cpp\nconst x=0;\n```');
 assert.equal(malformed[0].kind,'paragraph');
});

test('legacy GenyCaster cast from November 3 recovers its JavaScript block',()=>{
 const text='Test 3.0 Gm from modern client. 🎉 on topic dev, channel /replyguys. test markdown plus code blocks.\n\njavascript:\n\nconsole.log("Hello, world");';
 const rows=render(text);
 assert.deepEqual(rows.map(x=>x.kind),['paragraph','code']);
 assert.equal(rows[0].text,'Test 3.0 Gm from modern client. 🎉 on topic dev, channel /replyguys. test markdown plus code blocks.');
 assert.equal(rows[1].language,'javascript');
 assert.equal(rows[1].text,'console.log("Hello, world");');
});
test('legacy code fallback does not mistake ordinary language-labelled prose for code',()=>{
 const rows=render('python:\n\nI love snakes and nature.');
 assert.equal(rows.length,1);
 assert.equal(rows[0].kind,'paragraph');
});

test('multiple-backtick inline code keeps an interior backtick',()=>{
 const rows=render('Use ``templated`value`` without losing the marker.');
 assert.deepEqual(rows[0].inlines,[
   {kind:'text',text:'Use '},
   {kind:'inline-code',text:'templated`value'},
   {kind:'text',text:' without losing the marker.'}
 ]);
});
