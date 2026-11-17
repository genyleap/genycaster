import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync, statSync } from 'node:fs';

const load = file => readFileSync(new URL('../../' + file, import.meta.url), 'utf8');

test('desktop onboarding has a balanced editorial split without hiding the GPU sculpture', () => {
    const qml = load('ui/components/setup/InitialSetup.qml');
    const hero = qml.slice(qml.indexOf('id: desktopBrandPane'), qml.indexOf('id: setupProgress'));
    assert.match(hero, /visible: !root\.phone && root\.width >= 980/);
    assert.match(hero, /width: Math\.min\(parent\.width \* 0\.35, 560\)/);
    assert.match(hero, /width: Math\.min\(parent\.width \* 0\.91, parent\.height \* 0\.71\)/);
    assert.match(hero, /id: brandSecondaryOrbit/);
    assert.match(hero, /rotation: 36 - desktopBrandPane\.orbitYaw \* 0\.13/);
    assert.match(hero, /rotation: -18 \+ desktopBrandPane\.orbitYaw \* 0\.09/);
    assert.match(hero, /gradient: Gradient \{[\s\S]*?orientation: Gradient\.Horizontal/);
    assert.match(hero, /id: automaticRotation/);
    assert.match(hero, /motionAllowed: root\.visible && visible && !Theme\.reduceMotion/);
    assert.equal((hero.match(/layer\.effect: MultiEffect/g) || []).length, 1);
    assert.doesNotMatch(hero, /blurEnabled: true|Animation\.Infinite/);
});

test('welcome copy, progress and CTA share an aligned center without changing mobile footer', () => {
    const qml = load('ui/components/setup/InitialSetup.qml');
    const welcome = qml.slice(qml.indexOf('id: welcomeStage'), qml.indexOf('id: languageStage'));
    const cta = qml.slice(qml.indexOf('id: stageAction'), qml.indexOf('id: welcomeStage'));
    assert.match(welcome, /id: welcomeContent/);
    assert.match(welcome, /font\.pixelSize: root\.phone \? 32 : 40/);
    assert.match(welcome, /horizontalAlignment: root\.phone \? Theme\.textStartAlignment : Text\.AlignHCenter/g);
    assert.match(welcome, /Layout\.alignment: Qt\.AlignHCenter/);
    assert.match(welcome, /markerSpacing: root\.phone \? 8 : 10/);
    assert.match(cta, /anchors\.bottom: parent\.bottom/);
    assert.match(cta, /anchors\.bottomMargin: !root\.phone && root\.currentStep === 0/);
    assert.match(cta, /stagePanel\.implicitHeight/);
    assert.match(cta, /root\.phone \? Theme\.space5/);
    assert.match(cta, /neutralPrimary: false/);
    assert.match(qml, /\(stageScroll\.height - stageLoader\.implicitHeight[\s\S]*?- Theme\.space7 - stageAction\.implicitHeight\) \* 0\.47/);
});

test('original SVG is raymarched with richer metallic reflections in portable QSB', () => {
    const shader = load('ui/shaders/setup_glass_g.frag');
    const cmake = load('scripts/regenerate-setup-shader.cmake');
    assert.match(shader, /uniform sampler2D logoDistanceMap/);
    assert.match(shader, /uniform sampler2D logoNormalMap/);
    assert.match(shader, /float azureStrip/);
    assert.match(shader, /vec2 blueCore/);
    assert.match(shader, /float sideKey/);
    assert.match(shader, /sin\(params\.orbitYaw\*/);
    assert.match(shader, /for\(int i=0;i<116;i\+\+\)/);
    assert.match(cmake, /--glsl 100es,120,150 --hlsl 50 --msl 12/);
    assert.match(cmake, /logoDistanceMap/);
    assert.match(cmake, /logoNormalMap/);
    assert.ok(statSync(new URL('../../ui/shaders/setup_glass_g.frag.qsb', import.meta.url)).size > 19000);
});
