import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const qml = name=>readFileSync(new URL('../../ui/components/'+name+'.qml',import.meta.url),'utf8');

test('all drawer families close on outside press without dragging their scrolling content',()=>{
    const base=qml('GenyDrawer'), backdrop=qml('DrawerBackdrop');
    const search=qml('SearchPalette'),settings=qml('LocaleSettingsPopup');
    const composer=qml('ComposerDrawer'), create=qml('MobileCreateMenu');
    const account=qml('AppShell');
    assert.match(base,/closePolicy: Popup\.CloseOnEscape \| Popup\.CloseOnPressOutside/);
    assert.match(base,/Overlay\.modal: DrawerBackdrop \{/);
    assert.match(base,/onBackgroundTapped: control\.close\(\)/);
    assert.match(backdrop,/onPressed: mouse => \{[\s\S]*?root\.requestOutsideDismiss\(\)/);
    assert.match(search,/closePolicy: Popup\.CloseOnEscape \| Popup\.CloseOnPressOutside/);
    assert.match(search,/onBackgroundTapped: root\.close\(\)/);
    assert.match(settings,/closePolicy: Popup\.CloseOnEscape \| Popup\.CloseOnPressOutside/);
    assert.match(settings,/height: parent \? parent\.height : 820/);
    for(const q of [settings,composer,create])
        assert.match(q,/interactive: false/);
    assert.match(account,/GenyDrawer \{\s+id: accountPopup/);
});

test('centered modal outside click dismisses while intercepting input from underlying page',()=>{
    const modal=qml('GenyModal');
    assert.match(modal,/closePolicy: Popup\.CloseOnEscape \| Popup\.CloseOnPressOutside/);
    assert.match(modal,/onPressed: function\(mouse\) \{[\s\S]*?mouse\.accepted = true[\s\S]*?control\.close\(\)/);
    assert.match(modal,/Overlay\.modal: Rectangle/);
});

test('drawers animate from owning edge with reduced-motion safe transitions',()=>{
    const base=qml('GenyDrawer'), search=qml('SearchPalette');
    for(const part of [base,search]){
        assert.match(part,/enter: Transition \{/);
        assert.match(part,/exit: Transition \{/);
        assert.match(part,/property: "position"/);
        assert.match(part,/duration: (?:Theme\.reduceMotion \? 0 : )?Theme\.motionStandard/);
        assert.doesNotMatch(part,/property: "(?:x|y)"/);
    }
    assert.match(base,/Theme\.reduceMotion \? 0 : Theme\.motionStandard/);
    assert.match(search,/opacity: root\.position/);
});

test('Home restores four familiar tiles and compacts three future cards',()=>{
    const home=qml('HomeDashboard');
    assert.match(home,/"homeShortcut_" \+ modelData.destination/);
    assert.match(home,/columns: 2/);
    assert.match(home,/columns: 3/);
    assert.match(home,/avatarSize: root\.densePhone \? 56 : 76/);
    assert.match(home,/Intentionally non-interactive until the feature exists/);
    assert.match(home,/Layout.preferredHeight: root.compact \? 88 : 116/);
    assert.match(home,/function activateHubShortcut\(destination\)/);
    const en=readFileSync(new URL('../../ui/i18n/qml_en_US.ts',import.meta.url),'utf8');
    for(const [id,title] of [['hub_agent','Agent'],['hub_studio','Studio']]){
        assert.match(en,new RegExp('id="genycaster.home.'+id+'"[\\s\\S]*?<translation>'+title+'</translation>'));
    }
});
