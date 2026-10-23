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


test('WASM only shows HTTP(S) share destinations, never mobile app schemes', () => {
    const cast = read('ui/components/EngagementBar.qml');
    const profile = read('ui/components/ProfileSurface.qml');
    assert.match(cast, /visible: Qt\.platform\.os !== "wasm"[\s\S]*?text: qsTr\("Open in Quorum"\)/);
    assert.match(cast, /visible: Qt\.platform\.os !== "wasm"[\s\S]*?text: qsTr\("Copy Quorum URI"\)/);
    assert.match(cast, /visible: Qt\.platform\.os !== "wasm"\s*&& presentationBridge\.farcasterCastUri/);
    assert.match(profile, /visible: Qt\.platform\.os !== "wasm"[\s\S]*?text: qsTr\("Copy Farcaster URI"\)/);
    assert.match(cast, /presentationBridge\.castShareUrl\(root\.normalizedCastId\)/);
});
