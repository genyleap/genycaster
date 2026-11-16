import { componentPath } from './ui_component_paths.mjs';
import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

const qml = name => readFileSync(new URL('../../ui/components/' + componentPath(name) + '.qml', import.meta.url), 'utf8');
const wasm = name => readFileSync(new URL('../../ui/wasm/' + name, import.meta.url), 'utf8');

test('Home keeps original layout with GENY banner and four compact slots', () => {
 const home=qml('HomeDashboard');
 assert.equal((home.match(/HubWorkspace \{/g)||[]).length,2);
 assert.match(home,/id: mobileFlick/);
 assert.match(home,/avatarSize: root\.androidCompact\s*\? \(root\.densePhone \? 50 : 64\)\s*: \(root\.densePhone \? 56 : 76\)/);
 for(const id of ['explorer','channels','apps','pro'])
   assert.match(home,new RegExp('destination: "'+id+'"'));
 assert.match(home,/HomeActivityCard \{/);
 assert.doesNotMatch(home,/key: "agent"|key: "studio"/);
});

test('Settings occupies the full phone viewport, without a partial-height sheet', () => {
    const settings=qml('LocaleSettingsPopup');
    assert.match(settings,/GenyAdaptiveDialog \{/);
    assert.match(settings,/mobileFullScreen: true/);
    assert.match(settings,/height: parent \? \(mobilePresentation \? parent\.height/);
    assert.match(settings,/showHandle: false/);
    const adaptive=qml('GenyAdaptiveDialog');
    assert.match(adaptive,/bottomSheet: true/);
    assert.match(adaptive,/interactive: !control\.mobileFullScreen/);
    assert.match(adaptive,/closePolicy: control\.closePolicy/);
});

test('Create sheet fits content and removes old top/bottom spacer', () => {
    const create=qml('MobileCreateMenu');
    assert.match(create,/id: sheetContent/);
    assert.match(create,/sheetContent\.implicitHeight \+ root\.topPadding \+ root\.bottomPadding/);
    assert.match(create,/topPadding: Theme\.space5/);
    assert.match(create,/bottomPadding: Theme\.space3 \+ Theme\.space3/);
    assert.match(create,/spacing: Theme\.space2/);
    assert.match(create,/Layout\.preferredHeight: 216/);
    assert.doesNotMatch(create,/Item \{ Layout\.fillHeight: true \}/);
    assert.doesNotMatch(create,/Math\.min\(360,/);
});

test('iOS PWA chrome starts dark and respects light appearance', () => {
    const html=wasm('genycaster-shell.html');
    const loader=wasm('genycaster-loader.js');
    assert.doesNotMatch(html,/<meta name="apple-mobile-web-app-status-bar-style"/);
    assert.match(html,/#ios-status-chrome\{[^}]*env\(safe-area-inset-top,0px\)[^}]*background-color:var\(--app-chrome\)/);
    assert.match(html,/<meta name="theme-color" content="#0b0c0f">/);
    assert.doesNotMatch(loader,/statusMeta\.setAttribute/);
    assert.match(loader,/dark\?"#0b0c0f":"#f1f1f3"/);
});
