import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const qml = name => readFileSync(new URL('../../ui/components/' + name + '.qml', import.meta.url), 'utf8');

test('Home renders rotating local draft starters for both mobile and desktop', () => {
    const home = qml('HomeDashboard');
    assert.match(home, /readonly property var castIdeas:\s*\[/);
    assert.match(home, /function ideaAt\(offset\)/);
    assert.match(home, /\(root\.ideaOffset \+ offset\) % root\.castIdeas\.length/);
    assert.equal((home.match(/CastIdeaSection \{/g) || []).length, 2);
    assert.match(home, /objectName: "homeFeaturedCastIdea"/);
    assert.match(home, /objectName: "homeIdeasRefresh"/);
    assert.match(home, /onClicked: root\.ideaOffset = \(root\.ideaOffset \+ 1\) % root\.castIdeas\.length/);
    assert.match(home, /onClicked: root\.composeRequested\(root\.ideaAt\(0\)\.prompt\)/);
    assert.match(home, /onClicked: root\.composeRequested\(root\.ideaAt\(1\)\.prompt\)/);
    assert.match(home, /onClicked: root\.composeRequested\(root\.ideaAt\(2\)\.prompt\)/);
    assert.match(home, /Accessible\.onPressAction: root\.composeRequested\(root\.ideaAt\(0\)\.prompt\)/);
    assert.doesNotMatch(home, /fetch\(|XMLHttpRequest\(|publishCast\(/);
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

test('all supported locales contain complete writing prompts and accessibility labels', () => {
    const locales = ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
        'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
    const ids = ['ideas_title','ideas_refresh','ideas_hint',
        'idea_dev_topic','idea_dev_prompt','idea_nft_topic','idea_nft_prompt',
        'idea_opinion_topic','idea_opinion_prompt','idea_privacy_topic','idea_privacy_prompt',
        'idea_farcaster_topic','idea_farcaster_prompt'];
    for (const locale of locales) {
        const ts = readFileSync(new URL('../../ui/i18n/qml_' + locale + '.ts', import.meta.url), 'utf8');
        for (const id of ids) {
            const needle = 'id="genycaster.home.' + id + '"';
            assert.equal(ts.split(needle).length - 1, 1, locale + ': missing or duplicate ' + id);
        }
    }
});
