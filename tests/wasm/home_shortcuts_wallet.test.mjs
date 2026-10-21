import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';

const qml = file => readFileSync(new URL('../../ui/components/' + file + '.qml', import.meta.url), 'utf8');

test('every Home shortcut dispatches its corresponding, existing route', () => {
    const home = qml('HomeDashboard');
    const actions = ['explorer', 'search', 'apps', 'trends'];
    const start = home.indexOf('    function activateHubShortcut(destination) {');
    const end = home.indexOf('\n    }',start) + 6;
    assert.ok(start >= 0 && end > start);
    const source = home.slice(start,end).trim();
    const events = [];
    const root = {
        exploreRequested: () => events.push('explorer'),
        searchRequested: () => events.push('search'),
        appsRequested: () => events.push('apps'),
        trendsRequested: () => events.push('trends'),
    };
    const dispatch = runInNewContext('(' + source + ')', {root});
    for (const action of actions) {
        assert.match(home,new RegExp('destination: "'+action+'"'));
        dispatch(action);
    }
    dispatch('wallet');
    dispatch('agent');
    dispatch('studio');
    assert.deepEqual(events,actions);
    assert.match(home,/Accessible\.onPressAction: root\.activateHubShortcut\(modelData\.destination\)/);
    assert.match(home,/onClicked: root\.activateHubShortcut\(modelData\.destination\)/);
});

test('Trends navigates to real Trending feed, and apps/search use existing destinations', () => {
    const shell = qml('AppShell');
    const start = shell.indexOf('    function openTrends() {');
    const end = shell.indexOf('    function openSuggestedProfile(',start);
    assert.ok(start >= 0 && end > start);
    const trends = shell.slice(start,end);
    assert.match(trends,/root\.primaryDestination = "explorer"/);
    assert.match(trends,/root\.explorerPresentationMode = "for-you"/);
    assert.match(trends,/presentationBridge\.selectFeed\("trending"\)/);
    assert.match(trends,/feedSurfaceLoader\.item\.scrollToBeginning\(\)/);
    assert.match(shell,/onTrendsRequested: root\.openTrends\(\)/);
    assert.match(shell,/onExploreRequested: root\.openExplorer\(\)/);
    assert.match(shell,/onSearchRequested: searchPalette\.open\(\)/);
    assert.match(shell,/onAppsRequested: root\.openShellRoute\("apps"\)/);
});

test('Wallet spans a full row beneath passive Agent and Studio cards', () => {
    const home = qml('HomeDashboard');
    const locked = home.slice(home.indexOf('{ key: "agent"'),home.indexOf('    RowLayout {\n        id: desktopLayout'));
    assert.match(locked,/\{ key: "agent"/);
    assert.match(locked,/\{ key: "studio"/);
    assert.match(locked,/\{ key: "wallet"/);
    assert.match(home,/Layout.columnSpan: isWallet \? 2 : 1/);
    assert.match(locked,/text: "system\/lock"/);
    assert.match(locked,/Accessible\.role: Accessible\.StaticText/);
    assert.doesNotMatch(locked,/\n\s*(?:MouseArea|TapHandler)\s*\{|\bonClicked:|\bonActivated:/);
});

test('five new shortcut and wallet strings exist in every supported locale', () => {
    const locales=['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR','hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
    const ids=['hub_quick_access','hub_trends','hub_trends_detail','hub_wallet','hub_wallet_detail'];
    for (const locale of locales) {
        const text=readFileSync(new URL('../../ui/i18n/qml_'+locale+'.ts',import.meta.url),'utf8');
        for(const key of ids)
            assert.equal(text.split('id="genycaster.home.'+key+'"').length-1,1,locale+': '+key);
    }
});
