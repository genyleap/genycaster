import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('profile links use shared Qt metadata resolver for favicon/title/description on drawer open',()=>{
  const profile=read('ui/components/ProfileSurface.qml');
  const link=read('ui/components/ProfileLinkRow.qml');
  const cmake=read('ui/CMakeLists.txt');
  assert.match(profile,/id: profileLinksDrawer[\s\S]*?onOpened: \{[\s\S]*?linkPreviewResolver\.requestPreview/);
  assert.match(profile,/delegate: ProfileLinkRow/);
  assert.match(link,/linkPreviewResolver\.previewFor\(root\.targetUrl\)/);
  assert.match(link,/readonly property string faviconUrl: String\(metadata\.favicon/);
  assert.match(link,/metadata\.siteName \|\| metadata\.title/);
  assert.match(link,/metadata\.summary \|\| metadata\.description/);
  assert.doesNotMatch(link,/\b(?:fetch|XMLHttpRequest|localStorage|document|window)\s*[.(]/);
  assert.match(cmake,/components\/ProfileLinkRow\.qml/);
  const native=read('ui/mobile/main.cpp');
  assert.match(native,/LinkPreviewResolver linkPreviewResolver/);
  assert.match(native,/setContextProperty\(QStringLiteral\("linkPreviewResolver"\), &linkPreviewResolver\)/);
});

test('bookmark priority action is clear, not a location marker, and missing previews are actionable',()=>{
  const surface=read('ui/components/BookmarksSurface.qml');
  assert.match(surface,/objectName: "bookmarkPriorityButton"[\s\S]*?iconGlyph: Theme\.iconSpark/);
  assert.doesNotMatch(surface,/iconGlyph: Theme\.iconLocation/);
  assert.match(surface,/genycaster\.bookmarks\.open_saved_cast/);
  assert.match(surface,/destructive: true[\s\S]*?genycaster\.bookmarks\.remove/);
});

test('connections distinguish offered networks from confirmed links across devices',()=>{
  const picker=read('ui/components/AccountConnectionsPopup.qml');
  const controller=read('ui/mobile/openproof_connections_controller.cpp');
  assert.match(picker,/readonly property var orderedProviders:/);
  assert.match(picker,/readonly property int linkedCount:/);
  assert.match(picker,/model: picker\.orderedProviders/);
  assert.match(picker,/genycaster\.connections\.available_networks/);
  assert.match(controller,/row\.value\(QStringLiteral\("last_confirmed_at"\)\)\.isNull\(\)/);
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
