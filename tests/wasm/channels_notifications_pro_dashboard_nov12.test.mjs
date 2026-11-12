import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('Following is an explicit provider read, not a disguised membership or signer write',()=>{
 const controller=read('ui/app/social/channel_administration.cpp');
 const bootstrap=read('ui/app/main.cpp');
 const channels=read('ui/components/social/ChannelsSurface.qml');
 const manager=read('ui/components/social/ChannelManagementPanel.qml');
 assert.match(controller,/api\.neynar\.com\/v2\/farcaster\/user\/channels\//);
 assert.match(controller,/parameters\.addQueryItem\(QStringLiteral\("fid"\)/);
 assert.match(controller,/request\.setRawHeader\("x-api-key"/);
 assert.match(controller,/m_followingGeneration/);
 assert.match(controller,/if \(followingGeneration != m_followingGeneration\)/);
 assert.match(bootstrap,/const QString channelReadApiKey/);
 assert.match(bootstrap,/configuredMiniAppDiscoveryApiKey/);
 assert.doesNotMatch(controller,/NEYNAR_API_DOCS/);
 assert.match(channels,/root\.scope === "moderate"[\s\S]*?channel\.roleKnown/);
 assert.match(channels,/Boolean\(channel\.isModerator\)/);
 assert.match(manager,/root\.manager\.activeReadContract === "neynar-v2"/);
});

test('reaction overflow badge gives hidden count and opens a real actor list',()=>{
 const notifications=read('ui/components/feed/NotificationSurface.qml');
 assert.match(notifications,/additionalCount: Math\.max\(0, groupCount - actorStack\.count\)/);
 assert.match(notifications,/text: "\+" \+ LocaleFormat\.formatInteger\(actorCountBadge\.additionalCount\)/);
 assert.match(notifications,/id: reactionActorsPopup/);
 assert.match(notifications,/id: reactionActorsList/);
 assert.match(notifications,/root\.inspectActors\(notificationRow\.modelData\)/);
 assert.match(notifications,/root\.profileRequested\(fid\)/);
 assert.match(notifications,/visible: root\.expandedActorCount > root\.expandedActors\.length/);
 assert.match(notifications,/model: root\.expandedActors/);
});

test('Home shortcuts and premium sidebar actions are working and scoped',()=>{
 const home=read('ui/components/home/HomeDashboard.qml');
 const side=read('ui/components/navigation/SideNavigation.qml');
 const card=read('ui/components/profile/ProfileTrustCard.qml');
 assert.match(home,/destination: "channels"/);
 assert.match(home,/destination: "pro"/);
 assert.match(home,/onClicked: root\.channelsRequested\(\)/);
 assert.match(home,/else if \(destination === "pro"\) root\.proRequested\(\)/);
 assert.doesNotMatch(home,/destination: "wallet"/);
 assert.match(side,/function requestSidebarPro\(\)/);
 assert.match(side,/proRecoveryAttemptedFid === Number\(root\.accountFid\)/);
 assert.match(side,/openProofConnections\.retryProEntitlements\(\)/);
 assert.match(side,/proChecking: openProofConnections/);
 assert.match(card,/objectName: "sidebarProMembershipCard"/);
 assert.match(card,/fillColor: Theme\.proSurface/);
 assert.match(card,/text: "FID " \+ LocaleFormat\.formatInteger\(root\.fid\)/);
 assert.match(card,/Accessible\.onPressAction: root\.proRequested\(\)/);
});
