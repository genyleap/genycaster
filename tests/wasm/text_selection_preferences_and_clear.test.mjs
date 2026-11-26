import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = f => readFileSync(new URL('../../'+f,import.meta.url),'utf8');

test('selection mode is persisted per device and uses Qt cast selection on WASM',()=>{
  const cpp=read('ui/app/settings/selection.cpp');
  const header=read('ui/app/settings/selection.hpp');
  const main=read('ui/app/main.cpp');
  const settings=read('ui/components/settings/SettingsPopup.qml');
  assert.match(header,/Q_PROPERTY\(QString mode READ mode NOTIFY modeChanged/);
  assert.match(header,/Q_PROPERTY\(bool nativeAvailable READ nativeAvailable CONSTANT/);
  assert.match(cpp,/accessibility\/platformTextSelectionMode/);
  assert.match(cpp,/nativeAvailable\(\) const noexcept[\s\S]*?#if defined\(Q_OS_WASM\)\s*return false;\s*#else\s*return true;/);
  assert.match(cpp,/if \(requested == QStringLiteral\("system"\) && !nativeAvailable\(\)\) return;/);
  assert.match(main,/\{"textSelectionSettings", &textSelectionSettings\}/);
  assert.match(settings,/objectName: "textSelectionPreference"/);
  assert.match(settings,/textSelectionSettings\.setMode\("genycaster"\)/);
  assert.match(settings,/textSelectionSettings\.setMode\("system"\)/);
  assert.match(settings,/enabled: textSelectionSettings.nativeAvailable/);
});
test('editing does not depend on read-only text selection preferences',()=>{
 const input=read('ui/components/controls/BidiTextArea.qml');
 const menu=read('ui/components/composer/EditorContextMenu.qml');
 assert.doesNotMatch(input,/textSelectionSettings|composerTextSelectionHandles/);
 assert.doesNotMatch(input,/onLongPressed:|onTapped:/);
 assert.match(menu,/presentationBridge\.openBrowserTextEditor/);
});

test('cast selection exposes real document caret rectangles and draggable blue handles',()=>{
  const cast=read('ui/components/content/RichInlineText.qml');
  const cpp=read('src/presentation/qt/rich_inline_text_item.cpp');
  const h=read('src/presentation/qt/rich_inline_text_item.hpp');
  assert.match(h,/Q_PROPERTY\(QRectF selectionStartRect/);
  assert.match(h,/Q_PROPERTY\(QRectF selectionEndRect/);
  assert.match(h,/Q_INVOKABLE void dragSelectionBoundary/);
  assert.match(cpp,/document\.findBlock\(clamped\)/);
  assert.match(cpp,/line\.cursorToX\(local\)/);
  assert.match(cast,/objectName: "castTextSelectionHandles"/);
  assert.match(cast,/textLayout\.dragSelectionBoundary\(grip.leading, where.x, where.y\)/);
  assert.match(cast,/Theme\.nativeSelectionAccent/);
  assert.match(cast,/id: castSelectionGripLoader/);
  assert.match(cast,/parent: Overlay.overlay/);
});
test('published cast selection preference is independent of native editing',()=>{
 const cast=read('ui/components/content/RichInlineText.qml');
 const input=read('ui/components/controls/BidiTextArea.qml');
 const menu=read('ui/components/composer/EditorContextMenu.qml');
 assert.match(cast,/objectName: "platformCastSelectableText"/);
 assert.match(cast,/readOnly: true/);
 assert.match(cast,/onLinkActivated: url => root.externalRequested\(url\)/);
 assert.doesNotMatch(input,/ContextMenu\.menu:/);
 assert.doesNotMatch(menu,/popupType:|MenuItem \{/);
 assert.match(menu,/function mountBrowserEditor\(\)/);
 assert.doesNotMatch(menu,/root\.editor\.copy\(\)/);
 assert.doesNotMatch(input,/textSelectionSettings/);
});

test('Clear content destroys unsent content and media safely after confirmation',()=>{
  const composer=read('ui/components/composer/ComposerDrawer.qml');
  const engine=read('src/presentation/qt/rich_composer_controller.cpp');
  assert.match(composer,/function clearAllComposerContent\(\)/);
  assert.match(composer,/if \(presentationBridge.writeBusy \|\| presentationBridge.writePhase === "queued"\)/);
  assert.match(composer,/richComposer.clearCurrentContent\(/);
  assert.match(composer,/threadTailModel\.clear\(\)/);
  assert.match(composer,/mediaUpload.cancel\(\)/);
  assert.match(composer,/objectName: "composerClearContentMobile"/);
  assert.match(composer,/objectName: "composerClearContentDesktop"/);
  assert.match(composer,/objectName: "composerClearContentConfirmation"/);
  assert.match(engine,/void RichComposerController::newDocument\(\)[\s\S]*?clearDraft\(\)/);
  assert.doesNotMatch(composer.slice(composer.indexOf('function clearAllComposerContent'), composer.indexOf('function startNextMediaUpload')), /clearReplyTarget/);
});
test('settings and clear content strings available in all supported languages',()=>{
  for(const locale of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR','hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']){
    const xml=read('ui/i18n/qml_'+locale+'.ts');
    for(const id of ['genycaster.settings.text_selection','genycaster.settings.selection_system','genycaster.settings.selection_browser_limit','genycaster.composer.clear_content','genycaster.composer.clear_all_confirmation'])
      assert.ok(xml.includes('id="'+id+'"'),locale+': '+id);
  }
});
