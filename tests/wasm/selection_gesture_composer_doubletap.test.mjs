import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('double tap selects entire cast composer field, reply editor and rich block editor', () => {
  const input=read('ui/components/DirectionAwareTextArea.qml');
  const composer=read('ui/components/ComposerDrawer.qml');
  const rich=read('ui/components/RichTextEditor.qml');
  assert.match(input,/property bool selectAllOnDoubleTap: false/);
  assert.match(input,/TapHandler \{[\s\S]*?onDoubleTapped: \{[\s\S]*?root.selectAll\(\)/);
  for (const id of ['mobilePlainEditor','threadEditor','markdownEditor'])
    assert.match(composer, new RegExp('id: '+id+'\\s+selectAllOnDoubleTap: true'));
  assert.match(rich,/selectAllOnDoubleTap: true/);
  assert.match(input,/root.showSelectionActions\(\)/);
});

test('native cast selection reserves mouse and touch gestures from Flickable only after selection', () => {
  const cpp=read('src/presentation/qt/rich_inline_text_item.cpp');
  const header=read('src/presentation/qt/rich_inline_text_item.hpp');
  assert.match(cpp,/setAcceptTouchEvents\(true\)/);
  assert.match(cpp,/m_pressHoldTimer.setInterval\(460\)/);
  assert.match(cpp,/setKeepMouseGrab\(true\)/);
  assert.match(cpp,/setKeepTouchGrab\(true\)/);
  assert.match(cpp,/setKeepMouseGrab\(false\)/);
  assert.match(cpp,/setKeepTouchGrab\(false\)/);
  assert.match(cpp,/void RichInlineTextItem::touchEvent\(QTouchEvent\* event\)/);
  assert.match(cpp,/case QEvent::TouchUpdate:/);
  assert.match(cpp,/extendSelectionGesture\(position, true\)/);
  assert.match(cpp,/if \(!m_longPressSelected && touch\) return;/);
  assert.match(cpp,/if \(m_selectedWordStart >= 0 && m_selectedWordEnd >= 0\)/);
  assert.match(header,/Q_PROPERTY\(QPointF selectionMenuPosition/);
});

test('Copy popup is lazy and anchored to selected text in the window overlay, not card top-right', () => {
  const qml=read('ui/components/RichInlineText.qml');
  assert.match(qml,/Loader \{[\s\S]*?id: selectionMenuLoader/);
  assert.match(qml,/active: !root.usePlatformSelection && root.selectionReady/);
  assert.match(qml,/textLayout.selectedText.length > 0/);
  assert.match(qml,/parent: Overlay.overlay/);
  assert.match(qml,/textLayout.selectionMenuPosition.x/);
  assert.match(qml,/textLayout.selectionMenuPosition.y/);
  assert.match(qml,/closePolicy: Popup.CloseOnPressOutside/);
  assert.match(qml,/presentationBridge.copyText\(textLayout.selectedText\)/);
  assert.doesNotMatch(qml,/id: selectGesture/);
  assert.doesNotMatch(qml,/anchors.right: parent.right[\s\S]*?id: selectionCopyAction/);
});

test('composer preserves mouse selection and offers touch drag grips across platforms', () => {
  const input=read('ui/components/DirectionAwareTextArea.qml');
  const composer=read('ui/components/ComposerDrawer.qml');
  assert.match(input,/selectByMouse: true/);
  assert.match(input,/persistentSelection: true/);
  assert.match(input,/onPressAndHold: mouse =>/);
  assert.match(input,/onReleased: mouse =>/);
  assert.match(input,/id: selectionGrips/);
  assert.match(input,/objectName: "composerTextSelectionHandles"/);
  assert.match(input,/root\.positionToRectangle\(root\.selectionStart\)/);
  assert.match(input,/root\.positionToRectangle\(root\.selectionEnd\)/);
  assert.match(input,/width: 44\s+height: 44/);
  assert.match(input,/preventStealing: true/);
  assert.match(input,/root\.positionAt\([\s\S]*?point\.x - pressDeltaX, point\.y - pressDeltaY\)/);
  assert.match(input,/root\.select\(bounded, oppositeEdge\)/);
  assert.match(input,/parent: Overlay\.overlay/);
  assert.match(input,/root\.mapToItem\(selectionGrips/);
  assert.match(input,/!root\.inputMethodComposing/);
  assert.match(input,/selectionPressActive/);
  for (const id of ['mobilePlainEditor','threadEditor','markdownEditor'])
    assert.match(composer, new RegExp('id: '+id+'\\s+selectAllOnDoubleTap: true'));
  assert.doesNotMatch(input,/Qt\.platform\.os\s*===/);
});

test('cast text selection offers Copy, Select all and clear without deleting content', () => {
  const qml=read('ui/components/RichInlineText.qml');
  const editor=read('ui/components/DirectionAwareTextArea.qml');
  const desktop=read('ui/components/EditorContextMenu.qml');
  assert.match(qml,/objectName: "castSelectionCopy"/);
  assert.match(qml,/objectName: "castSelectionAll"/);
  assert.match(qml,/objectName: "castSelectionReset"/);
  assert.match(qml,/textLayout\.selectAllText\(\)/);
  assert.match(qml,/textLayout\.clearSelection\(\)/);
  assert.match(qml,/presentationBridge\.copyText\(textLayout\.selectedText\)/);
  assert.match(qml,/focusPolicy: Qt\.NoFocus/);
  assert.match(editor,/root\.deselect\(\)/);
  assert.match(editor,/text: qsTrId\("genycaster\.editor\.clear_selection"\)/);
  assert.match(desktop,/onTriggered: root\.editor\.deselect\(\)/);
  assert.match(desktop,/presentationBridge\.copyText\(root\.editor\.selectedText\)/);
  assert.doesNotMatch(qml,/textLayout\.clear\(\)/);
});

test('Clear selection is localized for all included languages',()=>{
  const locales=['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR','hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
  for (const locale of locales){
    const file=read('ui/i18n/qml_'+locale+'.ts');
    assert.match(file,/id="genycaster.editor.clear_selection"/);
    assert.match(file,/id="genycaster.editor.select_all"/);
  }
});
