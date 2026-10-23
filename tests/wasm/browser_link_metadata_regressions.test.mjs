import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('link and embed metadata run through one Qt adapter, not QML HTTP',()=>{
  const link=read('ui/components/LinkPreview.qml');
  const embed=read('ui/components/EmbedPreview.qml');
  const cpp=read('ui/mobile/browser_link_metadata_controller.cpp');
  const main=read('ui/mobile/main.cpp');
  const cmake=read('ui/CMakeLists.txt');
  for(const qml of [link,embed]){
    assert.doesNotMatch(qml,/XMLHttpRequest|JSON\.parse\(|\.send\(/);
    assert.match(qml,/browserLinkMetadataController\.requestPreview\(target, "link"\)/);
    assert.match(qml,/function onPreviewReady\(url, kind, preview\)/);
    assert.match(qml,/root\.scrollMoving/);
  }
  assert.match(link,/browserLinkMetadataController\.requestPreview\(target, "miniapp"\)/);
  assert.doesNotMatch(link,/metadata\.miniAppLaunchUrl = root\.targetUrl/);
  assert.match(link,/root\.previewData\.miniAppLaunchUrl \|\| root\.targetUrl/);
  assert.match(cpp,/query\.addQueryItem\(QStringLiteral\("url"\),request\.url\)/);
  assert.match(embed,/String\(url\) !== root\.targetUrl\.split\("#"\)\[0\]/);
  assert.match(main,/setContextProperty\([\s\S]*?browserLinkMetadataController/);
  assert.match(cmake,/farcaster_browser_link_metadata_tests/);
  assert.match(cpp,/maximumQueued\{32\}/);
  assert.match(cpp,/maximumActive\{2\}/);
  assert.match(cpp,/maximumResponseBytes\{96 \* 1024\}/);
  assert.match(cpp,/requestDeadlineMs\{7 \* 1000\}/);
  assert.match(cpp,/miniAppRequestDeadlineMs\{12 \* 1000\}/);
  assert.match(cpp,/request.kind == QLatin1String\("miniapp"\)\s*\? miniAppRequestDeadlineMs : requestDeadlineMs/);
  assert.match(cpp,/normalizedDomain == normalizedHost/);
});

test('no browser-side metadata result is permitted to redefine a recycled cast',()=>{
  const link=read('ui/components/LinkPreview.qml');
  const embed=read('ui/components/EmbedPreview.qml');
  assert.match(link,/String\(root\.targetUrl \|\| ""\)\.split\("#"\)\[0\] !== String\(url\)/);
  assert.match(embed,/kind !== "link"/);
  assert.match(link,/if \(root\.scrollMoving\) root\.pendingBrowserMiniAppPreview = metadata/);
  assert.match(link,/if \(root\.scrollMoving\) root\.pendingBrowserLinkPreview = preview/);
  assert.match(embed,/root\.acceptLinkMetadata\(preview\)/);
});

test('Mini App metadata retries are bounded, delayed beyond negative cache and cancelled on URL reuse',()=>{
  const qml=read('ui/components/LinkPreview.qml');
  assert.match(qml,/property int miniAppRetryCount: 0/);
  assert.match(qml,/root\.miniAppRetryCount < 2/);
  assert.match(qml,/id: miniAppManifestRetryTimer/);
  assert.match(qml,/interval: 17500/);
  assert.match(qml,/root\.browserPreviewRequestUrl = ""\s*root\.requestBrowserMiniAppManifest\(\)/);
  assert.match(qml,/root\.miniAppRetryCount = 0\s*miniAppManifestRetryTimer\.stop\(\)/);
});
