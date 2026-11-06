import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('token preview has no QML network or JSON and honors viewport demand',()=>{
  const qml=read('ui/components/TokenAddressPreview.qml');
  const embed=read('ui/components/EmbedPreview.qml');
  const adapter=read('ui/app/browser_token_preview_controller.cpp');
  const main=read('ui/app/main.cpp');
  assert.doesNotMatch(qml,/XMLHttpRequest|JSON\.parse\(|\.send\(/);
  assert.match(qml,/browserTokenPreviewController\.tokenFor\(locator\)/);
  assert.match(qml,/browserTokenPreviewController\.requestToken\(locator\)/);
  assert.match(qml,/function onTokenReady\(locator, snapshot\)/);
  assert.match(qml,/root\.requestedLocator !== String\(locator\)/);
  assert.match(qml,/if \(!root\.enrichmentActive\)/);
  assert.match(embed,/enrichmentActive: root.visible && !root.scrollMoving/);
  assert.match(main,/setContextProperty\([\s\S]*?browserTokenPreviewController/);
  assert.match(adapter,/maximumPending\{16\}/);
  assert.match(adapter,/maximumEntries\{128\}/);
  assert.match(adapter,/maximumResponseBytes\{32 \* 1024\}/);
  assert.match(adapter,/deadlineMs\{7 \* 1000\}/);
});

test('browser token snapshots require exact chain+address and preserve verified local state',()=>{
  const cpp=read('ui/app/browser_token_preview_controller.cpp');
  const header=read('ui/app/browser_token_preview_controller.hpp');
  const qml=read('ui/components/TokenAddressPreview.qml');
  assert.match(cpp,/base\|eth/);
  assert.match(cpp,/\[0-9a-f\]\{40\}/);
  assert.match(cpp,/obj\.value\(QStringLiteral\("network"\)\)\.toString\(\)\.toLower\(\)/);
  assert.match(cpp,/obj\.value\(QStringLiteral\("address"\)\)\.toString\(\)\.toLower\(\)/);
  assert.match(cpp,/m_retryAfter\.insert\(locator/);
  assert.match(cpp,/Never overwrite a previously verified count\/price/);
  assert.match(header,/void tokenReady\(const QString& locator/);
  assert.match(qml,/if \(!root\.snapshot\.available\)/);
});
