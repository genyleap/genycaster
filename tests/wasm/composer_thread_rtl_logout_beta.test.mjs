import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

// Runtime binding functions live in a separate Qt composition unit to keep
// GCC C++26 module compilation bounded; test both halves as one read pipeline.
function readGenycasterMain() {
  return readFileSync(new URL('../../ui/app/main.cpp', import.meta.url), 'utf8') +
    '\n' + readFileSync(new URL('../../ui/app/runtime_ui_bindings.cpp', import.meta.url), 'utf8');
}
const read=name=>readFileSync(new URL('../../'+name, import.meta.url),'utf8');

test('thread composer owns vertical scrolling and cannot be swiped away mid-edit',()=>{
 const composer=read('ui/components/ComposerDrawer.qml');
 assert.match(composer,/interactive: false/);
 assert.match(composer,/contentHeight: composerContent\.implicitHeight/);
 assert.match(composer,/id: composerContent/);
 assert.match(composer,/threadRepeater\.itemAt\(index\)/);
 assert.match(composer,/row\.focusThreadEditor\(\)/);
 assert.doesNotMatch(composer,/flick\.contentY = Math\.max\(0, flick\.contentHeight - flick\.height\)/);
 assert.match(composer,/delegate: ColumnLayout \{\s*id: threadRow/);
 assert.match(composer,/objectName: "composerThreadEditor"/);
 assert.doesNotMatch(composer,/anchors\.right: threadActions\.left/);
 assert.match(composer,/Layout\.minimumWidth: 0/);
});
test('RTL rich text is detected from paragraph rather than its HTML markup',()=>{
 const editor=read('ui/components/RichTextEditor.qml');
 assert.match(editor,/forceRightToLeft: localizationController\.isRightToLeftText\(/);
 assert.match(editor,/blockSurface\.parent\.blockData\.text/);
});
test('formatting and verbose hints stay optional on compact screens',()=>{
 const composer=read('ui/components/ComposerDrawer.qml');
 assert.match(composer,/property bool showAdvancedFormatting: false/);
 assert.match(composer,/objectName: "composerMobileFormattingToggle"/);
 assert.match(composer,/!root\.compact \|\| root\.showAdvancedFormatting/);
 assert.match(composer,/visible: !root\.compact \|\| presentationBridge\.writeErrorText\.length > 0/);
});
test('WASM Sign Out unconditionally transitions to Guest even for recovery-only logins',()=>{
 const main=readGenycasterMain();
 const section=main.split('QObject::connect(&bridge, &PresentationBridge::signOutRequested,')[1]?.split('#else')[0];
 assert.ok(section);
 assert.match(section,/browserSigner->forgetAccount\(\)/);
 assert.match(section,/browserAuth->signOut\(\)/);
 assert.match(section,/sessionObserver\(application::session::SessionState\{/);
 assert.match(section,/SessionPhase::Guest/);
});
test('Updates labels the beta channel and built-in locales translate it',()=>{
 const settings=read('ui/components/LocaleSettingsPopup.qml');
 assert.match(settings,/objectName: "settingsBetaReleaseLabel"/);
 assert.match(settings,/genycaster\.settings\.beta_release_short/);
 assert.match(settings,/genycaster\.settings\.beta_program_info/);
 const locales=['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR','hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
 for(const locale of locales){
  const ts=read('ui/i18n/qml_'+locale+'.ts');
  for(const key of ['genycaster.composer.formatting_tools','genycaster.settings.current_beta_release','genycaster.settings.beta_program_info'])
   assert.ok(ts.includes('id="'+key+'"'),locale+': missing '+key);
 }
});
