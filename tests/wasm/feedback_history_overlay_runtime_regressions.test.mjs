import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const source = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('private feedback history includes report text and notes from PostgreSQL', () => {
  const store = source('services/account-sync/pg_store.mjs');
  const dialog = source('ui/components/settings/FeedbackDialog.qml');
  const controller = source('ui/app/identity/connections.cpp');
  assert.match(store, /SELECT id,category,message,status,review_note,points_awarded,created_at,updated_at/);
  assert.match(store, /WHERE issuer=\$1 AND subject=\$2/);
  assert.match(dialog, /readonly property var feedbackReports: openProofConnections\s*\? \(openProofConnections\.feedbackReports \|\| \[\]\) : \[\]/);
  assert.match(dialog, /model: root\.feedbackReports/);
  assert.match(dialog, /String\(reportCard\.modelData\.message \|\| ""\)/);
  assert.match(dialog, /String\(reportCard\.modelData\.review_note \|\| ""\)/);
  assert.match(dialog, /Layout\.preferredHeight: reportBody\.implicitHeight/);
  assert.match(controller, /reply->bytesAvailable\(\) <= 512 \* 1024/);
});

test('submitted text is confirmed by the API before history refresh', () => {
  const store = source('services/account-sync/pg_store.mjs');
  const controller = source('ui/app/identity/connections.cpp');
  const dialog = source('ui/components/settings/FeedbackDialog.qml');
  assert.match(store, /RETURNING id,category,status,points_awarded,created_at,message/);
  assert.match(controller, /const QByteArray confirmationBytes = status == 201/);
  assert.match(controller, /report\.insert\(QStringLiteral\("message"\), body\)/);
  assert.match(controller, /m_feedbackReports\.prepend\(report\)/);
  assert.match(controller, /cancelNetworkReply\(m_feedbackRead, this\);\s*fetchFeedback\(\);/);
  assert.match(dialog, /objectName: "feedbackReportMessage"/);
  assert.match(dialog, /text: String\(reportCard\.modelData\.message \|\| ""\)\s+textFormat: Text\.PlainText/);
  assert.match(dialog, /wrapMode: Text\.WrapAtWordBoundaryOrAnywhere/);
});

test('recycled cast share menus guard missing window overlay', () => {
  const bar = source('ui/components/feed/EngagementBar.qml');
  assert.match(bar, /const overlay = Overlay\.overlay/);
  assert.match(bar, /if \(!overlay\) return/);
  assert.match(bar, /sourceComponent: Overlay\.overlay && Overlay\.overlay\.width > 0/);
});

test('market token limit notice no longer has a directly recursive visible binding', () => {
  const settings = source('ui/components/settings/SettingsPopup.qml');
  assert.match(settings, /Loader \{\s+Layout\.fillWidth: true\s+active: !root\.marketProEligible/);
  assert.doesNotMatch(settings, /DirectionAwareLabel \{\s+visible: !root\.marketProEligible\s+&& root\.enabledMarketCount/);
});

test('mobile blur uses an explicit texture-backed mask, not the desktop sidebar', () => {
  const dock = source('ui/components/navigation/BottomNavigation.qml');
  const side = source('ui/components/navigation/SideNavigation.qml');
  assert.match(dock, /id: capsuleMaskTexture/);
  assert.match(dock, /sourceItem: capsuleMask/);
  assert.match(dock, /maskSource: capsuleMaskTexture/);
  assert.match(dock, /source: capsuleBackdrop/);
  assert.match(side, /fillColor: Theme\.surfacePrimary/);
  assert.doesNotMatch(side, /desktopNavigationGlassBlur/);
});

test('embed previews only instantiate once a scrolling card is admitted', () => {
  const embeds = source('ui/components/media/CastEmbedsPanel.qml');
  assert.match(embeds, /active: tile\.admitted\s+\/\//);
  assert.match(embeds, /asynchronous: false/);
});
