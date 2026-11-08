import { componentPath } from './ui_component_paths.mjs';
import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

const qml = name => readFileSync(new URL('../../ui/components/' + componentPath(name) + '.qml', import.meta.url), 'utf8');

test('all primary destinations provide explicit scroll-to-start without wiping drafts', () => {
    const shell = qml('AppShell');
    const home = qml('HomeDashboard');
    const feed = qml('FeedSurface');
    const profile = qml('ProfileSurface');
    const apps = qml('AppsSurface');
    const settings = qml('LocaleSettingsPopup');

    assert.match(home, /function scrollToBeginning\(\) \{[\s\S]*?mobileFlick\.contentY = 0/);
    assert.match(feed, /function scrollToBeginning\(\) \{[\s\S]*?readList\.positionViewAtBeginning\(\)/);
    assert.match(profile, /function scrollToBeginning\(\) \{[\s\S]*?viewportRestoreTimer\.stop\(\)[\s\S]*?root\.readList\.positionViewAtBeginning\(\)/);
    assert.match(apps, /function scrollToBeginning\(\) \{[\s\S]*?appsScroller\.contentY = 0[\s\S]*?appGrid\.positionViewAtBeginning\(\)/);
    assert.match(settings, /function scrollToBeginning\(\) \{[\s\S]*?mobileSettingsIndex[\s\S]*?recoverySettingsScroll/);
    for (const destination of ['homeDashboard', 'feedSurfaceLoader.item', 'profileSurfaceLoader.item', 'appsSurfaceLoader.item'])
        assert.match(shell, new RegExp(destination.replaceAll('.', '\\.') + '\\.scrollToBeginning\\(\\)'));
    assert.match(shell, /popup\.scrollToBeginning\(\)/);
    assert.doesNotMatch(home.slice(home.indexOf('function scrollToBeginning()'),home.indexOf('signal exploreRequested')), /draftText\s*=/);
});

test('Explorer tab does not force network refresh when returning to the top', () => {
    const shell = qml('AppShell');
    const start = shell.indexOf('function openExplorer()');
    const end = shell.indexOf('function openSuggestedProfile(', start);
    const section = shell.slice(start,end);
    assert.match(section, /const wasExplorer/);
    assert.match(section, /if \(!wasExplorer\)/);
    assert.match(section, /selectExplorerPresentationMode\(root\.explorerPresentationMode, false\)/);
    assert.match(section, /\+\+root\.explorerTopRevision/);
    assert.match(shell, /scrollRequestRevision: root\.primaryDestination === "explorer"/);
    assert.match(qml('FeedSurface'), /function honorPrimaryScrollRequest\(\)/);
    assert.doesNotMatch(section, /refreshLatestFeed\(\)/);
    assert.match(shell, /function returnFromCastDetail\(\) \{[\s\S]*?presentationBridge\.returnFromDetail\(\)/);
});

test('Create sheet is a unified curved surface with real actions and disabled Spaces', () => {
    const menu = qml('MobileCreateMenu');
    assert.match(menu, /objectName: "createUnifiedActions"/);
    assert.match(menu, /curveDepth: Theme\.curveLarge/);
    assert.equal((menu.match(/component CreateRow:/g) || []).length, 1);
    assert.equal((menu.match(/CreateRow \{/g) || []).length, 3);
    assert.match(menu, /onActivated: \{ root\.close\(\); root\.castRequested\(\) \}/);
    assert.match(menu, /onActivated: \{ root\.close\(\); root\.photoRequested\(\) \}/);
    assert.match(menu, /subtitleText: qsTrId\("genycaster\.home\.hub_coming_soon"\)/);
    assert.match(menu, /available: false/);
    assert.doesNotMatch(menu, /root\.spaceRequested\(\)/);
});
