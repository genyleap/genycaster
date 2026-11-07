import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('WASM channel imagery uses the existing first-party media gateway rather than a missing SPA proxy',()=>{
  const main=read('ui/app/main.cpp');
  const backend=read('ui/app/social/channels.cpp');
  const gateway=read('services/media-gateway/gateway.mjs');
  assert.match(main,/channelDirectoryBaseUrl = mediaGatewayBaseUrlText/);
  assert.match(main,/channelDirectoryBaseUrl \+= QStringLiteral\("\/v1\/browser\/channels"\)/);
  assert.match(main,/GENYCASTER_CHANNEL_DIRECTORY_BASE_URL/);
  assert.match(backend,/requestDirectory\(QStringLiteral\("search-channels"\)/);
  assert.match(backend,/m_directoryMetadata\.insert/);
  assert.match(backend,/m_baseUrl = m_baseUrl\.trimmed\(\)/);
  assert.match(gateway,/\/v1\/browser\/channels\/search-channels/);
  assert.match(gateway,/\/v1\/browser\/channels\/channel/);
  assert.match(gateway,/access-control-allow-origin', '\*'/);
  assert.doesNotMatch(read('ui/components/composer/ComposerDrawer.qml'),/XMLHttpRequest|channel-icons\.json/);
});

test('cast text supports native long press without blocking native touch scrolling',()=>{
  const cpp=read('src/presentation/qt/rich_inline_text_item.cpp');
  const header=read('src/presentation/qt/rich_inline_text_item.hpp');
  assert.match(header,/QTimer m_pressHoldTimer/);
  assert.match(cpp,/m_pressHoldTimer\.setInterval\(460\)/);
  assert.match(cpp,/selectWordAt\(m_pressPosition\.x\(\), m_pressPosition\.y\(\)\)/);
  assert.match(cpp,/event->source\(\) != Qt::MouseEventNotSynthesized/);
  assert.match(cpp,/setKeepMouseGrab\(true\)/);
  assert.match(cpp,/setKeepTouchGrab\(true\)/);
  assert.match(cpp,/void RichInlineTextItem::touchEvent\(QTouchEvent\* event\)/);
  assert.match(cpp,/setKeepTouchGrab\(false\)/);
  assert.match(cpp,/void RichInlineTextItem::mouseUngrabEvent\(\)/);
  assert.match(cpp,/m_pressHoldTimer\.stop\(\)/);
});

test('copy uses browser Clipboard API under WASM user interaction and labels lazily instantiate a selection editor',()=>{
  const bridge=read('src/presentation/qt/presentation_bridge.cpp');
  const label=read('ui/components/controls/BidiLabel.qml');
  assert.match(bridge,/navigator\.clipboard/);
  assert.match(bridge,/clipboard\.writeText\(payload\)/);
  assert.match(bridge,/QGuiApplication::clipboard\(\)/);
  assert.match(label,/selectionOverlay/);
  assert.match(label,/active: control.selectionActive/);
  assert.match(label,/TextEdit \{/);
  assert.match(label,/readOnly: true/);
  assert.match(label,/Component.onCompleted: selectAll\(\)/);
  assert.match(label,/presentationBridge.copyText\(/);
  assert.doesNotMatch(label,/textInteractionFlags\s*:/);
});
