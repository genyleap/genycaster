import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const shell = readFileSync(new URL('../../ui/components/AppShell.qml',import.meta.url),'utf8');
const bridge = readFileSync(new URL('../../src/presentation/qt/presentation_bridge.cpp',import.meta.url),'utf8');
const runtime = readFileSync(new URL('../../ui/app/runtime_ui_bindings.cpp',import.meta.url),'utf8');
const code = readFileSync(new URL('../../ui/components/CodeBlockView.qml',import.meta.url),'utf8');

test('topic UI matches both indexed and HyperSnap 40-hex cast IDs', () => {
  const match = shell.match(/function canonicalTopicHash\(value\) \{([\s\S]*?)\n    \}/);
  assert.ok(match,'topic normalization must remain a single tested helper');
  const normalize = new Function('value',match[1]);
  const digits='55c129f6fa06ecf4a974fd5bcd432a8b3e4640c5';
  for(const source of [digits,'0x'+digits,digits.toUpperCase()])
    assert.equal(normalize(source),'0x'+digits);
  for(const source of ['0x123','evil'+digits, '0x'+'g'.repeat(40),''])
    assert.equal(normalize(source),'');
  assert.match(shell,/root\.canonicalTopicHash\(cast\.castId\)/);
  assert.match(shell,/root\.canonicalTopicHash\(membership\.cast_hash\)/);
  assert.match(bridge,/cast\.insert\(QStringLiteral\("castId"\), hash\);/);
});

test('topic preview hydration uses a bounded provider prefetch', () => {
  const start=runtime.indexOf('PresentationBridge::topicHydrationRequested');
  const end=runtime.indexOf('PresentationBridge::bookmarkHydrationRequested',start);
  assert.ok(start>0 && end>start);
  assert.match(runtime.slice(start,end),/ReadIntent::Prefetch/);
  assert.match(runtime.slice(start,end),/hash, ownerFid/);
});

test('read-only code blocks shrink with short snippets', () => {
  assert.match(code,/codeText\.implicitHeight \+ header\.implicitHeight/);
  assert.doesNotMatch(code,/Math\.max\(118,/);
});
