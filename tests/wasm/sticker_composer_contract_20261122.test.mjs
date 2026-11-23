import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';

const root=process.cwd();
const read=file=>fs.readFileSync(path.join(root,file),'utf8');
const sandbox=vm.createContext({});
vm.runInContext(read('ui/components/composer/StickerCatalog.js').replace(/^\.pragma library\s*/,''),sandbox);
const {stickers,packs,packForId,stickersForPack,decodeIds,toggleFavorite,recordRecent,
       filtered,embedUrl,previewUrl,isStickerUrl,itemForId,publisherUrl,imageOrigin}=sandbox;

test('full Njal pack and licensed Geny Pro reactions have stable canonical assets',()=>{
 assert.equal(stickers.length,276);
 assert.deepEqual(Array.from(packs,x=>x.id),['njal','cream-cat-pro']);
 assert.equal(packForId('njal').count,68);
 assert.equal(packForId('cream-cat-pro').count,208);
 assert.equal(packForId('njal').tier,'free');
 assert.equal(packForId('cream-cat-pro').tier,'pro');
 assert.equal(stickersForPack('njal').length,68);
 assert.equal(stickersForPack('cream-cat-pro').length,208);
 assert.equal(new Set(Array.from(stickers,x=>x.id)).size,276);
 assert.equal(publisherUrl,'https://github.com/cryptonjal/njals-sticker-pack');
 assert.match(imageOrigin,/^https:\/\/red-geographical-clownfish-714\.mypinata\.cloud\/ipfs\/bafy[a-z0-9]+\/$/);
 for(const item of stickers){
   assert.match(item.id,/^[a-z0-9-]+$/);
   assert.match(item.sha256,/^[a-f0-9]{64}$/);
   assert.ok(item.title && item.alt);
   assert.equal(embedUrl(item.id),item.url);
   assert.equal(previewUrl(item.id),item.thumbnailUrl || item.url);
   assert.equal(isStickerUrl(item.url),true);
   if(item.pack==='njal'){
     assert.equal(item.mediaType,'image/webp');
     assert.ok(item.url.startsWith(imageOrigin));
     assert.ok(item.url.endsWith('.webp'));
   }else{
     assert.equal(item.pack,'cream-cat-pro');
     assert.equal(item.mediaType,'image/webp');
     assert.match(item.url,/^https:\/\/mssgs-stickers\.com\/files\/sticker-cream-cat-[a-z0-9-]+\.webp$/);
     assert.match(item.thumbnailUrl,/^https:\/\/mssgs-stickers\.com\/thumbs\/sticker-cream-cat-[a-z0-9-]+\.webp$/);
   }
 }
 assert.equal(itemForId('bloom'),null);
 assert.equal(embedUrl('../bad'),'');
 assert.equal(previewUrl('https://evil.test/'),'');
 assert.equal(isStickerUrl('https://example.com/smile.webp'),false);
 assert.equal(isStickerUrl(imageOrigin+'unknown.webp'),false);
 assert.equal(isStickerUrl('https://genycaster.xyz/stickers/v1.0.0/bloom.png'),true,
              'preserve historical cast compatibility');
 assert.equal(isStickerUrl('https://genycaster.xyz/stickers/twemoji-pro-v1/1f973.png'),true,
              'retired Twemoji casts must still display');
 assert.equal(packForId('twemoji-pro'),null,'retired Pro pack is no longer selectable');
 assert.equal(itemForId('twemoji-1f973'),null);
 assert.ok(!fs.existsSync(path.join(root,'ui/assets/stickers/v1')));
});

test('favorites and recents remain bounded to catalog items, independent from account installs',()=>{
 assert.equal(filtered('njal','',[],[]).length,68);
 assert.equal(filtered('cream-cat-pro','',[],[]).length,208);
 assert.deepEqual(Array.from(filtered('njal','gm',[],[]),x=>x.id),['21-gm']);
 assert.deepEqual(Array.from(filtered('cream-cat-pro','soup',[],[]),x=>x.id),['cream-cat-soup-sip']);
 assert.deepEqual(Array.from(filtered('favorites','',['21-gm','cream-cat-soup-sip'],[]),x=>x.id),
                  ['21-gm','cream-cat-soup-sip']);
 assert.deepEqual(Array.from(decodeIds(JSON.stringify(['21-gm','bloom','21-gm','cream-cat-soup-sip']),32)),
                  ['21-gm','cream-cat-soup-sip']);
 assert.deepEqual(Array.from(decodeIds('not json',12)),[]);
 assert.deepEqual(Array.from(decodeIds('"broken"',12)),[]);
 assert.deepEqual(Array.from(toggleFavorite(['21-gm'],'21-gm')),[]);
 assert.deepEqual(Array.from(toggleFavorite(['21-gm'],'cream-cat-soup-sip')),
                  ['cream-cat-soup-sip','21-gm']);
 assert.deepEqual(Array.from(recordRecent(['21-gm'],'cream-cat-soup-sip')),
                  ['cream-cat-soup-sip','21-gm']);
 assert.equal(recordRecent(stickers.map(x=>x.id),'21-gm').length,12);
 assert.deepEqual(Array.from(toggleFavorite(['21-gm'],'../bad')),['21-gm']);
});

