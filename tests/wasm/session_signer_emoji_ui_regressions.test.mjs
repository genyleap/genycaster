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

test('Mini App rows reflow and keep Resume text readable at compact widths', () => {
  const apps = source('ui/components/AppsSurface.qml');
  const manager = source('ui/components/TaskManagerPopup.qml');
  assert.match(apps, /readonly property bool mobileActionsStacked:/);
  assert.match(apps, /columns: cardCell\.mobileActionsStacked \? 1 : 2/);
  assert.match(apps, /Layout\.minimumWidth: Math\.max\(96, implicitWidth\)/);
  assert.match(apps, /Layout\.minimumWidth: Math\.max\(104, implicitWidth\)/);
  assert.match(manager, /readonly property bool narrow:/);
  assert.match(manager, /Layout\.minimumWidth: 0/);
  assert.match(manager, /Layout\.preferredHeight: Theme\.scaleUi\(62\)/);
});

test('inline emoji reserve ink space and replace Unicode only in their live layout', () => {
  const rich = source('ui/components/RichInlineText.qml');
  const textView = source('ui/components/RichTextView.qml');
  const cpp = source('src/presentation/qt/rich_inline_text_item.cpp');
  const header = source('src/presentation/qt/rich_inline_text_item.hpp');
  assert.match(rich, /readonly property real emojiVerticalInset:/);
  assert.match(rich, /clip: false/);
  assert.match(textView, /clip: false/);
  assert.match(rich, /opacity: animationReady \? 1 : 0/);
  assert.match(rich, /onEmojiGenerationChanged: emojiOverlay\.claimFallbackOwnership\(\)/);
  assert.match(rich, /objectName: "richInlineEmojiStaticFallback"/);
  assert.match(rich, /visible: !emojiOverlay\.artworkReady/);
  assert.match(rich, /renderType: Text\.NativeRendering/);
  assert.doesNotMatch(rich, /onArtworkReadyChanged: emojiOverlay\.syncAnimatedState\(\)/);
  assert.match(rich, /textLayout\.setEmojiAnimated\(emojiPosition, emojiGeneration/);
  assert.match(header, /setEmojiAnimated\(int position, int generation, bool animated\)/);
  assert.match(cpp, /generation != m_impl->emojiRectGeneration/);
  assert.match(cpp, /handler\.animatedPositions\.clear\(\)/);
});


test('completed feed emoji releases the Unicode glyph without blinking on scroll', () => {
  const policy = source('src/application/emoji/animation_policy.cpp');
  const emoji = source('ui/components/AnimatedEmoji.qml');
  const lottie = source('ui/components/AnimatedEmojiLottie.qml');
  const sprite = source('ui/components/SpriteEmoji.qml');
  const rich = source('ui/components/RichInlineText.qml');
  const feed = policy.slice(policy.indexOf('case Surface::Feed:'),
                            policy.indexOf('case Surface::PickerCell:'));
  assert.match(feed, /PlaybackMode::PlayOnce/);
  assert.match(feed, /pauseWhenInvisible = true/);
  assert.match(emoji, /readonly property int configuredLoops:/);
  assert.match(emoji, /readonly property int playbackLoops: root\.motionActive \? root\.configuredLoops : 0/);
  assert.match(emoji, /lottie\.item\.playCount = root\.configuredLoops === 0 \? 1 : root\.configuredLoops/);
  assert.match(emoji, /lottie\.item\.animationActive = root\.playbackLoops !== 0/);
  assert.match(emoji, /!lottie\.item\.playbackCompleted/);
  assert.match(lottie, /animation\.runningChanged\.connect/);
  assert.match(lottie, /root\.playbackCompleted = true/);
  assert.match(lottie, /interval: 100/);
  assert.match(sprite, /property bool playbackFinished: false/);
  assert.match(sprite, /!root\.requestFailed && !root\.playbackFinished/);
  assert.match(sprite, /finishBehavior: AnimatedSprite\.FinishAtFinalFrame/);
  assert.match(sprite, /onFinished:/);
  assert.match(sprite, /opacity: root\.animationReady \? 1 : 0/);
  assert.match(rich, /onEmojiGenerationChanged: emojiOverlay\.claimFallbackOwnership\(\)/);
  assert.match(rich, /textLayout\.setEmojiAnimated\(emojiPosition, emojiGeneration, true\)/);
  assert.match(rich, /visible: !emojiOverlay\.artworkReady/);
  assert.match(rich, /renderType: Text\.NativeRendering/);
});


test('inline emoji remain painted while Lottie starts, finishes, or is recycled', () => {
  const rich = source('ui/components/RichInlineText.qml');
  const cpp = source('src/presentation/qt/rich_inline_text_item.cpp');
  const qmlFallback = rich.slice(rich.indexOf('Label {\n                id: inlineStaticEmoji'),
                                rich.indexOf('AnimatedEmoji {\n                id: lottieEmoji'));
  const ownership = rich.slice(rich.indexOf('function claimFallbackOwnership()'),
                               rich.indexOf('x: Number(emojiRect.x'));
  assert.match(qmlFallback, /visible: !emojiOverlay\.artworkReady/);
  assert.match(qmlFallback, /text: String\(emojiOverlay\.emojiRect\.sequence \|\| ""\)/);
  assert.match(qmlFallback, /textFormat: Text\.PlainText/);
  assert.match(qmlFallback, /renderType: Text\.NativeRendering/);
  assert.match(qmlFallback, /clip: false/);
  // The native text renderer is suppressed only when a QML delegate with its
  // own guaranteed glyph is alive, never because the animation became ready.
  assert.match(ownership, /setEmojiAnimated\(emojiPosition, emojiGeneration, true\)/);
  assert.doesNotMatch(ownership, /artworkReady/);
  assert.match(rich, /Component\.onDestruction:[\s\S]*?setEmojiAnimated\(emojiPosition, emojiGeneration, false\)/);
  assert.match(cpp, /generation != m_impl->emojiRectGeneration/);
  assert.match(rich, /model: root\.usePlatformSelection \? \[\] : textLayout\.emojiRects/);
});


test('profile public activity remains null-safe through service teardown and profile switches', () => {
  const qml = source('ui/components/ProfileSurface.qml');
  assert.match(qml, /readonly property bool publicActivityMatchesProfile: Boolean\(openProofConnections/);
  assert.match(qml, /Number\(openProofConnections\.publicActivityFid\)/);
  assert.match(qml, /readonly property string publicActivityGithubLogin: root\.publicActivityMatchesProfile/);
  assert.match(qml, /readonly property string publicActivitySource: root\.publicActivityMatchesProfile/);
  assert.match(qml, /String\(openProofConnections\.publicActivitySource \|\| "casts"\)/);
  assert.doesNotMatch(qml, /publicActivitySource: root\.publicActivityMatchesProfile\s*\? root\.publicActivitySource/);
  assert.doesNotMatch(qml, /:\s*Number\(openProofConnections\.publicActivityFid\)/);
});
