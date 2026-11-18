import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=p=>readFileSync(new URL('../../'+p,import.meta.url),'utf8');

test('Farcaster hosted manifests retain publisher-domain ownership after official HTTPS redirect',()=>{
  const runtime=read('ui/app/miniapp/runtime.cpp');
  assert.match(runtime,/manifest\.setPath\(QStringLiteral\("\/\.well-known\/farcaster\.json"\)\)/);
  assert.match(runtime,/bool allowedManifestRedirect\(const QUrl& current,/);
  assert.match(runtime,/!publicHttps\(destination\) \|\| !publicHttps\(current\)/);
  assert.match(runtime,/destination\.host\(\)\.compare\(current\.host\(\), Qt::CaseInsensitive\) == 0/);
  assert.match(runtime,/current\.host\(\)\.compare\(publisherHost, Qt::CaseInsensitive\) == 0/);
  assert.match(runtime,/destination\.host\(\)\.compare\(QStringLiteral\("api\.farcaster\.xyz"\)/);
  assert.match(runtime,/destination\.path\(\)\.startsWith\(QStringLiteral\("\/miniapps\/hosted-manifest\/"\)\)/);
  assert.match(runtime,/!destination\.hasQuery\(\) && !destination\.hasFragment\(\)/);
  assert.match(runtime,/if \(!allowedManifestRedirect\(url, destination, requestedLaunch\.host\(\)\)\)/);
  assert.match(runtime,/resolveManifest\(\*active, destination, redirects \+ 1, requestedLaunch\)/);
  assert.match(runtime,/validAccountAssociationEnvelope\(manifest, requestedLaunch\.host\(\)\)/);
  assert.doesNotMatch(runtime,/validAccountAssociationEnvelope\(manifest, url\.host\(\)\)/);
});

test('Manifest requirements enforce backend-confirmed Base Swap and never silently advertise unavailable capabilities',()=>{
  const runtime=read('ui/app/miniapp/runtime.cpp');
  assert.match(runtime,/QStringLiteral\("wallet\.getEthereumProvider"\)/);
  assert.match(runtime,/if \(!missingChains\.isEmpty\(\) \|\| !missingCapabilities\.isEmpty\(\)\)/);
  assert.match(runtime,/unsupportedRequirementsText\(missingCapabilities, missingChains\)/);
  assert.match(runtime,/if \(!validAccountAssociationEnvelope\(manifest, requestedLaunch\.host\(\)\)\)/);
  const caps=runtime.slice(runtime.indexOf('[[nodiscard]] QJsonArray capabilities('),
                           runtime.indexOf('[[nodiscard]] QJsonArray chains('));
  assert.match(caps,/if \(supportsSwap && wallet != nullptr && wallet->available\(\)\)/);
  assert.match(caps,/std::string\{"eip155:8453"\}/);
  assert.match(caps,/actions\.swapToken/);
  assert.match(runtime,/!m_controller.swapAvailabilityResolved\(\)/);
  assert.match(runtime,/active->swapReadinessRetries\+\+ < 12/);
});

test('Selected Mini App icons use foreground derived from their blue surface, not neutral CTA fill',()=>{
  const theme=read('ui/theme/Theme.qml');
  const apps=read('ui/components/apps/AppsSurface.qml');
  const multi=read('ui/components/controls/GenyMultiSelect.qml');
  const setup=read('ui/components/setup/InitialSetup.qml');
  assert.match(theme,/readonly property color accentInk: contrastInk\(presetFill\)/);
  assert.match(theme,/readonly property color accentSurfaceInk: contrastInk\(presetAccent\)/);
  assert.match(apps,/fillColor: destinationCard\.selected \? Theme\.accent : Theme\.surfaceRaised/);
  assert.match(apps,/color: destinationCard\.selected \? Theme\.accentSurfaceInk : Theme\.inkMuted/);
  assert.match(multi,/fillColor: optionButton\.selected \? Theme\.accent[\s\S]*?color: Theme\.accentSurfaceInk/);
  assert.match(setup,/id: completionBadge[\s\S]*?fillColor: Theme\.accent[\s\S]*?color: Theme\.accentSurfaceInk/);
  assert.match(apps,/text: Theme\.iconCheck[\s\S]*?color: Theme\.accent/);
});


test('legacy signed Farcaster frame.version next is accepted without weakening current Mini App v1 rules',()=>{
  // Live publisher example: f.upon.ly/.well-known/farcaster.json currently
  // exposes an accountAssociation + frame.version "next" in Neynar's index.
  const runtime=read('ui/app/miniapp/runtime.cpp');
  assert.match(runtime,/const bool currentMiniApp = manifest\.value\(QStringLiteral\("miniapp"\)\)\.isObject\(\)/);
  assert.match(runtime,/const QString version = app\.value\(QStringLiteral\("version"\)\)\.toString\(\)/);
  assert.match(runtime,/version == QStringLiteral\("1"\)/);
  assert.match(runtime,/\(!currentMiniApp && version == QStringLiteral\("next"\)\)/);
  assert.match(runtime,/if \(!supportedVersion/);
  assert.match(runtime,/validAccountAssociationEnvelope\(manifest, requestedLaunch\.host\(\)\)/);
  assert.match(runtime,/if \(!missingChains\.isEmpty\(\) \|\| !missingCapabilities\.isEmpty\(\)\)/);
  assert.match(runtime,/const bool supportedVersion = version == QStringLiteral\("1"\)/);
});
