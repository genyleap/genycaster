import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const source = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('inactive windows preserve inline emoji frames while motion is paused', () => {
  const emoji = source('ui/components/AnimatedEmoji.qml');
  const rich = source('ui/components/RichInlineText.qml');
  const sprite = source('ui/components/SpriteEmoji.qml');
  const motion = emoji.slice(emoji.indexOf('readonly property bool motionActive:'),
                             emoji.indexOf('function resolveHosts()'));
  const artwork = emoji.slice(emoji.indexOf('readonly property bool artworkWanted:'),
                              emoji.indexOf('readonly property string assetSource:'));
  assert.match(motion, /Qt\.application\.state === Qt\.ApplicationActive/);
  assert.doesNotMatch(artwork, /Qt\.application\.state/);
  assert.match(artwork, /root\.preserveArtwork/);
  assert.doesNotMatch(rich, /active: root\.visible[^;]*&& Qt\.application\.state/s);
  const spriteArtwork = sprite.slice(sprite.indexOf('readonly property bool artworkWanted:'),
                                     sprite.indexOf('readonly property bool animationReady:'));
  assert.doesNotMatch(spriteArtwork, /Qt\.application\.state/);
  assert.match(sprite, /running: root\.playbackLoops !== 0/);
  assert.match(sprite, /&& root\.artworkWanted\s+onStatusChanged:/);
});

test('saved signer verification never requires a new custody approval', () => {
  const bridgeH = source('src/presentation/qt/presentation_bridge.hpp');
  const bridge = source('src/presentation/qt/presentation_bridge.cpp');
  const runtime = source('ui/mobile/runtime_ui_bindings.cpp');
  const shell = source('ui/components/AppShell.qml');
  assert.match(bridgeH, /Q_INVOKABLE void refreshSigner\(\)/);
  assert.match(bridgeH, /void signerRefreshRequested\(\)/);
  assert.match(bridge, /void PresentationBridge::refreshSigner\(\)/);
  assert.match(runtime, /&PresentationBridge::signerRefreshRequested/);
  const retryStart = runtime.indexOf('&PresentationBridge::signerRefreshRequested');
  const retry = runtime.slice(retryStart,
                              runtime.indexOf('&PresentationBridge::networkReachabilityChanged', retryStart));
  assert.match(retry, /signerService->restore\(session, signerObserver\)/);
  assert.match(retry, /browserSigner->start\(QStringLiteral\("restore"\)/);
  assert.doesNotMatch(retry, /signerService->provision|nativeRecoveryRequested|request\.requestFid/);
  assert.match(runtime, /bridge\.refreshSigner\(\)/);
  assert.match(shell, /onClicked: canRetryVerification \? presentationBridge\.refreshSigner\(\)/);
});
