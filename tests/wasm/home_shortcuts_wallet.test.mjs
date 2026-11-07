import { componentPath } from './ui_component_paths.mjs';
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';

const qml = file => readFileSync(new URL('../../ui/components/' + componentPath(file) + '.qml', import.meta.url), 'utf8');

test('Home routes Explorer, Search, Apps and keeps Wallet disabled', () => {
 const home=qml('HomeDashboard');
 const shell=qml('AppShell');
 const actions=['explorer','search','apps'];
 const block=home.slice(home.indexOf('function activateHubShortcut'),home.indexOf('component HubWorkspace'));
 const calls=[];
 const root={exploreRequested:()=>calls.push('explorer'),searchRequested:()=>calls.push('search'),appsRequested:()=>calls.push('apps')};
 const dispatch=runInNewContext('('+block.trim()+')',{root});
 for (const action of actions) dispatch(action);
 dispatch('wallet');
 assert.deepEqual(calls,actions);
 assert.match(home,/destination: "wallet"/);
 assert.match(home,/enabled: !shortcut.locked/);
 assert.match(home,/Accessible.role: locked \? Accessible.StaticText : Accessible.Button/);
 assert.doesNotMatch(home,/destination: "trends"|key: "agent"|key: "studio"/);
 assert.match(shell,/onSearchRequested: searchPalette.open\(\)/);
 assert.match(shell,/onAppsRequested: root.openShellRoute\("apps"\)/);
});

test('GENY banners rotate without token price claims or fake navigation',()=>{
 const home=qml('HomeDashboard');
 assert.match(home,/component GenyTokenBanner: Superellipse/);
 assert.equal((home.match(/GenyTokenBanner \{/g)||[]).length,2);
 assert.match(home,/interval: 6200/);
 assert.match(home,/objectName: "genyTokenBanner"/);
 assert.doesNotMatch(home,/tokenPrice|GENY.*price/);
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
