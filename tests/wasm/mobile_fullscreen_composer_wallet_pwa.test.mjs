import { componentPath } from './ui_component_paths.mjs';
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const qml = file => readFileSync(new URL('../../ui/components/' + componentPath(file) + '.qml', import.meta.url), 'utf8');
const wasm = file => readFileSync(new URL('../../ui/wasm/' + file, import.meta.url), 'utf8');

test('Home offers implemented shortcuts with Search available as a separate action', () => {
 const home=qml('HomeDashboard');
 for(const id of ['explorer','channels','apps','pro'])
   assert.match(home,new RegExp('destination: "'+id+'"'));
 assert.doesNotMatch(home,/destination: "wallet"|destination: "trends"|key: "agent"|key: "studio"/);
 assert.match(home,/if \(destination === "search"\) root\.searchRequested\(\)/);
 assert.match(home,/onClicked: root\.searchRequested\(\)/);
 assert.match(home,/objectName: "homeActivityCard"/);
 assert.match(home,/component HomeActivityCard: NeonCard/);
 assert.match(home,/enabled: !shortcut.locked/);
});

test('Mobile home launcher is vertically centered, accessible and skips inline input', () => {
    const home=qml('HomeDashboard'), shell=qml('AppShell');
    assert.match(home,/objectName: "homeComposerLauncher"/);
    assert.match(home,/Layout\.preferredHeight: root\.androidCompact \? 52 : 60/);
    assert.match(home,/verticalAlignment: Text\.AlignVCenter/);
    assert.match(home,/Accessible\.onPressAction: root\.composeRequested\(root\.draftText\)/);
    assert.match(home,/onClicked: root\.composeRequested\(root\.draftText\)/);
    assert.match(home,/id: mobileDraftEditor[\s\S]*?visible: false[\s\S]*?readOnly: true/);
    assert.match(shell,/onComposeRequested: text => root\.openComposer\(null, "", text\)/);
    assert.match(shell,/onCreateRequested: root\.openComposer\(null\)/);
});

test('Full-height mobile composer retains publishing, replies, saved drafts and live media actions', () => {
    const composer=qml('ComposerDrawer');
    assert.match(composer,/fullScreen: root\.compact/);
    assert.match(composer,/height: parent \? \(root\.compact \? parent\.height/);
    assert.match(composer,/showHandle: !root\.fullScreen/);
    assert.match(composer,/objectName: "mobileComposerNavigation"/);
    assert.match(composer,/qsTrId\("genycaster\.shell\.cancel"\)/);
    assert.match(composer,/qsTrId\("genycaster\.composer\.save_draft"\)/);
    assert.match(composer,/onClicked: root\.publishCurrent\(\)/);
    assert.match(composer,/objectName: "mobileComposerMediaBar"/);
    assert.match(composer,/root\.openMediaPicker\(\)/);
    assert.match(composer,/richComposer\.saveDraft\(\)/);
    assert.match(composer,/richComposer\.restoreDraft\(\)/);
    assert.match(composer,/root\.replyRequested\(/);
    assert.match(composer,/root\.threadRequested\(/);
    assert.match(composer,/initialComposerFocus\.restart\(\)/);
    for(const locale of ['en_US','fa_IR','ar_SA','fr_FR']) {
        const ts=readFileSync(new URL('../../ui/i18n/qml_'+locale+'.ts', import.meta.url),'utf8');
        assert.match(ts,/id="genycaster\.composer\.save_draft"/);
    }
});

test('Floating Create sheet curves exposed corners and preserves attached Drawer edge', () => {
    const base=qml('GenyDrawer'), menu=qml('MobileCreateMenu'), theme=readFileSync(new URL('../../ui/theme/Theme.qml',import.meta.url),'utf8');
    assert.match(base,/property bool floatingSheet: false/);
    // An edge-attached Qt Drawer must not add an external inset on its
    // originating edge: this would desynchronize gesture and sheet geometry.
    assert.match(base,/leftInset: floatingSheet && edge !== Qt\.LeftEdge \? Theme\.space3 : 0/);
    assert.match(base,/rightInset: floatingSheet && edge !== Qt\.RightEdge \? Theme\.space3 : 0/);
    assert.match(base,/bottomInset: floatingSheet && edge !== Qt\.BottomEdge \? Theme\.space3 : 0/);
    assert.match(base,/curveTopLeft: !control\.fullScreen && \(control\.bottomSheet \|\| control\.edge !== Qt\.LeftEdge\)/);
    assert.match(base,/curveTopRight: !control\.fullScreen && \(control\.bottomSheet \|\| control\.edge !== Qt\.RightEdge\)/);
    assert.match(base,/curveBottomLeft: !control\.fullScreen && !control\.bottomSheet && control\.edge !== Qt\.LeftEdge/);
    assert.match(base,/curveBottomRight: !control\.fullScreen && !control\.bottomSheet && control\.edge !== Qt\.RightEdge/);
    assert.match(base,/curveDepth: control\.fullScreen \? 0/);
    assert.match(theme,/curveSheetFloating: scaleUi\(36\)/);
    assert.match(menu,/bottomSheet: true/);
    assert.match(menu,/floatingSheet: true/);
    assert.match(menu,/objectName: "createUnifiedActions"/);
    assert.match(menu,/available: false/);
});

test('iOS standalone uses full viewport bleed and theme-aware top inset', () => {
    const html=wasm('genycaster-shell.html'), loader=wasm('genycaster-loader.js');
    assert.match(html,/viewport-fit=cover/);
    assert.doesNotMatch(html,/<meta name="apple-mobile-web-app-status-bar-style"/);
    assert.match(html,/#ios-status-chrome\{[^}]*z-index:6/);
    assert.match(html,/:root\[data-chrome-theme="light"\]\{[^}]*--boot-ink/);
    assert.match(html,/:root\[data-chrome-theme="dark"\]\{[^}]*--boot-ink/);
    assert.doesNotMatch(loader,/statusMeta\.setAttribute/);
    assert.match(loader,/__genycasterSetBrowserChromeTheme=applyBrowserChromeTheme/);
});

test('GIF selection is offered and accepted end to end by native MIME policy',()=>{
 const composer=qml('ComposerDrawer');
 const picker=readFileSync(new URL('../../src/presentation/qt/media_upload_controller.cpp',import.meta.url),'utf8');
 const upload=readFileSync(new URL('../../src/transport/qt_network/media_upload.cpp',import.meta.url),'utf8');
 assert.match(composer,/\*\.gif/);
 assert.match(composer,/text: "GIF"/);
 assert.match(picker,/SupportedMedia\{u"gif", u"image\/gif", u"image"\}/);
 assert.match(upload,/contentType == QStringLiteral\("image\/gif"\)/);
});
