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
  assert.match(qml,/active: root.selectionReady && textLayout.selectedText.length > 0/);
  assert.match(qml,/parent: Overlay.overlay/);
  assert.match(qml,/textLayout.selectionMenuPosition.x/);
  assert.match(qml,/textLayout.selectionMenuPosition.y/);
  assert.match(qml,/closePolicy: Popup.CloseOnPressOutside/);
  assert.match(qml,/presentationBridge.copyText\(textLayout.selectedText\)/);
  assert.doesNotMatch(qml,/id: selectGesture/);
  assert.doesNotMatch(qml,/anchors.right: parent.right[\s\S]*?id: selectionCopyAction/);
});
