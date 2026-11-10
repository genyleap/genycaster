import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=(name)=>readFileSync(new URL('../../'+name,import.meta.url),'utf8');

test('WASM profile links use the vetted first-party favicon proxy with ordered fallbacks',()=>{
 const row=read('ui/components/profile/ProfileLinkRow.qml');
 const proxy=read('services/media-gateway/browser_image_proxy.mjs');
 assert.match(row,/readonly property string browserFaviconSource:/);
 assert.match(row,/storage-farcaster\.genyleap\.com\/v1\/browser\/images\?url=/);
 assert.match(row,/\["\/favicon\.ico", "\/favicon\.png",/);
 assert.match(row,/apple-touch-icon\.png/);
 assert.match(row,/if \(!path\) return root\.previewArtworkSource/);
 assert.match(row,/source: !root\.mediaActive \? ""\s*: Qt\.platform\.os === "wasm"\s*\? root\.browserFaviconSource/);
 const profile=read("ui/components/profile/ProfileSurface.qml");
 assert.match(profile,/mediaActive: profileLinksDrawer\.opened/);
 assert.match(row,/browserFaviconVariant\+\+/);
 assert.match(proxy,/export function validPublicFaviconUrl/);
 assert.match(proxy,/rasterMimeFromBytes/);
});

test('Mini App details size to content with a fixed aligned header and short Open action',()=>{
 const app=read('ui/components/apps/AppsSurface.qml');
 assert.match(app,/id: miniAppDetailDrawer/);
 assert.match(app,/Math\.min\(parent\.height \* 0\.92, Math\.max\(360,/);
 assert.match(app,/detailsColumn\.implicitHeight \+ Theme\.scaleUi\(150\)/);
 assert.match(app,/objectName: "miniAppDetailModalHeader"/);
 assert.match(app,/title: qsTrId\("genycaster\.apps\.details"\)/);
 assert.match(app,/text: qsTrId\("genycaster\.apps\.open"\)/);
 assert.match(app,/Accessible\.description: qsTrId\("genycaster\.miniapp\.open_internal"\)/);
});

test('compact cast detail places channel identity on toolbar and author actions in one row',()=>{
 const detail=read('ui/components/feed/DetailSurface.qml');
 assert.match(detail,/objectName: "mobileConversationChannelHeader"/);
 assert.match(detail,/visible: root\.compact && root\.activeView === "cast"/);
 assert.match(detail,/sourceUrl: root\.resolvedChannelImage/);
 assert.match(detail,/onClicked: root\.channelRequested\(root\.castChannel\)/);
 assert.match(detail,/objectName: "detailDeleteCastAction"/);
 assert.match(detail,/onClicked: root\.removeCastRequested\(root\.detail\)/);
 assert.match(detail,/root\.replyRequested\(root\.detail\)/);
 assert.doesNotMatch(detail,/On phones, channel identity gets a dedicated line/);
 assert.doesNotMatch(detail,/Destructive actions are deliberately separated/);
});

test('mobile Widgets is a full-height, non-draggable inspector with an aligned close action',()=>{
 const shell=read('ui/components/shell/AppShell.qml');
 const widget= shell.slice(shell.indexOf('id: contextDrawer'),shell.indexOf('FloatingComposeButton {',shell.indexOf('id: contextDrawer')));
 assert.match(widget,/objectName: "responsiveWidgetsDrawer"/);
 assert.match(widget,/fullScreen: root\.mobile/);
 assert.match(widget,/showHandle: false/);
 assert.match(widget,/interactive: !root\.mobile && root\.compactDesktop/);
 assert.match(widget,/height: parent \? parent\.height : 720/);
 assert.match(widget,/Layout\.alignment: Qt\.AlignVCenter/);
 assert.match(widget,/ContextRail \{/);
 assert.doesNotMatch(widget,/parent\.height \* 0\.88/);
});

test('drawer headers use the same centered title and close-button layout',()=>{
 const shared=read('ui/components/controls/ModalHeader.qml');
 const pro=read('ui/components/profile/GenyProDetailsDrawer.qml');
 const apps=read('ui/components/apps/AppsSurface.qml');
 assert.match(shared,/Layout\.alignment: Qt\.AlignVCenter/);
 assert.match(shared,/Layout\.minimumHeight: root\.sheetPresentation[\s\S]*?Theme\.scaleUi\(32\)/);
 assert.match(pro,/height: Theme\.scaleUi\(56\)/);
 assert.match(pro,/ActionButton \{\s*visible: !root\.compact\s*Layout\.alignment: Qt\.AlignVCenter/);
 assert.match(apps,/ModalHeader \{/);
});

test('Cast URL and Mini App embeds share gutters, rounded artwork and stable failure placeholders',()=>{
 const card=read('ui/components/media/LinkPreview.qml');
 assert.match(card,/cardInset: root\.embeddedInCard \? 0 : root\.layoutMiniApp \? Theme\.space4/);
 assert.match(card,/anchors\.margins: root\.cardInset/);
 assert.match(card,/spacing: Theme\.space2/);
 assert.equal([...card.matchAll(/&& root\.reservedLayout\.artwork \? readyHeight : 0/g)].length, 2);
 assert.match(card,/clipToSuperellipse: true/);
 assert.match(card,/if \(loadFailed && root\.previewSource\.length > 0\)/);
 assert.match(card,/text: root\.miniAppLaunching[\s\S]*?: qsTrId\("genycaster\.apps\.open"\)/);
 assert.match(card,/Layout\.minimumWidth: 0/);
});

test('Save draft is disabled and guarded when no meaningful text/media/embed/thread content exists',()=>{
 const composer=read('ui/components/composer/ComposerDrawer.qml');
 assert.match(composer,/function hasDraftContent\(\)/);
 assert.match(composer,/root\.hasMeaningfulText\(mobilePlainEditor\.text\)/);
 assert.match(composer,/root\.hasMeaningfulText\(markdownEditor\.text\)/);
 assert.match(composer,/root\.attachedMediaCount > 0 \|\| root\.pendingMediaFiles\.length > 0/);
 assert.match(composer,/return String\(embed && embed\.kind \|\| ""\) !== "cast-reference"/);
 assert.match(composer,/enabled: !presentationBridge\.writeBusy && root\.hasDraftContent\(\)/);
 assert.match(composer,/if \(!root\.hasDraftContent\(\)\) return/);
 assert.match(composer,/&& root\.hasDraftContent\(\)\)\s*richComposer\.saveDraft\(\)/);
});
