import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';

const root=process.cwd();
const read=file=>fs.readFileSync(path.join(root,file),'utf8');
const catalogText=read('ui/components/composer/StickerCatalog.js');
const sandbox=vm.createContext({});
vm.runInContext(catalogText.replace(/^\.pragma library\s*/,''),sandbox);
const {stickers,decodeIds,toggleFavorite,recordRecent,filtered,embedUrl,
       previewUrl,isStickerUrl,itemForId,publisherUrl,imageOrigin}=sandbox;
const selectedIds=Array.from(stickers,x=>x.id);

test('curated Njal catalog pins 32 original remote WebP images without copying unlicensed art',()=>{
 assert.equal(stickers.length,32);
 assert.equal(new Set(selectedIds).size,stickers.length);
 assert.deepEqual([...new Set(stickers.map(x=>x.pack))].sort(),['memes','reactions','web3']);
 assert.equal(publisherUrl,'https://github.com/cryptonjal/njals-sticker-pack');
 assert.match(imageOrigin,/^https:\/\/red-geographical-clownfish-714\.mypinata\.cloud\/ipfs\/bafy[a-z0-9]+\/$/);
 for(const sticker of stickers){
  assert.match(sticker.id,/^[0-9]{2}-[a-z0-9_-]+$/);
  assert.match(sticker.sha256,/^[a-f0-9]{64}$/);
  assert.equal(sticker.mediaType,'image/webp');
  assert.ok(sticker.title.length>1 && sticker.alt.length>0);
  assert.ok(sticker.url.startsWith(imageOrigin));
  assert.ok(sticker.url.endsWith('.webp'));
  assert.equal(embedUrl(sticker.id),sticker.url);
  assert.equal(previewUrl(sticker.id),sticker.url);
  assert.equal(isStickerUrl(sticker.url),true);
 }
 assert.equal(itemForId('bloom'),null);
 assert.equal(embedUrl('../bad'),'');
 assert.equal(previewUrl('https://evil.test/'),'');
 assert.equal(isStickerUrl('https://example.com/smile.webp'),false);
 assert.equal(isStickerUrl(imageOrigin+'unreviewed.webp'),false);
 assert.equal(isStickerUrl('https://genycaster.xyz/stickers/v1.0.0/bloom.png'),true,
   'historical cast images must remain visible');
 assert.ok(!fs.existsSync(path.join(root,'ui/assets/stickers/v1')));
 assert.ok(!fs.existsSync(path.join(root,'scripts/generate_stickers.py')));
 assert.ok(!fs.existsSync(path.join(root,'scripts/publish_stickers.py')));
});

test('search, favorites and recent history stay bounded to reviewed catalog entries',()=>{
 assert.equal(filtered('all','',[],[]).length,32);
 assert.equal(filtered('reactions','',[],[]).length,stickers.filter(x=>x.pack==='reactions').length);
 assert.deepEqual(Array.from(filtered('web3','gm',[],[]),x=>x.id),['21-gm']);
 assert.deepEqual(Array.from(filtered('reactions','hug',[],[]),x=>x.id),['24-hug']);
 assert.deepEqual(Array.from(filtered('favorites','', ['21-gm','24-hug'],[]),x=>x.id),
   ['21-gm','24-hug']);
 assert.deepEqual(Array.from(filtered('recent','',[],['02-big-smile']),x=>x.id),['02-big-smile']);
 assert.deepEqual(Array.from(decodeIds(JSON.stringify(['21-gm','bloom','21-gm','24-hug']),32)),
   ['21-gm','24-hug']);
 assert.deepEqual(Array.from(decodeIds('not json',12)),[]);
 assert.deepEqual(Array.from(decodeIds('"nope"',12)),[]);
 assert.deepEqual(Array.from(toggleFavorite(['21-gm'],'21-gm')),[]);
 assert.deepEqual(Array.from(toggleFavorite(['21-gm'],'24-hug')),['24-hug','21-gm']);
 assert.deepEqual(Array.from(recordRecent(['21-gm','24-hug'],'02-big-smile')),
   ['02-big-smile','21-gm','24-hug']);
 assert.equal(recordRecent(selectedIds,'21-gm').length,12);
 assert.deepEqual(Array.from(toggleFavorite(['21-gm'],'../../bad')),['21-gm']);
});

