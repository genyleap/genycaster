import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const qml = name => readFileSync(new URL('../../ui/components/' + name + '.qml', import.meta.url), 'utf8');

test('cast Share mobile sheet derives its height from actions and uses tight balanced gutters', () => {
    const src = qml('EngagementBar');
    const sheet = src.slice(src.indexOf('id: mobileCastShareSheet'), src.indexOf('id: castShareActionList'));
    assert.match(sheet, /height: parent\s*\? Math\.min\(parent\.height \* 0\.82,[\s\S]*?shareSheetContents\.implicitHeight \+ topPadding \+ bottomPadding/);
    assert.match(sheet, /contentPadding: Theme\.space2/);
    assert.match(sheet, /topPadding: Theme\.space5/);
    assert.match(sheet, /bottomPadding: Theme\.space3/);
    assert.match(sheet, /Layout\.preferredHeight: Theme\.scaleUi\(34\)/);
    assert.doesNotMatch(sheet, /Layout\.fillHeight: true/);
    const actions = src.slice(src.indexOf('id: castShareActionList'));
    assert.equal((actions.match(/compact: root\.compact/g) || []).length, 5);
    assert.equal((actions.match(/alignStart: true/g) || []).length, 5);
    assert.match(actions, /iconGlyph: Theme\.iconShare/);
    assert.match(actions, /iconGlyph: Theme\.iconCopy/);
});

test('sheet title minimum height no longer wastes vertical space or keeps an empty close slot', () => {
    const header = qml('ModalHeader');
    assert.match(header, /Layout\.minimumHeight: root\.sheetPresentation\s*\? Theme\.scaleUi\(32\)/);
    assert.match(header, /spacing: root\.sheetPresentation \? 0 : Theme\.space3/);
    assert.match(header, /horizontalAlignment: root\.sheetPresentation\s*\? Text\.AlignHCenter/);
    assert.match(header, /visible: root\.showClose && !root\.sheetPresentation/);
});

test('reorder affordances move to physical left in RTL without eating the title space', () => {
    const rail = qml('ContextRail');
    assert.match(rail, /Layout\.leftMargin: Theme\.rtl \? 34 : 0/);
    assert.match(rail, /Layout\.rightMargin: Theme\.rtl \? 0 : 34/);
    const handle = rail.slice(rail.indexOf('id: reorderHandle'), rail.indexOf('id: dragHover'));
    assert.match(handle, /x: Theme\.rtl \? Theme\.space2/);
    assert.match(handle, /: widgetDelegate\.width - width - Theme\.space2/);
    assert.match(handle, /LayoutMirroring\.enabled: false/);
    assert.doesNotMatch(handle, /anchors\.left:/);
    assert.doesNotMatch(handle, /anchors\.right:/);
    assert.match(rail, /text: card\.headerText\s*alignToUi: true/);
    assert.match(rail, /text: card\.headerDetail\s*alignToUi: true/);
    assert.match(rail, /implicitHeight: body\.implicitHeight \+ Theme\.space3 \* 2/);
});

test('RTL feed pulse chart mirrors its chronology and mobile widget drawer avoids doubled gutters', () => {
    const pulse = qml('FeedPulseCard');
    assert.match(pulse, /Layout\.leftMargin: Theme\.rtl \? 34 : 0/);
    assert.match(pulse, /Layout\.rightMargin: Theme\.rtl \? 0 : 34/);
    assert.match(pulse, /Layout\.preferredHeight: 54[\s\S]*?layoutDirection: Theme\.uiDirection/);
    assert.match(pulse, /text: root\.freshnessText\(\)\s*alignToUi: true/);
    const shell = qml('AppShell');
    const drawer = shell.slice(shell.indexOf('id: contextDrawer'), shell.indexOf('id: floatingCompose'));
    assert.match(drawer, /fullScreen: root\.mobile/);
    assert.match(drawer, /contentPadding: 0/);
    assert.match(drawer, /Layout\.leftMargin: root\.mobile \? Theme\.space3 : 0/);
    assert.match(drawer, /Layout\.rightMargin: root\.mobile \? Theme\.space3 : 0/);
    const rail = qml('ContextRail');
    assert.match(rail, /GridLayout \{\s*Layout\.fillWidth: true\s*layoutDirection: Theme\.uiDirection/);
});
