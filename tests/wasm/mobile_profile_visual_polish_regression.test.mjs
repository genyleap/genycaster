import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=file=>readFileSync(new URL('../../'+file,import.meta.url),'utf8');
test('Profile editor uses application design-system text fields, media previews and advanced URL access',()=>{
 const qml=read('ui/components/ProfileSurface.qml');
 assert.match(qml,/DirectionAwareTextField \{\s*id: displayProtocolEditor/);
 assert.match(qml,/DirectionAwareTextArea \{\s*id: bioProtocolEditor/);
 assert.match(qml,/DirectionAwareTextField \{\s*id: websiteProtocolEditor/);
 assert.match(qml,/Avatar \{[\s\S]*?sourceUrl: avatarProtocolEditor\.text\.trim\(\)/);
 assert.match(qml,/SuperellipseImage \{[\s\S]*?linkPreviewResolver\.imageSourceForUrl\(bannerProtocolEditor\.text\.trim\(\)\)/);
 assert.match(qml,/visible: root\.editProfileMediaUrls/);
 assert.match(qml,/id: linksEditor[\s\S]*?wrapMode: TextEdit\.WrapAnywhere/);
 assert.match(qml,/genycaster\.profile\.save_additional_details/);
});
test('Channel artwork is essential small UI imagery with controlled network admission',()=>{
 const avatar=read('ui/components/Avatar.qml');
 const compose=read('ui/components/ComposerDrawer.qml');
 const cast=read('ui/components/CastItem.qml');
 const detail=read('ui/components/DetailSurface.qml');
 assert.match(avatar,/forceAutomaticDownload: root\.forceAutomaticDownload/);
 assert.match(compose,/avatarSize: 39[\s\S]*?forceAutomaticDownload: true/);
 assert.match(cast,/sourceUrl: root\.channelImageUrl\s*forceAutomaticDownload: true/);
 assert.match(detail,/sourceUrl: root\.resolvedChannelImage\s*forceAutomaticDownload: true/);
});
test('Favicon uses bounded image provider and handles transparent failure response',()=>{
 const row=read('ui/components/ProfileLinkRow.qml');
 assert.match(row,/dataStorage\.remoteImageSource\(root\.faviconUrl, "avatars", true\)/);
 assert.match(row,/implicitWidth <= 1 && implicitHeight <= 1/);
 assert.match(row,/root\.genericLabel && root\.websiteTitle\.length > 0/);
 assert.match(row,/Object\.assign\(\{\}, native, root\.browserMetadata\)/);
});
test('Localized chrome and social connections use dedicated IDs and mirror controls',()=>{
 const chrome=read('ui/components/AppHeader.qml');
 const bar=read('ui/components/EngagementBar.qml');
 const home=read('ui/components/HomeFeedHeader.qml');
 const connections=read('ui/components/AccountConnectionsPopup.qml');
 const fa=read('ui/i18n/qml_fa_IR.ts');
 assert.match(chrome,/font\.family: Theme\.fontFamilyForText\(text\)/);
 assert.match(bar,/objectName: "engagementBar"[\s\S]*?layoutDirection: Theme\.uiDirection/);
 assert.match(home,/genycaster\.home\.scope_following/);
 assert.match(home,/genycaster\.home\.scope_everyone/);
 assert.match(home,/genycaster\.home\.scope_feed/);
 assert.match(connections,/genycaster\.connections\.copy_edit_connections/);
 assert.match(connections,/genycaster\.connections\.copy_saved_account_links/);
 for(const k of ['genycaster.home.scope_following','genycaster.profile.edit_image_url',
                 'genycaster.profile.extras_heading','genycaster.connections.copy_professional_profile'])
  assert.ok(fa.includes('id="'+k+'"'),k);
});