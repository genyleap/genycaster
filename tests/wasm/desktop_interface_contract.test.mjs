import {readFileSync} from 'node:fs';
import assert from 'node:assert/strict';
import test from 'node:test';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('share menus show concise, untruncated action labels', () => {
    const cast = read('ui/components/EngagementBar.qml');
    const profile = read('ui/components/ProfileSurface.qml');
    assert.match(cast, /text: qsTr\("Share…".*\)/);
    assert.match(cast, /text: qsTr\("Copy link"\)/);
    assert.match(cast, /text: qsTr\("Copy Farcaster URI"\)/);
    assert.match(profile, /text: qsTr\("Share profile"\)/);
    assert.match(profile, /text: qsTr\("Copy profile link"\)/);
    assert.match(profile, /text: qsTr\("Copy Farcaster URI"\)/);
    assert.doesNotMatch(cast + profile, /Copy Farcaster URI \(experimental\)|Copy Genycaster profile link/);
});

test('views are a reaction metric, not a centered flexible footer action', () => {
    const bar = read('ui/components/EngagementBar.qml');
    const reactions = bar.indexOf('id: reactionActions');
    const views = bar.indexOf('id: castViewsLabel');
    const reactionClose = bar.indexOf('// Flexible space keeps utility actions');
    const utilities = bar.indexOf('id: utilityActions');
    assert.ok(reactions >= 0 && reactions < views && views < reactionClose && reactionClose < utilities);
    assert.match(bar.slice(views, reactionClose), /Layout\.alignment: Qt\.AlignVCenter/);
});

test('desktop Home presents one bounded content column', () => {
    const home = read('ui/components/HomeDashboard.qml');
    assert.match(home, /Math\.min\(792, parent\.width - Theme\.space4 \* 2\)/);
    assert.match(home, /anchors\.horizontalCenter: parent\.horizontalCenter/);
    assert.match(home, /id: desktopComposerSurface/);
    assert.match(home, /GenyTokenBanner \{\}/);
});

test('desktop store uses borderless app rows and compact Open buttons', () => {
    const apps = read('ui/components/AppsSurface.qml');
    const feature = read('ui/components/MiniAppFeaturedCarousel.qml');
    assert.match(apps, /cellHeight: root\.compact \? 126 : 112/);
    assert.match(apps, /id: storeRow/);
    assert.match(apps, /objectName: "miniAppDesktopStoreRow"/);
    assert.match(apps, /objectName: "miniAppStoreOpenButton"/);
    assert.match(apps, /text: cardCell\.launchBusy \? qsTr\("Opening…\"\)/);
    assert.match(apps, /: cardCell\.running \? qsTr\("Resume"\) : qsTr\("Open"\)/);
    assert.match(apps, /cardCell\.modelData\.category/);
    assert.doesNotMatch(apps, /id: card\n/);
    assert.match(apps, /id: miniAppStoreTile/);
    assert.match(feature, /compact \? 272 : 260/);
});

test('desktop outside click closes drawers without changing mobile input handling', () => {
    const drawer = read('ui/components/GenyDrawer.qml');
    assert.match(drawer, /onBackgroundTapped: control\.close\(\)/);
    assert.match(drawer, /visible: !control\.bottomSheet/);
    assert.doesNotMatch(drawer, /visible: !control\.bottomSheet\s*&&\s*\(Qt\.platform\.os/);
    const settings = read('ui/components/LocaleSettingsPopup.qml');
    assert.match(settings, /viewportWidth: Overlay\.overlay/);
    assert.match(settings, /viewportWidth < Theme\.breakpointMobile/);
    assert.match(drawer, /onPressed: mouse => \{[\s\S]*?control\.close\(\)/);
    assert.match(drawer, /blockInput: true/);
});

test('profile lifecycle and cast identity avoid undefined access and layout feedback', () => {
    const cast = read('ui/components/CastItem.qml');
    const profile = read('ui/components/ProfileSurface.qml');
    assert.doesNotMatch(cast, /castNameAndChannel\.width \* 0\.62/);
    assert.doesNotMatch(cast, /Layout\.maximumWidth: Layout\.preferredWidth/);
    assert.match(profile, /const current = root\.detail \|\| \(\{\}\)/);
    assert.match(profile, /const retained = root\.retainedDetail \|\| \(\{\}\)/);
});


test('WASM offers verified web clients and never launches mobile app URIs', () => {
    const cast = read('ui/components/EngagementBar.qml');
    assert.match(cast, /text: Qt\.platform\.os === "wasm" \? qsTr\("Quorum Web"\)/);
    assert.match(cast, /presentationBridge\.openExternalUrl\("https:\/\/app\.quorummessenger\.com\/"\)/);
    assert.match(cast, /text: Qt\.platform\.os === "wasm" \? qsTr\("Copy Quorum Web"\)/);
    assert.match(cast, /presentationBridge\.farcasterWebCastUrl/);
    assert.match(cast, /visible: Qt\.platform\.os !== "wasm"\s*&& presentationBridge\.farcasterCastUri/);
    assert.match(cast, /presentationBridge\.castShareUrl\(root\.normalizedCastId\)/);
});

test('Apps search has constrained width and data-driven topic filter', () => {
    const apps = read('ui/components/AppsSurface.qml');
    assert.match(apps, /objectName: "miniAppTopicFilter"/);
    assert.match(apps, /Layout\.maximumWidth: 420/);
    assert.match(apps, /readonly property var availableCategories/);
    assert.match(apps, /source\.filter\(app => String\(app\.category \|\| ""\)/);
});

test('cast backgrounds and floating compose use page canvas and physical right edge', () => {
    const cast = read('ui/components/CastItem.qml');
    const reply = read('ui/components/ThreadReplyItem.qml');
    const shell = read('ui/components/AppShell.qml');
    assert.match(cast, /fillColor: Theme\.backgroundPrimary\s*borderWidth: 0/);
    assert.match(reply, /fillColor: Theme\.backgroundPrimary/);
    assert.match(shell, /root\.width - Theme\.space5 - width/);
    assert.doesNotMatch(shell, /root\.width - root\.railReservation - Theme\.space5 - width/);
});

test('release updates use a top-edge drawer with real versions and actions', () => {
    const notice = read('ui/components/UpdateNoticeDrawer.qml');
    const shell = read('ui/components/AppShell.qml');
    assert.match(notice, /edge: Qt\.TopEdge/);
    assert.match(notice, /updateController\.updateAvailable/);
    assert.match(notice, /updateController\.appVersion/);
    assert.match(notice, /updateController\.latestVersion/);
    assert.match(notice, /updateController\.downloadUpdate\(\)/);
    assert.match(notice, /updateController\.refreshWebApp\(\)/);
    assert.match(shell, /UpdateNoticeDrawer \{/);
});

test('client badge only uses provider-returned metadata in feed and replies', () => {
    const badge = read('ui/components/SourceClientBadge.qml');
    const feed = read('ui/components/CastItem.qml');
    const reply = read('ui/components/ThreadReplyItem.qml');
    assert.match(badge, /visible: sanitizedName\.length > 0/);
    assert.match(feed, /objectName: "castSourceClientBadge"/);
    assert.match(reply, /objectName: "replySourceClientBadge"/);
    assert.match(feed, /clientName: String\(root\.modelData\.sourceClient/);
    assert.match(reply, /clientName: String\(root\.reply\.sourceClient/);
});
