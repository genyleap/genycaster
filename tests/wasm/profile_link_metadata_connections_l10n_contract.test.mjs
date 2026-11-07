import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('profile links use shared Qt metadata resolver for favicon/title/description on drawer open',()=>{
  const profile=read('ui/components/ProfileSurface.qml');
  const link=read('ui/components/ProfileLinkRow.qml');
  const cmake=read('ui/CMakeLists.txt');
  assert.match(profile,/id: profileLinksDrawer[\s\S]*?function requestMetadata\(url\)[\s\S]*?linkPreviewResolver\.requestPreview/);
  assert.match(profile,/onOpened: \{[\s\S]*?requestMetadata/);
  assert.match(profile,/browserLinkMetadataController\.requestPreview\(url, "link"\)/);
  assert.match(profile,/delegate: ProfileLinkRow/);
  assert.match(link,/linkPreviewResolver\.previewFor\(root\.targetUrl\)/);
  assert.match(link,/readonly property string faviconUrl:/);
  assert.match(link,/linkPreviewResolver\.imageSourceForUrl\(/);
  assert.match(link,/metadata\.siteName \|\| metadata\.title/);
  assert.match(link,/metadata\.summary \|\| metadata\.description/);
  assert.match(link,/browserLinkMetadataController\.previewFor\(root\.targetUrl, "link"\)/);
  assert.match(link,/function onPreviewReady\(url, kind, preview\)/);
  assert.match(read('ui/app/link_preview_resolver.cpp'),/#if defined\(Q_OS_WASM\)[\s\S]*?Metadata parsing requires a native worker/);
  assert.doesNotMatch(link,/\b(?:fetch|XMLHttpRequest|localStorage|document|window)\s*[.(]/);
  assert.match(cmake,/components\/ProfileLinkRow\.qml/);
  const native=read('ui/app/main.cpp');
  assert.match(native,/LinkPreviewResolver linkPreviewResolver/);
  assert.match(native,/\{"linkPreviewResolver", &linkPreviewResolver\}/);
});

test('bookmark priority action is clear, not a location marker, and missing previews are actionable',()=>{
  const surface=read('ui/components/BookmarksSurface.qml');
  assert.match(surface,/objectName: "bookmarkPriorityMobileButton"[\s\S]*?iconGlyph: Theme\.iconPriorityStar/);
  assert.doesNotMatch(surface,/iconGlyph: Theme\.iconLocation/);
  assert.match(surface,/onOpenRequested: castId => root.castRequested\(castId\)/);
  assert.match(surface,/destructive: true[\s\S]*?genycaster\.bookmarks\.remove/);
});

test('connections distinguish offered networks from confirmed links across devices',()=>{
  const picker=read('ui/components/AccountConnectionsPopup.qml');
  const controller=read('ui/app/openproof/connections.cpp');
  assert.match(picker,/readonly property var orderedProviders:/);
  assert.match(picker,/readonly property int linkedCount:/);
  assert.match(picker,/model: picker\.orderedProviders/);
  assert.match(picker,/genycaster\.connections\.available_networks/);
  assert.match(controller,/const auto confirmed=row\.value\(QStringLiteral\("last_confirmed_at"\)\)/);
  assert.match(controller,/if\(!confirmed\.isString\(\)/);
  assert.match(controller,/bool serverVerifiedFarcaster=false/);
  assert.match(controller,/if\(!serverVerifiedFarcaster \|\| m_actionBusy/);
  assert.match(controller,/if\(retained!=m_connections\)/);
  assert.match(read('services/account-sync/pg_store.mjs'),/last_confirmed_at/);
});

test('bookmarks, profile links and network labels have translations in every shipped locale',()=>{
  for(const locale of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
                       'pt_BR','ru_RU','hi_IN','ja_JP','ko_KR','zh_CN']){
    const catalog=read('ui/i18n/qml_'+locale+'.ts');
    for(const id of ['genycaster.bookmarks.pin','genycaster.bookmarks.unpin',
      'genycaster.bookmarks.open_saved_cast','genycaster.profile.links_title',
      'genycaster.profile.website','genycaster.connections.connected_accounts',
      'genycaster.connections.available_networks']){
      assert.match(catalog,new RegExp('<message id="'+id.replaceAll('.','\\.')+'">[\\s\\S]*?<translation>[^<]+<\\/translation>'));
    }
  }
});
