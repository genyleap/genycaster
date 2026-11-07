import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../'+path, import.meta.url), 'utf8');
const qml = path => read('ui/components/'+path+'.qml');

test('phone form dialogs are genuinely full screen with no bogus drag handle', () => {
  const adaptive = qml('GenyAdaptiveDialog');
  const shell = qml('AppShell');
  assert.match(adaptive, /property bool mobileFullScreen: false/);
  assert.match(shell, /GenyAdaptiveDialog \{[\s\S]*?id: signOutConfirm/);
  assert.match(qml('RenderingSettingsPopup'), /mobileFullScreen: true/);
  assert.match(qml('AccentColorStudio'), /mobileFullScreen: true/);
  assert.match(adaptive, /fullScreen: control.mobileFullScreen/);
  assert.match(adaptive, /interactive: !control.mobileFullScreen/);
  assert.match(adaptive, /showHandle: !control.mobileFullScreen && control.showHandle/);
  assert.match(adaptive, /height: control.mobileFullScreen[\s\S]*?parent\.height/);
  assert.match(qml('FeedbackDialog'), /mobileFullScreen: true/);
});

test('mobile feedback action sits by bell and opens real full-screen form', () => {
  const header = qml('AppHeader');
  const shell = qml('AppShell');
  assert.match(header, /signal feedbackRequested\(\)/);
  assert.match(header, /visible: root\.compact[\s\S]*?iconGlyph: Theme\.iconFeedback[\s\S]*?onClicked: root\.feedbackRequested\(\)/);
  assert.match(shell, /onFeedbackRequested: feedbackDialog\.open\(\)/);
  assert.match(qml('FeedbackDialog'), /objectName: "feedbackFixedActions"/);
  assert.match(qml('FeedbackDialog'), /objectName: "feedbackFormScroll"/);
});

test('feedback form performs authenticated server submission and tracks real status', () => {
  const ui = qml('FeedbackDialog');
  const controller = read('ui/app/openproof/connections.cpp');
  const header = read('ui/app/openproof/connections.hpp');
  assert.match(ui, /openProofConnections\.submitFeedback\(root\.category, root\.sentiment/);
  assert.match(ui, /openProofConnections\.fetchFeedback\(\)/);
  assert.match(ui, /readonly property var feedbackStats: openProofConnections/);
  assert.match(ui, /root\.feedbackStats\.accepted/);
  assert.match(ui, /root\.feedbackStats\.resolved/);
  assert.doesNotMatch(ui, /backend_pending|copyText\(/);
  assert.match(header, /Q_INVOKABLE void submitFeedback/);
  assert.match(controller, /m_network\.post\(request, QJsonDocument\{payload\}/);
  assert.match(controller, /m_feedbackSubmitted = status == 201/);
  assert.match(controller, /if \(viewer != m_viewerFid \|\| generation != m_grantGeneration\) return;/);
  assert.match(controller, /reply->isReadable\(\)/);
});

test('feedback choices consume pointer input and the entry point uses a face icon', () => {
  const dialog = qml('FeedbackDialog');
  const theme = read('ui/theme/Theme.qml');
  const header = qml('AppHeader');
  assert.ok(theme.includes('iconFeedback: "social/smile"'));
  assert.ok(header.includes('iconGlyph: Theme.iconFeedback'));
  assert.equal((dialog.match(/delegate: Button \{/g) || []).length, 3);
  assert.doesNotMatch(dialog, /TapHandler \{/);
  assert.match(dialog, /onClicked: root\.sentiment = moodButton\.modelData\.value/);
  assert.match(dialog, /onClicked: root\.category = categoryCard\.modelData\.id/);
});

test('desktop bookmark cards keep a consistent inset on every edge', () => {
  const bookmarks = qml('BookmarksSurface');
  // A clipped card keeps its outer grid geometry separate from its inner
  // padding; layout margins are zero, and children are inset on all sides.
  assert.match(bookmarks, /implicitHeight: Math\.max\(110,\s*savedContent\.implicitHeight \+ Theme\.space3 \* 2\)/);
  assert.match(bookmarks, /Layout\.margins: 0/);
  assert.match(bookmarks, /anchors\.margins: Theme\.space3/);
});

test('feedback text is translated in every supported locale', () => {
  const dialog = qml('FeedbackDialog');
  const ids = [...new Set([...dialog.matchAll(/qsTrId\("([^"]+)"\)/g)].map(m => m[1]))];
  for (const locale of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
                        'pt_BR','ru_RU','hi_IN','ja_JP','ko_KR','zh_CN']) {
    const ts = read('ui/i18n/qml_'+locale+'.ts');
    for (const id of ids) assert.ok(ts.includes('message id="'+id+'"'), locale+' missing '+id);
  }
  const catalog = read('ui/i18n/catalog.cpp');
  for (const id of ids) assert.ok(catalog.includes('qtTrId("'+id+'")'), 'missing catalog ID: '+id);
});
