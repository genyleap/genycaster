import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const qml = name => readFileSync(new URL('../../ui/components/' + name + '.qml', import.meta.url), 'utf8');

test('Home provides a real composer and a future-facing noninteractive hub', () => {
    const home = qml('HomeDashboard');
    assert.match(home, /component HubWorkspace: ColumnLayout/);
    assert.match(home, /"homeLocked_" \+ modelData.key/);
    assert.match(home, /key: "agent"/);
    assert.match(home, /key: "studio"/);
    assert.match(home, /key: "wallet"/);
    assert.equal((home.match(/HubWorkspace \{/g) || []).length, 2);
    assert.match(home, /onClicked: root.activateHubShortcut\(modelData.destination\)/);
    assert.doesNotMatch(home, /CastIdeaSection|homeContinuation|homeRecentCastPreview/);
});

test('mobile nav Profile replaces redundant Settings, with guest sign-in preserved', () => {
    const header = qml('AppHeader');
    const nav = qml('BottomNavigation');
    const shell = qml('AppShell');
    assert.match(header, /visible: !root\.compact \|\| !root\.isAuthenticated/);
    assert.match(header, /text: Theme\.iconLeaf[\s\S]*?color: Theme\.accent/);
    assert.doesNotMatch(header, /fillColor: Theme\.primaryFill/);
    assert.match(nav, /signal profileRequested\(\)/);
    assert.match(nav, /text: qsTrId\("genycaster\.side_nav\.profile"\)[\s\S]*?onClicked: root\.profileRequested\(\)/);
    assert.doesNotMatch(nav, /onClicked: root\.settingsRequested\(\)/);
    assert.match(shell, /onProfileRequested: \{[\s\S]*?presentationBridge\.isAuthenticated[\s\S]*?root\.openMyProfileFromNavigation\(\)[\s\S]*?root\.openAccountPopup\(\)/);
    assert.match(header, /onClicked: root\.settingsRequested\(\)/);
});

test('all supported locales include Hub, Agent, Studio and Create labels', () => {
    const locales = ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
        'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
    const ids = ['hub_workspace','hub_coming_soon','hub_agent','hub_agent_detail',
        'hub_studio','hub_studio_detail','hub_discover','hub_discover_detail',
        'hub_tagline','hub_photo_detail','hub_quick_access',
        'hub_trends','hub_trends_detail','hub_wallet','hub_wallet_detail'];
    for (const locale of locales) {
        const ts = readFileSync(new URL('../../ui/i18n/qml_' + locale + '.ts', import.meta.url), 'utf8');
        for (const id of ids) {
            const needle = 'id="genycaster.home.' + id + '"';
            assert.equal(ts.split(needle).length - 1, 1, locale + ': missing or duplicate ' + id);
        }
    }
});
