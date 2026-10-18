import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const source = name => readFileSync(new URL('../../ui/components/' + name, import.meta.url), 'utf8');

test('Home quick composer transfers text to the rich Cast paragraph', () => {
    const home = source('HomeDashboard.qml');
    const shell = source('AppShell.qml');
    const composer = source('ComposerDrawer.qml');
    assert.equal((home.match(/onClicked: root\.composeRequested\(root\.draftText\)/g) || []).length, 4);
    assert.match(shell, /onComposeRequested: text => root\.openComposer\(null, "", text\)/);
    assert.match(shell, /composerDrawer\.suggestedText = String\(suggestedText \|\| ""\)/);
    assert.match(composer, /!root\.parentCast && root\.suggestedText\.length > 0/);
    assert.match(composer, /richComposer\.setBlockText\(0, root\.suggestedText\)/);
    assert.match(shell, /homeDashboard\.draftText === root\.homeDraftForCast/);
    assert.match(shell, /homeDashboard\.draftText = ""/);
    assert.match(shell, /root\.homeDraftForCast = ""/);
});

test('Mobile Settings is a full-height grouped index with accessible detail navigation', () => {
    const settings = source('LocaleSettingsPopup.qml');
    const shell = source('AppShell.qml');
    assert.match(settings, /height: parent \? parent\.height : 820/);
    assert.match(settings, /component MobileSettingsGroup: ColumnLayout/);
    assert.match(settings, /id: mobileSettingsIndex/);
    assert.match(settings, /visible: root\.mobilePresentation && root\.mobileIndexOpen/);
    assert.match(settings, /visible: !root\.mobilePresentation \|\| !root\.mobileIndexOpen/);
    assert.match(settings, /onTapped: root\.openMobileSection\(/);
    assert.match(settings, /section: 5/);
    assert.match(settings, /section: -1/);
    assert.match(settings, /if \(root\.mobileIndexOpen\) root\.close\(\)/);
    assert.match(shell, /popup\.mobileIndexOpen = requestedSection < 0/);
});

test('Mobile full Cast footer stacks compatibility hint above publish byte counts', () => {
    const composer = source('ComposerDrawer.qml');
    assert.match(composer, /columns: root\.compact \? 1 : 2/);
    assert.match(composer, /Layout\.fillWidth: root\.compact\n\s+Layout\.minimumWidth: 0/);
    assert.match(composer, /wrapMode: Text\.Wrap\n\s+text: qsTrId\("genycaster\.composer\.editor_count"\)/);
});

test('mobile Settings navigation categories are translated for every built-in locale', () => {
    const locales = ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
        'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
    const ids = ['mobile_recovery_options','mobile_account_security',
        'mobile_personalization','mobile_feeds_content','mobile_app_storage'];
    for (const locale of locales) {
        const ts=readFileSync(new URL('../../ui/i18n/qml_'+locale+'.ts',import.meta.url),'utf8');
        for (const key of ids) {
            const needle='id="genycaster.settings.'+key+'"';
            assert.ok(ts.includes(needle),locale+' missing '+key);
        }
    }
});
