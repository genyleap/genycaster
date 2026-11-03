import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=name=>readFileSync(new URL('../../'+name,import.meta.url),'utf8');

test('Explorer audience filter persists and applies to each semantic tab',()=>{
 const shell=read('ui/components/AppShell.qml');
 const tabs=read('ui/components/ContentTabs.qml');
 const header=read('ui/components/HomeFeedHeader.qml');
 assert.doesNotMatch(tabs,/id: "following"/);
 assert.match(shell,/property string explorerAudience: "all"/);
 assert.match(shell,/category: "explorer-feed"/);
 assert.match(shell,/function chooseExplorerAudience\(audience\)/);
 assert.match(shell,/const source = root\.explorerAudience === "following"/);
 assert.match(shell,/\? "following" : globalSource/);
 assert.match(shell,/presentationBridge\.selectFeed\(source\)/);
 assert.match(header,/objectName: "explorerAudienceSelector"/);
 assert.match(header,/onClicked: root\.audienceRequested\(String\(modelData\.id\)\)/);
 assert.match(header,/readonly property bool selected: root\.audience === String\(modelData\.id\)/);
 assert.match(header,/Behavior on color/);
 assert.doesNotMatch(header,/audienceMenu\.open\(\)/);
 assert.match(read('ui/mobile/runtime_ui_bindings.cpp'),/feed == QStringLiteral\("following"\)\) query\.kind = Kind::Following/);
});
test('Stories use only verified following graph and never feed author suggestions',()=>{
 const shell=read('ui/components/AppShell.qml');
 const bridge=read('src/presentation/qt/presentation_bridge.cpp');
 const strip=read('ui/components/StoryStrip.qml');
 assert.match(shell,/recentProfiles: presentationBridge\.followingStoryProfiles/);
 assert.match(bridge,/m_accountFid != m_followingStoriesViewerFid/);
 assert.match(bridge,/m_followingStoryProfiles/);
 assert.match(strip,/followingOnly: true/);
 assert.match(read('ui/components/HomeFeedHeader.qml'),/root\.authenticated \? root\.recentProfiles : \[\]/);
});
test('Portrait policy is present and Web Safari gets a reversible landscape guard',()=>{
 const manifest=JSON.parse(read('ui/wasm/genycaster.webmanifest'));
 assert.equal(manifest.orientation,'portrait');
 const js=read('ui/wasm/portrait-orientation-pre.js');
 assert.match(js,/orientation\.lock|orientation\.lock ===/); // inspected best-effort support
 assert.match(js,/orientationchange/);
 assert.match(js,/rotate|عمودی/);
 assert.match(read('ui/CMakeLists.txt'),/wasm\/portrait-orientation-pre\.js/);
});
test('Feed and detail reveal source channel as metadata without a hardcoded provider domain',()=>{
 for(const name of ['CastItem.qml','DetailSurface.qml']){
  const code=read('ui/components/'+name);
  assert.match(code,/readonly property string castChannel:/);
  assert.match(code,/channel\\\/|channel\//);
  assert.doesNotMatch(code,/warpcast\.com\/~/);
 }
 assert.match(read('ui/mobile/runtime_ui_bindings.cpp'),/parentUrl.*QString::fromStdString/);
});
test('Cast surfaces blend into theme, including the detail header',()=>{
 const cast=read('ui/components/CastItem.qml');
 const detail=read('ui/components/DetailSurface.qml');
 assert.match(cast,/fillColor: Theme\.backgroundPrimary/);
 assert.match(cast,/id: messageSurface[\s\S]*?borderWidth: 0/);
 assert.match(detail,/root\.activeView === "cast" \? Theme\.backgroundPrimary/);
});
test('Quote produces a visible error when a canonical target is missing and opens a composer for valid hashes',()=>{
 const shell=read('ui/components/AppShell.qml');
 const bar=read('ui/components/EngagementBar.qml');
 assert.match(bar,/onClicked: root\.quoteRequested\(\)/);
 assert.match(shell,/function openQuoteComposer\(cast\)/);
 assert.match(shell,/replyTargetErrorLoader\.active = true/);
 assert.match(shell,/root\.openComposer\(null, "quote", "", Object\.assign/);
 assert.match(read('ui/components/ComposerDrawer.qml'),/richComposer\.insertCastReference/);
});
test('Hydrated mention display preserves protocol text and crosses feed, detail and notifications',()=>{
 const cast=read('src/domain/cast.cppm');
 const codec=read('src/providers/hypersnap/codec.cpp');
 const ui=read('ui/mobile/runtime_ui_bindings.cpp');
 assert.match(cast,/optional<std::string> displayText/);
 assert.match(codec,/resolvedMentionText\(/);
 assert.match(codec,/mentioned_profiles/);
 assert.match(codec,/mentions_positions/);
 assert.match(codec,/isBoundary\(pos\)/);
 assert.match(codec,/cast\.displayText = resolvedMentionText/);
 assert.match(ui,/QString::fromStdString\(cast\.displayText\.value_or\(cast\.text\)\)/);
 assert.match(ui,/castText"\), readableCastText\(\*item\.cast\)/);
});

test('Quoted cast hashes accept both prefixless provider hashes and 0x hashes',()=>{
 const shell=read('ui/components/AppShell.qml');
 assert.match(shell,/source\.replace\(\/\^0x\/i, ""\)/);
 assert.match(shell,/\/\^\[0-9a-f\]\{40\}\$\/i\.test\(bare\)/);
 assert.match(shell,/castId: hash, authorFid: authorFid/);
 assert.match(read('src/presentation/qt/rich_composer_controller.cpp'),/normalized\.size\(\) != 42/);
});
test('Feed and details display actual channel metadata including an image from provider',()=>{
 const codec=read('src/providers/hypersnap/codec.cpp');
 const domain=read('src/domain/cast.cppm');
 const presentation=read('ui/mobile/runtime_ui_bindings.cpp');
 for(const word of ['channelId','channelName','channelImageUrl']){
   assert.match(domain,new RegExp(word));
   assert.match(presentation,new RegExp(word));
 }
 assert.match(codec,/findValue\(castObject, "channel"\)/);
 assert.match(codec,/optionalString\(channel,"image_url"\)/);
 assert.match(presentation,/attachChannelPresentation\(row, \*cast\)/);
 assert.match(presentation,/attachChannelPresentation\(detail, \*state\.cast\)/);
 assert.match(read('ui/components/CastItem.qml'),/sourceUrl: root\.channelImageUrl/);
 assert.match(read('ui/components/DetailSurface.qml'),/sourceUrl: root\.resolvedChannelImage/);
});

test('Following selection uses authenticated account FID for initial and next pages',()=>{
 const bridge=read('src/presentation/qt/presentation_bridge.cpp');
 const app=read('ui/components/AppShell.qml');
 assert.match(bridge,/feed == QStringLiteral\("following"\) && \(!m_isAuthenticated \|\| m_accountFid == 0\)/);
 assert.match(bridge,/nextFeed == QStringLiteral\("following"\) \? m_accountFid : m_identityFid/);
 assert.match(bridge,/m_activeFeed == QStringLiteral\("following"\) \? m_accountFid : m_identityFid/);
 assert.match(app,/Number\(presentationBridge\.accountFid \|\| 0\) > 0/);
 assert.doesNotMatch(app.slice(app.indexOf('function chooseExplorerAudience('),
    app.indexOf('property string explorerPresentationMode:')),/root\.identityAvailable/);
});

test('Following audience action updates state and requests new feed instead of silently restoring Everyone',()=>{
 const src=read('ui/components/AppShell.qml');
 const method=src.slice(src.indexOf('function chooseExplorerAudience('),
                        src.indexOf('property string explorerPresentationMode:')).trim();
 const bridge={isAuthenticated:true,accountFid:1234};
 const calls=[];
 const root={
   explorerAudience:'all',explorerPresentationMode:'visuals',
   selectExplorerPresentationMode(mode,forceRefresh){calls.push({mode,forceRefresh});}
 };
 const choose=new Function('root','presentationBridge',method+
     '; return chooseExplorerAudience;')(root,bridge);
 choose('following');
 assert.equal(root.explorerAudience,'following');
 assert.deepEqual(calls,[{mode:'visuals',forceRefresh:true}]);
 choose('all');
 assert.equal(root.explorerAudience,'all');
 assert.equal(calls.length,2);
});

test('New Cast cannot inherit a quoted reference from a previously composed cast',()=>{
 const cpp=read('src/presentation/qt/rich_composer_controller.cpp');
 const header=read('src/presentation/qt/rich_composer_controller.hpp');
 const composer=read('ui/components/ComposerDrawer.qml');
 assert.match(header,/Q_INVOKABLE void beginContextDocument\(\)/);
 assert.match(cpp,/void RichComposerController::beginContextDocument\(\)/);
 assert.match(cpp,/if \(contextDocument\) return/);
 assert.match(cpp,/if \(!contextDocument\) autosave\.start\(\)/);
 assert.match(cpp,/std::holds_alternative<farcaster::domain::rich::CastReferenceBlock>/);
 assert.match(cpp,/if \(!m_impl->contextDocument\) \{[\s\S]*?settings\.remove\(m_impl->draftKey\(\)\)/);
 assert.match(composer,/richComposer\.beginContextDocument\(\)/);
 assert.match(composer,/function publicationEmbeds\(\)/);
 assert.match(composer,/return root\.quoteMode \? embeds : embeds\.filter/);
 assert.match(composer,/String\(embed && embed\.kind \|\| ""\) !== "cast-reference"/);
 assert.match(composer,/ProCastPolicy\.publicationPlan\(richComposer\.safeText[\s\S]*?root\.publicationEmbeds\(\)/);
 assert.match(composer,/root\.publishRequested\(richComposer\.safeText[\s\S]*?planned\.parts\[0\]\.embeds/);
 assert.match(composer,/!root\.parentCast && !root\.quotedCast\s+&& richComposer\.dirty/);
 assert.match(composer,/visible: !root\.replyMode && !root\.quoteMode && !root\.transientDraft/);
});


test('Provider channel imagery is resolved from the configured directory, not synthetic glyphs',()=>{
 const controller=read('ui/mobile/channel_picker_controller.cpp');
 const header=read('ui/mobile/channel_picker_controller.hpp');
 const item=read('ui/components/CastItem.qml');
 const detail=read('ui/components/DetailSurface.qml');
 assert.match(header,/Q_INVOKABLE void ensureChannelImage\(const QString& id\)/);
 assert.match(controller,/requestDirectory\(QStringLiteral\("channel"\), \{\{QStringLiteral\("key"\), id\}\}/);
 assert.match(controller,/m_castImagePending\.size\(\) >= 4/);
 assert.match(controller,/imageRevisionChanged\(\)/);
 assert.match(item,/channelDirectory\.imageUrlForChannel\(root\.castChannel\)/);
 assert.match(item,/channelDirectory\.ensureChannelImage\(root\.castChannel\)/);
 assert.match(detail,/sourceUrl: root\.resolvedChannelImage/);
});

test('Hydrated mention offsets after multibyte emoji preserve raw Farcaster text',()=>{
 const codec=read('src/providers/hypersnap/codec.cpp');
 const tests=read('tests/providers/hypersnap/contract_tests.cpp');
 assert.match(codec,/const auto characterToByte/);
 assert.match(codec,/if \(canonical\) return std::nullopt/);
 assert.match(codec,/const auto decoded = characterToByte\(pos, false\)/);
 assert.match(tests,/mentioned_profiles_ranges.*start.*9.*end.*9/);
 assert.match(tests,/Right! 💯 @vrypan\.eth/);
});

test('Image composer uploads appear below text, large, uncropped and with bounded uploads',()=>{
 const editor=read('ui/components/ComposerDrawer.qml');
 const media=read('ui/components/MediaUploadPanel.qml');
 const preview=read('ui/components/MediaPreview.qml');
 assert.ok(editor.indexOf('id: mediaPanel') > editor.indexOf('id: mobilePlainEditor'));
 assert.match(media,/fitImage: true/);
 assert.match(media,/Layout\.preferredHeight: root\.width < 620 \? 238 : 174/);
 assert.match(media,/visible: !root\.hasAttachedMedia && !root\.hasTransientUpload/);
 assert.match(preview,/root\.fitImage \? Image\.PreserveAspectFit/);
 const gateway=read('services/media-gateway/gateway.mjs');
 assert.match(gateway,/const uploadLimiter = dependencies\.uploadLimiter/);
 assert.match(gateway,/partRoute \? partLimiter : uploadLimiter/);
});
