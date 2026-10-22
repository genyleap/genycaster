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
 assert.match(header,/onTriggered: root\.audienceRequested\("following"\)/);
 assert.match(header,/onTriggered: root\.audienceRequested\("all"\)/);
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
 assert.match(cast,/borderWidth: root\.compact \? 0/);
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
 assert.match(codec,/original\[pos\]/);
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
 assert.match(read('ui/components/DetailSurface.qml'),/sourceUrl: String\(root\.detail\.channelImageUrl \|\| ""\)/);
});
