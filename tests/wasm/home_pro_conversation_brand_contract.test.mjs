import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const file=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('official GENY token assets are bundled and consistently selected on home and market',()=>{
 const light=file('ui/assets/icons/genyleap/icons/brands/geny-token-mark-light.svg');
 const dark=file('ui/assets/icons/genyleap/icons/brands/geny-token-mark-dark.svg');
 for(const svg of [light,dark]){
  assert.match(svg,/viewBox="0 0 228\.73 465\.02"/);
  assert.match(svg,/#00423B/);
  assert.match(svg,/#00A693/);
  assert.doesNotMatch(svg,/<script/);
 }
 const home=file('ui/components/home/HomeDashboard.qml');
 const rail=file('ui/components/navigation/ContextRail.qml');
 const asset=file('ui/components/content/MarketAssetIcon.qml');
 assert.match(home,/geny-token-mark-dark\.svg/);
 assert.match(rail,/geny-token-mark-dark\.svg/);
 assert.match(asset,/canonicalGenyAsset: root\.symbol === "GENY"/);
  assert.match(asset,/readonly property bool verifiedRemoteArtwork: root\.canonicalGenyAsset/);
  assert.match(asset,/bundledGenyArtwork: root\.canonicalGenyAsset/);
 assert.match(asset,/geny-token-mark-dark\.svg/);
 assert.match(asset,/root\.symbol === "ETH" \? 0\.78 : 0\.64/);
 assert.match(asset,/bundledArtwork: root\.semanticIcon\.length > 0/);
});

test('Geny Pro replaces the sidebar Streak CTA with a themed, interactive drawer',()=>{
 const trust=file('ui/components/profile/ProfileTrustCard.qml');
 const nav=file('ui/components/navigation/SideNavigation.qml');
 const shell=file('ui/components/shell/AppShell.qml');
 const drawer=file('ui/components/profile/GenyProDetailsDrawer.qml');
 assert.match(trust,/signal proRequested\(\)/);
 assert.match(trust,/text: root\.compact && root\.proEligible\s*\? qsTrId\("genycaster\.pro\.short"\)/);
 assert.match(trust,/: qsTrId\("genycaster\.pro\.title"\)/);
 assert.match(trust,/onClicked: root\.proRequested\(\)/);
 assert.match(nav,/onProRequested: root\.proRequested\(\)/);
 assert.match(shell,/onProRequested: genyProDetailsDrawer\.open\(\)/);
 assert.match(drawer,/objectName: "genyProDetailsDrawer"/);
 assert.match(drawer,/id: fixedHeader/);
 assert.match(drawer,/id: detailsViewport/);
 assert.match(drawer,/id: fixedActions/);
 assert.match(drawer,/membership\.status === "verified"/);
 assert.match(drawer,/membership\.eligible === true/);
 assert.match(drawer,/fetchProPlan\(true\)/);
 assert.match(drawer,/retryProEntitlements\(\)/);
 assert.match(drawer,/root\.canRequestQuote/);
});

test('Cast conversation header shows canonical avatar and verified membership without repeating channel by author',()=>{
 const detail=file('ui/components/feed/DetailSurface.qml');
 assert.match(detail,/readonly property bool channelMembershipKnown:/);
 assert.match(detail,/channelDirectory\.membershipReady/);
 assert.match(detail,/channelDirectory\.openForViewer\(Math\.floor\(root\.accountFid\)\)/);
 assert.match(detail,/sourceUrl: root\.resolvedChannelImage/);
 assert.match(detail,/genycaster\.composer\.joined_channel/);
 assert.match(detail,/genycaster\.composer\.not_joined_channel/);
 assert.match(detail,/onClicked: root\.channelRequested\(root\.castChannel\)/);
 assert.match(detail,/visible: false \/\/ Channel identity is shown once/);
});