test('composer inserts a normal WebP embed through canonical protocol publishing',()=>{
 const drawer=read('ui/components/composer/ComposerDrawer.qml');
 const toolbar=read('ui/components/composer/FormattingToolbar.qml');
 const picker=read('ui/components/composer/StickerPicker.qml');
 const controller=read('src/presentation/qt/rich_composer_controller.cpp');
 assert.match(drawer,/function insertSticker\(sticker\)/);
 assert.match(drawer,/StickerCatalog\.embedUrl\(id\)/);
 assert.match(drawer,/root\.canUploadMedia \|\| mediaUpload\.busy/);
 assert.match(drawer,/richComposer\.insertMedia\(canonical, "image\/webp"/);
 assert.doesNotMatch(drawer,/insertManagedMedia\(canonical/);
 assert.match(controller,/bool RichComposerController::insertMedia\(/);
 assert.match(controller,/BlockKind::Media/);
 assert.match(controller,/QVariantList RichComposerController::protocolEmbeds\(/);
 assert.match(drawer,/stickerPicker\.recordUsage\(sticker\.id\)/);
 assert.match(drawer,/onStickerSelected: sticker/);
 assert.match(drawer,/objectName: "mobileComposerStickerAction"/);
 assert.match(toolbar,/objectName: "desktopComposerStickerAction"/);
 assert.match(toolbar,/signal stickerRequested\(var anchor\)/);
 assert.match(toolbar,/onClicked: root\.stickerRequested\(this\)/);
 assert.match(drawer,/onStickerRequested: anchor => root\.openStickerPicker\(anchor,/);
 assert.match(drawer,/onClicked: root\.openStickerPicker\(this, 0\)/);
 assert.match(picker,/Settings \{/);
 assert.match(picker,/genycaster-stickers-v1/);
 assert.match(picker,/readonly property var visibleStickers/);
 assert.match(picker,/onClicked: root\.toggleFavorite/);
 assert.match(picker,/StickerCatalog\.previewUrl\(tile\.modelData\.id\)/);
 assert.match(picker,/root\.canAdd/);
 assert.match(drawer,/!stickerPicker\.opened/);
});

test('sticker popup opens at real trigger with viewport clamping and mobile bottom sheet',()=>{
 const picker=read('ui/components/composer/StickerPicker.qml');
 assert.match(picker,/modal: root\.compact/);
 assert.match(picker,/dim: root\.compact/);
 assert.match(picker,/root\.anchorItem\.mapToItem\(root\.parent/);
 assert.match(picker,/const maxX = Math\.max/);
 assert.match(picker,/const maxY = Math\.max/);
 assert.match(picker,/root\.y = Math\.round\(maxY\)/);
 assert.match(picker,/onOpened: \{/);
 assert.match(picker,/Qt\.callLater\(root\.reposition\)/);
 assert.match(picker,/function onWidthChanged\(\)/);
 assert.match(picker,/function onHeightChanged\(\)/);
 assert.match(picker,/objectName: "stickerThumbnail"/);
 assert.match(picker,/Image\.PreserveAspectFit/);
 assert.match(picker,/presentationBridge\.openExternalUrl\(StickerCatalog\.publisherUrl\)/);
 assert.doesNotMatch(picker,/assets\/stickers\/v1/);
});

test('mobile Flow sticker tray does not assign Qt read-only implicitHeight',()=>{
 const drawer=read('ui/components/composer/ComposerDrawer.qml');
 const tray=drawer.match(/Flow \{\s*objectName: "mobileInlineStickerDrafts"([\s\S]*?)\n\s*Repeater \{/);
 assert.ok(tray, 'the compact sticker Flow must remain in the Composer');
 assert.doesNotMatch(tray[1],/^\s*implicitHeight\s*:/m,
   'Qt Quick Flow inherits a read-only implicitHeight from Positioner');
 assert.match(tray[1],/Layout\.preferredHeight: visible \? childrenRect\.height : 0/);
});

test('inline Telegram-like sticker preview avoids duplicated opaque media upload cards',()=>{
 const drawer=read('ui/components/composer/ComposerDrawer.qml');
 const media=read('ui/components/composer/MediaUploadPanel.qml');
 const editor=read('ui/components/composer/RichTextEditor.qml');
 const preview=read('ui/components/media/FarcasterPreview.qml');
 const tile=read('ui/components/composer/StickerDraftTile.qml');
 assert.match(drawer,/readonly property var draftStickers/);
 assert.match(drawer,/objectName: "mobileInlineStickerDrafts"/);
 assert.match(drawer,/StickerDraftTile \{/);
 assert.match(media,/regularAttachedMedia: root\.attachedMedia\.filter\(item => !item\.sticker\)/);
 assert.match(media,/hasAttachedMedia: regularAttachedMedia\.length > 0/);
 assert.match(editor,/readonly property bool stickerBlock/);
 assert.match(editor,/fillColor: stickerBlock \? "transparent"/);
 assert.match(editor,/objectName: "visualEditorStickerPreview"/);
 assert.match(preview,/sticker: sticker/);
 assert.match(preview,/StickerDraftTile \{/);
 assert.match(tile,/Image\.PreserveAspectFit/);
 assert.match(tile,/onClicked: root\.removeRequested\(\)/);
 assert.doesNotMatch(tile,/AmbientMediaBackdrop/);
 assert.doesNotMatch(tile,/MediaPreview \{/);
});

test('feed recognizes only pinned Njal and historical image links as compact uncropped stickers',()=>{
 const cast=read('ui/components/feed/CastItem.qml');
 const layout=read('ui/components/media/GalleryLayout.qml');
 const tile=read('ui/components/media/GalleryTile.qml');
 assert.match(cast,/function isVisualImage\(embed\)/);
 assert.match(cast,/StickerCatalog\.isStickerUrl\(root\.normalizedEmbedUrl\(embed\)\)/);
 assert.match(layout,/singleSticker/);
 assert.match(layout,/StickerCatalog\.isStickerUrl\(root\.mediaKey\(0\)\)/);
 assert.match(layout,/stickerWidth = Math\.min\(availableWidth/);
 assert.match(tile,/readonly property bool officialSticker: StickerCatalog\.isStickerUrl/);
 assert.match(tile,/Image\.PreserveAspectFit/);
 assert.match(tile,/root\.officialSticker \? "transparent"/);
 assert.match(tile,/active: root\.ambientEnabled && !root\.officialSticker/);
});

test('QML map and icon use reviewed assets; translations remain available',()=>{
 const cmake=read('ui/CMakeLists.txt');
 const theme=read('ui/theme/Theme.qml');
 const map=JSON.parse(read('ui/components/source_paths.json'));
 assert.match(cmake,/components\/composer\/StickerPicker\.qml/);
 assert.match(cmake,/components\/composer\/StickerDraftTile\.qml/);
 assert.match(cmake,/components\/composer\/StickerCatalog\.js/);
 assert.doesNotMatch(cmake,/assets\/stickers\/v1\//);
 assert.equal(map['StickerPicker.qml'],'composer/StickerPicker.qml');
 assert.equal(map['StickerDraftTile.qml'],'composer/StickerDraftTile.qml');
 assert.match(theme,/iconSticker: "media\/sticker"/);
 const svg=read('ui/assets/icons/genyleap/icons/media/sticker.svg');
 assert.match(svg,/Lucide/);
 assert.match(svg,/stroke-width="2"/);
 assert.match(svg,/M15 3v5/);
 const ids=['title','subtitle','search','all','reactions','web3',
            'favorites','recent','add','add_favorite','remove_favorite','empty',
            'media_slot','limit'];
 for(const locale of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
                      'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']){
  const ts=read('ui/i18n/qml_'+locale+'.ts');
  for(const id of ids)
   assert.match(ts,new RegExp('<message id="genycaster\\.stickers\\.'+id+'">'),locale+': '+id);
 }
 assert.ok(fs.existsSync(path.join(root,'scripts/import_njal_stickers.py')));
});
