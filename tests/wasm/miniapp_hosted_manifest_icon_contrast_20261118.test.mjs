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

test('Manifest requirements remain enforced: never silently advertise unimplemented swaps',()=>{
  const runtime=read('ui/app/miniapp/runtime.cpp');
  assert.match(runtime,/QStringLiteral\("wallet\.getEthereumProvider"\)/);
  assert.match(runtime,/if \(!missingChains\.isEmpty\(\) \|\| !missingCapabilities\.isEmpty\(\)\)/);
  assert.match(runtime,/unsupportedRequirementsText\(missingCapabilities, missingChains\)/);
  assert.match(runtime,/if \(!validAccountAssociationEnvelope\(manifest, requestedLaunch\.host\(\)\)\)/);
  const caps=runtime.slice(runtime.indexOf('[[nodiscard]] QJsonArray capabilities('),
                           runtime.indexOf('[[nodiscard]] QJsonArray chains('));
  assert.doesNotMatch(caps,/actions\.swapToken/);
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
