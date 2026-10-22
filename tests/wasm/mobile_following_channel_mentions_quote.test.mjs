import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=name=>readFileSync(new URL('../../'+name,import.meta.url),'utf8');

test('Explorer Following selects the authenticated provider Following source, not Trending',()=>{
 const shell=read('ui/components/AppShell.qml');
 const tabs=read('ui/components/ContentTabs.qml');
 assert.match(tabs, /id: "following", label: qsTr\("Following"\)/);
 assert.match(shell, /nextMode === "following" \? \(presentationBridge\.isAuthenticated/);
 assert.match(shell, /\? "following" : "recent"/);
 assert.match(shell, /presentationBridge\.selectFeed\(source\)/);
 assert.match(read('ui/mobile/runtime_ui_bindings.cpp'), /feed == QStringLiteral\("following"\)\) query\.kind = Kind::Following/);
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
