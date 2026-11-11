import { componentPath } from './ui_component_paths.mjs';
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const qml = name=>readFileSync(new URL('../../ui/components/' + componentPath(name)+'.qml',import.meta.url),'utf8');

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
    for(const q of [settings,composer])
        assert.match(q,/interactive: false/);
    // The small Create menu is intentionally gesture-dismissable, unlike
    // scrollable full-screen editors.
    assert.match(create,/interactive: true/);
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
        assert.match(part,/duration: (?:control\.sheetMotionDuration|Theme\.motionStandard)/);
        assert.doesNotMatch(part,/property: "(?:x|y)"/);
    }
    assert.match(base,/sheetMotionDuration: Theme\.reduceMotion \? 0 : 275/);
    assert.match(search,/opacity: root\.position/);
});

test('Home presents a compact banner, three navigation cards and passive Wallet',()=>{
 const home=qml('HomeDashboard');
 assert.match(home,/component HomeActivityCard: Superellipse/);
 assert.match(home,/destination: "wallet"/);
 assert.match(home,/enabled: !shortcut.locked/);
 assert.doesNotMatch(home,/key: "agent"|key: "studio"/);
});