test('Composer has server-confirmed pack and Pro guard, uses ordinary protocol embed',()=>{
 const drawer=read('ui/components/composer/ComposerDrawer.qml');
 const toolbar=read('ui/components/composer/FormattingToolbar.qml');
 const controller=read('src/presentation/qt/rich_composer_controller.cpp');
 assert.match(drawer,/function insertSticker\(sticker\)/);
 assert.match(drawer,/StickerCatalog\.embedUrl\(id\)/);
 assert.match(drawer,/openProofConnections\.stickerPacksReady/);
 assert.match(drawer,/sticker\.pack === "cream-cat-pro" && !root\.verifiedGenyPro/);
 assert.match(drawer,/root\.canUploadMedia \|\| mediaUpload\.busy/);
 assert.match(drawer,/richComposer\.insertMedia\(canonical, mediaType/);
 assert.doesNotMatch(drawer,/insertManagedMedia\(canonical/);
 assert.match(controller,/bool RichComposerController::insertMedia\(/);
 assert.match(controller,/QVariantList RichComposerController::protocolEmbeds\(/);
 assert.match(drawer,/stickerPicker\.recordUsage\(sticker\.id\)/);
 assert.match(drawer,/objectName: "mobileComposerStickerAction"/);
 assert.match(toolbar,/objectName: "desktopComposerStickerAction"/);
 assert.match(toolbar,/signal stickerRequested\(var anchor\)/);
 assert.match(toolbar,/onClicked: root\.stickerRequested\(this\)/);
 assert.match(drawer,/onStickerRequested: anchor => root\.openStickerPicker\(anchor,/);
 assert.match(drawer,/onClicked: root\.openStickerPicker\(this, 0\)/);
 assert.match(drawer,/onProRequested: \{/);
});

test('picker has library, discover, pack-detail preview, server install action and Pro gate',()=>{
 const picker=read('ui/components/composer/StickerPicker.qml');
 assert.match(picker,/signal proRequested\(\)/);
 assert.match(picker,/readonly property bool libraryReady: authorized && openProofConnections\.stickerPacksReady/);
 assert.match(picker,/readonly property var installedIds/);
 assert.match(picker,/openProofConnections\.fetchStickerPacks\(\)/);
 assert.match(picker,/openProofConnections\.setStickerPackInstalled/);
 assert.match(picker,/objectName: "installStickerPackAction"/);
 assert.match(picker,/objectName: "stickerPackPreview"/);
 assert.match(picker,/qsTrId\("genycaster\.stickers\.install_count"\)/);
 assert.match(picker,/root\.activePro && !root\.verifiedPro/);
 assert.match(picker,/root\.installedIds\.indexOf\(sticker\.pack\) < 0/);
 assert.match(picker,/openProofConnections\.proEntitlements\.eligible === true/);
 assert.match(picker,/StickerCatalog\.stickersForPack/);
 assert.match(picker,/function showLibrary\(\)/);
 assert.match(picker,/function showPack\(id\)/);
 assert.match(picker,/function togglePack\(\)/);
 assert.match(picker,/stickerPrefs\.favoritesJson/);
 assert.match(picker,/StickerCatalog\.previewUrl\(stickerTile\.modelData\.id\)/);
 assert.match(picker,/onClicked: root\.chooseSticker/);
 assert.match(picker,/presentationBridge\.openExternalUrl\(root\.activePack\.credit\)/);
 assert.doesNotMatch(picker,/source: "file:\/\//);
});

test('mobile bottom sheet and desktop popup anchor accurately, without read-only Flow',()=>{
 const picker=read('ui/components/composer/StickerPicker.qml');
 const drawer=read('ui/components/composer/ComposerDrawer.qml');
 assert.match(picker,/modal: root\.compact/);
 assert.match(drawer,/parent: root\.contentItem\s*popupType: Popup\.Item/);
 assert.doesNotMatch(drawer,/StickerPicker \{\s*id: stickerPicker\s*parent: Overlay\.overlay/);
 assert.match(picker,/dim: root\.compact/);
 assert.match(picker,/root\.anchorItem\.mapToItem\(root\.parent/);
 assert.match(picker,/const maxX = Math\.max/);
 assert.match(picker,/const maxY = Math\.max/);
 assert.match(picker,/root\.y = Math\.round\(maxY\)/);
 assert.match(picker,/Qt\.callLater\(root\.reposition\)/);
 assert.match(picker,/function onWidthChanged\(\)/);
 assert.match(picker,/function onHeightChanged\(\)/);
 assert.match(picker,/Image\.PreserveAspectFit/);
 const tray=drawer.match(/Flow \{\s*objectName: "mobileInlineStickerDrafts"([\s\S]*?)\n\s*Repeater \{/);
 assert.ok(tray);
 assert.doesNotMatch(tray[1],/^\s*implicitHeight\s*:/m);
 assert.match(tray[1],/Layout\.preferredHeight: visible \? childrenRect\.height : 0/);
});

test('transparent sticker draft and feed remain uncropped without upload duplication',()=>{
 const drawer=read('ui/components/composer/ComposerDrawer.qml');
 const media=read('ui/components/composer/MediaUploadPanel.qml');
 const editor=read('ui/components/composer/RichTextEditor.qml');
 const preview=read('ui/components/media/FarcasterPreview.qml');
 const tile=read('ui/components/composer/StickerDraftTile.qml');
 const cast=read('ui/components/feed/CastItem.qml');
 const layout=read('ui/components/media/GalleryLayout.qml');
 assert.match(drawer,/readonly property var draftStickers/);
 assert.match(drawer,/StickerDraftTile \{/);
 assert.match(media,/regularAttachedMedia: root\.attachedMedia\.filter\(item => !item\.sticker\)/);
 assert.match(editor,/readonly property bool stickerBlock/);
 assert.match(editor,/fillColor: stickerBlock \? "transparent"/);
 assert.match(preview,/StickerDraftTile \{/);
 assert.match(tile,/Image\.PreserveAspectFit/);
 assert.match(tile,/onClicked: root\.removeRequested\(\)/);
 assert.match(cast,/StickerCatalog\.isStickerUrl\(root\.normalizedEmbedUrl\(embed\)\)/);
 assert.match(layout,/StickerCatalog\.isStickerUrl\(root\.mediaKey\(0\)\)/);
});

test('Qt/QML registration and attributed Solar sticker icon and translations exist',()=>{
 const cmake=read('ui/CMakeLists.txt');
 const theme=read('ui/theme/Theme.qml');
 const map=JSON.parse(read('ui/components/source_paths.json'));
 assert.match(cmake,/components\/composer\/StickerPicker\.qml/);
 assert.match(cmake,/components\/composer\/StickerDraftTile\.qml/);
 assert.match(cmake,/components\/composer\/StickerCatalog\.js/);
 assert.equal(map['StickerPicker.qml'],'composer/StickerPicker.qml');
 assert.equal(map['StickerDraftTile.qml'],'composer/StickerDraftTile.qml');
 assert.match(theme,/iconSticker: "media\/sticker"/);
 const svg=read('ui/assets/icons/genyleap/icons/media/sticker.svg');
 assert.match(svg,/Solar Icons/);
 assert.match(svg,/527550\/sticker-smile-square/);
 assert.match(svg,/CC Attribution License/);
 assert.match(svg,/opacity="\.72"/);
 assert.match(read('ui/assets/icons/genyleap/README.md'),/Solar Icons/);
 for(const locale of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
                      'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']){
  const ts=read('ui/i18n/qml_'+locale+'.ts');
  for(const id of ['my_packs','discover','install_count','remove_pack',
      'signin','unavailable','pro_required','pro_description','unlock_pro','view_pack'])
    assert.match(ts,new RegExp('<message id="genycaster\\.stickers\\.'+id+'">'),locale+': '+id);
 }
 assert.ok(fs.existsSync(path.join(root,'scripts/import_njal_stickers.py')));
 assert.ok(fs.existsSync(path.join(root,'scripts/import_cream_cat_stickers.py')));
 assert.ok(fs.existsSync(path.join(root,'scripts/cream_cat_pack_manifest.json')));
 const tabs=read('ui/components/composer/StickerTab.qml');
 const picker=read('ui/components/composer/StickerPicker.qml');
 assert.match(tabs,/Theme\.scaleUi\(32\)/);
 assert.match(tabs,/root\.selected/);
 assert.match(tabs,/color: Theme\.accent/);
 assert.match(picker,/objectName: "stickerPrimaryTabs"/);
 assert.match(picker,/StickerTab \{/);
 assert.match(picker,/model: \["all", "favorites", "recent"\]/);
 assert.doesNotMatch(picker,/model: \["all", "favorites", "recent"\][\s\S]*?delegate: ActionButton \{/);
 const profile=read('ui/components/profile/ProfileSurface.qml');
 assert.match(profile,/readonly property bool badgesNeedOwnLine:/);
 assert.match(profile,/id: profileCountsRow/);
 assert.match(profile,/id: profileBadgesRow[\s\S]*?anchors\.right: parent\.right/);
 assert.match(profile,/height: profileStatsFlow\.rowHeight/);
 assert.doesNotMatch(profile,/profileStreakAlignmentSpacer/);
 const manifest=JSON.parse(read('scripts/cream_cat_pack_manifest.json'));
 assert.equal(manifest.stickers.length,208);
 assert.equal(manifest.credit,'Stickers by mssgs-stickers.com');
 assert.equal(new Set(manifest.stickers.map(x=>x.id)).size,208);
 for(const sticker of manifest.stickers){
  assert.match(sticker.sha256,/^[a-f0-9]{64}$/);
  assert.match(sticker.url,/^https:\/\/mssgs-stickers\.com\/files\//);
 }

});
