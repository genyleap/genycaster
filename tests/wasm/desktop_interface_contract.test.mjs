import {readFileSync} from 'node:fs';
import assert from 'node:assert/strict';
import test from 'node:test';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('share menus show concise, untruncated action labels', () => {
    const cast = read('ui/components/EngagementBar.qml');
    const profile = read('ui/components/ProfileSurface.qml');
    assert.match(cast, /text: qsTrId\("genycaster.share.share_action"\)/);
    assert.match(cast, /text: qsTrId\("genycaster.share.copy_link"\)/);
    assert.doesNotMatch(cast, /text: qsTr\("Copy Farcaster URI"\)/);
    assert.match(profile, /text: qsTrId\("genycaster\.profile\.share_profile"\)/);
    assert.match(profile, /text: qsTrId\("genycaster.share.copy_profile_link"\)/);
    assert.doesNotMatch(profile, /text: qsTr\("Copy Farcaster URI"\)/);
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
    assert.match(home, /Math\.min\(root\.homeContentMaxWidth,/);
    assert.match(home, /anchors\.horizontalCenter: parent\.horizontalCenter/);
    assert.match(home, /objectName: "homeComposerLauncher"/);
    assert.match(home, /GenyTokenBanner \{\}/);
});

test('desktop store uses borderless app rows and compact Open buttons', () => {
    const apps = read('ui/components/AppsSurface.qml');
    const feature = read('ui/components/MiniAppFeaturedCarousel.qml');
    assert.match(apps, /cellHeight: root\.compact \? 116 : 112/);
    assert.match(apps, /id: storeRow/);
    assert.match(apps, /objectName: "miniAppDesktopStoreRow"/);
    assert.match(apps, /objectName: "miniAppStoreOpenButton"/);
    assert.ok(apps.includes('text: cardCell.launchBusy ? qsTrId("genycaster.apps.opening")'));
    assert.match(apps, /: cardCell\.running \? qsTrId\("genycaster.apps.resume"\) : qsTrId\("genycaster.apps.open"\)/);
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


test('all platforms share consistent app-first client actions', () => {
    const cast = read('ui/components/EngagementBar.qml');
    const profile = read('ui/components/ProfileSurface.qml');
    const bridge = read('src/presentation/qt/presentation_bridge.cpp');
    const header = read('src/presentation/qt/presentation_bridge.hpp');

    assert.ok(cast.includes('text: qsTrId("genycaster.share.open_quorum")'));
    assert.ok(cast.includes('text: qsTrId("genycaster.share.open_farcaster")'));
    assert.ok(cast.includes('presentationBridge.openQuorumCast(root.normalizedCastId)'));
    assert.ok(cast.includes('presentationBridge.openFarcasterCast('));
    assert.ok(profile.includes('presentationBridge.openFarcasterProfile('));
    assert.ok(!cast.includes('Quorum Web'));
    assert.ok(!cast.includes('Copy Quorum URI'));
    assert.ok(!cast.includes('Copy Farcaster URI'));
    assert.ok(!profile.includes('Copy Farcaster URI'));
    assert.ok(!cast.includes('visible: Qt.platform.os !== "wasm"'));
    assert.ok(header.includes('Q_INVOKABLE void openFarcasterCast'));
    assert.ok(header.includes('Q_INVOKABLE void openFarcasterProfile'));
    assert.ok(bridge.includes('openBrowserUniversalClientLink(url)'));
    assert.ok(bridge.includes('window.location.assign(href)'));
    assert.ok(bridge.includes('window.open(href,'));
    assert.ok(bridge.includes('const QString appUri = quorumCastUri(castId)'));
    assert.ok(bridge.includes('https://app.quorummessenger.com/'));
    assert.ok(bridge.includes("document.addEventListener('visibilitychange', markHidden)"));
    assert.ok(bridge.includes('if (!appOpened && !document.hidden)'));
    assert.ok(bridge.includes('window.location.assign(fallback)'));
    assert.ok(bridge.includes('if (!QDesktopServices::openUrl(QUrl{appUri}))'));
    assert.ok(bridge.includes('openExternalUrl(webUrl)'));
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

test('mobile Apps show the same compact store rows as desktop with topic filters', () => {
    const apps=read('ui/components/AppsSurface.qml');
    const library=read('ui/components/MiniAppLibraryShelves.qml');
    assert.match(apps,/targetColumns: root\.compact\s*\? 1/);
    assert.match(apps,/cellHeight: root\.compact \? 116 : 112/);
    assert.match(apps,/objectName: "miniAppMobileStoreRow"/);
    assert.match(apps,/objectName: "miniAppMobileOpenButton"/);
    assert.match(apps,/objectName: "miniAppMobileTopicFilter"/);
    assert.match(apps,/onLaunchRequested: function\(app\) \{ root\.openApp\(app\) \}/);
    assert.match(library,/readonly property int savedColumns: 1/);
    assert.match(library,/objectName: "savedMiniAppOpenButton"/);
    assert.match(library,/signal launchRequested\(var app\)/);
    assert.match(library,/orientation: ListView\.Horizontal/);
});

test('native update sheet shows published release metadata with vertical drawer motion', () => {
    const drawer=read('ui/components/UpdateNoticeDrawer.qml');
    const header=read('ui/app/update_controller.hpp');
    const cpp=read('ui/app/update_controller.cpp');
    assert.match(drawer,/edge: Qt\.TopEdge/);
    assert.match(drawer,/property: "position"/);
    assert.match(drawer,/updateController\.releaseTitle/);
    assert.match(drawer,/updateController\.releaseNotes/);
    assert.match(drawer,/updateController\.releasePublishedAt/);
    assert.match(header,/Q_PROPERTY\(QString releaseNotes READ releaseNotes/);
    assert.match(cpp,/obj\.value\(QStringLiteral\("body"\)\)/);
    assert.match(cpp,/obj\.value\(QStringLiteral\("published_at"\)\)/);
});

test('release notice has fixed header and footer with only release notes scrollable', () => {
    const source = read('ui/components/UpdateNoticeDrawer.qml');
    assert.match(source,/contentItem: Item \{[\s\S]*?id: updateLayoutViewport/);
    assert.match(source,/id: updateFixedChrome/);
    assert.match(source,/id: updateScrollableSection[\s\S]*?Layout.minimumHeight: 0[\s\S]*?Layout.preferredHeight: 0/);
    assert.match(source,/objectName: "updateReleaseNotesScroll"[\s\S]*?flickableDirection: Flickable.VerticalFlick/);
    assert.match(source,/objectName: "updateNoticeStickyActions"/);
    assert.ok(source.indexOf('id: updateFixedChrome') < source.indexOf('id: updateScrollableSection'));
    assert.ok(source.indexOf('id: updateScrollableSection') < source.indexOf('objectName: "updateNoticeStickyActions"'));
});
