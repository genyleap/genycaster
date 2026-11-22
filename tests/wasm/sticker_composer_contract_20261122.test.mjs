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
const {stickers,decodeIds,toggleFavorite,recordRecent,filtered,embedUrl,previewUrl,itemForId}=sandbox;

test('original catalog is fixed, complete, platform-agnostic and first-party',()=>{
 assert.equal(stickers.length,12);
 assert.equal(new Set(stickers.map(x=>x.id)).size,12);
 assert.deepEqual([...new Set(stickers.map(x=>x.pack))].sort(),['originals','reactions','web3']);
 for(const sticker of stickers){
  assert.match(sticker.id,/^[a-z0-9_-]+$/);
  assert.ok(sticker.title.length>1);
  assert.equal(embedUrl(sticker.id),'https://genycaster.xyz/stickers/v1.0.0/'+sticker.id+'.png');
  assert.equal(previewUrl(sticker.id),'qrc:/qt/qml/Farcaster/Client/assets/stickers/v1/'+sticker.id+'.png');
  const directory='ui/assets/stickers/v1/';
  assert.ok(fs.existsSync(path.join(root,directory,sticker.id+'.svg')));
  const buf=fs.readFileSync(path.join(root,directory,sticker.id+'.png'));
  assert.equal(buf.subarray(0,8).toString('hex'),'89504e470d0a1a0a');
  assert.equal(buf.readUInt32BE(16),512);
  assert.equal(buf.readUInt32BE(20),512);
  assert.ok([2,6].includes(buf[25]),'supported PNG color type');
  assert.ok(buf.length>1000&&buf.length<350000);
 }
 assert.equal(itemForId('unknown'),null);
 assert.equal(embedUrl('../bad'), '');
 assert.equal(previewUrl('https://evil.test/'), '');
});

test('filtering, favorites and recents are bounded to trusted catalog IDs',()=>{
 const ids=Array.from(stickers,x=>x.id);
 assert.equal(filtered('all','',[],[]).length,12);
 assert.equal(filtered('originals','',[],[]).length,4);
 assert.deepEqual(Array.from(filtered('all','rocket',[],[]),x=>x.id),['rocket']);
 assert.deepEqual(Array.from(filtered('reactions','heart',[],[]),x=>x.id),['love']);
 assert.deepEqual(Array.from(filtered('favorites','', ['gm','bloom'],[]),x=>x.id),['gm','bloom']);
 assert.deepEqual(Array.from(filtered('recent','',[],['rocket','gm']),x=>x.id),['rocket','gm']);
 assert.deepEqual(Array.from(decodeIds(JSON.stringify(['gm','../bad','gm','rocket']),12)),['gm','rocket']);
 assert.deepEqual(Array.from(decodeIds('not json',12)),[]);
 assert.deepEqual(Array.from(decodeIds('"nope"',12)),[]);
 assert.deepEqual(Array.from(toggleFavorite(['gm'],'gm')),[]);
 assert.deepEqual(Array.from(toggleFavorite(['gm'],'bloom')),['bloom','gm']);
 assert.deepEqual(Array.from(recordRecent(['gm','bloom'],'rocket')),['rocket','gm','bloom']);
 assert.deepEqual(Array.from(recordRecent(ids,'bloom')).slice(0,2),['bloom','orbit']);
 assert.equal(recordRecent(ids,'rocket').length,12);
 assert.deepEqual(Array.from(toggleFavorite(['gm'],'../bad')),['gm']);
});

test('sticker publication uses regular protocol media embed without extra uploads',()=>{
 const drawer=read('ui/components/composer/ComposerDrawer.qml');
 const toolbar=read('ui/components/composer/FormattingToolbar.qml');
 const picker=read('ui/components/composer/StickerPicker.qml');
 const controller=read('src/presentation/qt/rich_composer_controller.cpp');
 assert.match(drawer,/function insertSticker\(sticker\)/);
 assert.match(drawer,/StickerCatalog\.embedUrl\(id\)/);
 assert.match(drawer,/root\.canUploadMedia \|\| mediaUpload\.busy/);
 assert.match(drawer,/richComposer\.insertMedia\(canonical, "image\/png"/);
 assert.doesNotMatch(drawer,/insertManagedMedia\(canonical/);
 assert.match(controller,/bool RichComposerController::insertMedia\(/);
 assert.match(controller,/BlockKind::Media/);
 assert.match(controller,/QVariantList RichComposerController::protocolEmbeds\(/);
 assert.match(drawer,/stickerPicker\.recordUsage\(sticker\.id\)/);
 assert.match(drawer,/onStickerSelected: sticker/);
 assert.match(drawer,/objectName: "mobileComposerStickerAction"/);
 assert.match(toolbar,/objectName: "desktopComposerStickerAction"/);
 assert.match(drawer,/onStickerRequested: root\.openStickerPicker/);
 assert.match(picker,/Settings \{/);
 assert.match(picker,/genycaster-stickers-v1/);
 assert.match(picker,/readonly property var visibleStickers/);
 assert.match(picker,/stickerHit\.pressed/);
 assert.match(picker,/onClicked: root\.toggleFavorite/);
 assert.match(picker,/StickerCatalog\.previewUrl\(tile\.modelData\.id\)/);
 assert.match(picker,/root\.canAdd/);
 assert.match(drawer,/!stickerPicker\.opened/);
});

test('cast feed recognizes first-party image links and shows compact uncropped stickers',()=>{
 const cast=read('ui/components/feed/CastItem.qml');
 const layout=read('ui/components/media/GalleryLayout.qml');
 const tile=read('ui/components/media/GalleryTile.qml');
 assert.match(cast,/function isVisualImage\(embed\)/);
 assert.match(cast,/genycaster\\?\.xyz/);
 assert.match(layout,/singleSticker/);
 assert.match(layout,/stickerWidth = Math\.min\(availableWidth/);
 assert.match(tile,/readonly property bool officialSticker/);
 assert.match(tile,/Image\.PreserveAspectFit/);
 assert.match(tile,/root\.officialSticker \? "transparent"/);
 assert.match(tile,/active: root\.ambientEnabled && !root\.officialSticker/);
});

test('QML is registered, resources are bundled, and all release locales contain sticker UI',()=>{
 const cmake=read('ui/CMakeLists.txt');
 const theme=read('ui/theme/Theme.qml');
 assert.match(cmake,/components\/composer\/StickerPicker\.qml/);
 assert.match(cmake,/components\/composer\/StickerCatalog\.js/);
 assert.match(cmake,/assets\/stickers\/v1\/verified\.png/);
 assert.match(theme,/iconSticker: "media\/sticker"/);
 assert.ok(fs.existsSync(path.join(root,'ui/assets/icons/genyleap/icons/media/sticker.svg')));
 const ids=[
   'title','subtitle','search','all','originals','reactions','web3',
   'favorites','recent','add','add_favorite','remove_favorite','empty','media_slot','limit'
 ];
 for(const locale of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
                       'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']){
   const ts=read('ui/i18n/qml_'+locale+'.ts');
   for(const id of ids){
     assert.match(ts,new RegExp('<message id="genycaster\\.stickers\\.'+id+'">'),locale+': '+id);
   }
 }
});
