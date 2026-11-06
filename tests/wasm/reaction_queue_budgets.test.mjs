import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

const read=path=>readFileSync(new URL('../../'+path, import.meta.url),'utf8');
const qml=read('ui/components/CastReactions.qml');
const adapter=read('ui/app/browser_reaction_controller.cpp');
const header=read('ui/app/browser_reaction_controller.hpp');
const bar=read('ui/components/EngagementBar.qml');
const cmake=read('ui/CMakeLists.txt');

test('WASM reaction admission is C++ owned and bounded rather than QML network state',()=>{
  assert.doesNotMatch(qml,/XMLHttpRequest|\.open\(|\.send\(|fetch\(|JSON\.parse/);
  assert.match(qml,/browserReactionController/);
  assert.match(qml,/function retain\(castId, viewerFid\)/);
  assert.match(qml,/function release\(castId, viewerFid\)/);
  assert.match(adapter,/maximumPending = 32/);
  assert.match(adapter,/maximumBatch = 8/);
  assert.match(adapter,/maximumEntries = 192/);
  assert.match(adapter,/maximumResponseBytes = 64 \* 1024/);
  assert.match(adapter,/requestDeadlineMs = 8 \* 1000/);
  assert.match(adapter,/m_pending\.removeAll\(key\)/);
  assert.match(bar,/onBrowserInterestKeyChanged: requestBrowserReactions\(\)/);
  assert.match(bar,/CastReactions\.release\(/);
  assert.match(cmake,/farcaster_browser_reactions_tests/);
  assert.match(header,/Q_PROPERTY\(quint64 revision/);
});

test('verified positive and local optimistic reactions remain independently owned',()=>{
  assert.match(adapter,/if \(!it->liked && item.value\(QStringLiteral\("liked"\)\)\.toBool\(false\)\)/);
  assert.match(adapter,/if \(!it->recasted && item.value\(QStringLiteral\("recasted"\)\)\.toBool\(false\)\)/);
  assert.match(adapter,/if \(const auto override = m_local\.constFind/);
  assert.match(adapter,/m_local\.size\(\) >= maximumLocal/);
  assert.match(adapter,/return \{\};/);
  assert.match(adapter,/canonicalKey\(castId, viewerFid\)/);
  assert.match(bar,/CastReactions\.setLocal\(root.reactionCastId/);
  assert.doesNotMatch(qml,/positive:|pending:|retryAfter:|busy:/);
  // The actual queue, identity, pooling and HTTP behavior is asserted by the
  // compiled Qt local-socket fixture ui.browser_reactions_contract.
  assert.match(read('ui/tests/browser_reaction_controller_tests.cpp'),
    /Unexpected unbounded reaction request/);
});
