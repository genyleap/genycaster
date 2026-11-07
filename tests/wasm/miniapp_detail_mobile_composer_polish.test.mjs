import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = path => readFileSync(new URL('../../'+path, import.meta.url), 'utf8');

test('Mini App detail cover preserves the supplied artwork and its rounded corners', () => {
  const qml=read('ui/components/apps/AppsSurface.qml');
  const cover=qml.slice(qml.indexOf('id: detailCover'),qml.indexOf('id: detailCover')+1700);
  assert.match(cover,/detailCover\.sourceImplicitHeight \/ detailCover\.sourceImplicitWidth/);
  assert.match(cover,/fillMode: Image\.PreserveAspectFit/);
  assert.match(cover,/clipToSuperellipse: true/);
  assert.doesNotMatch(cover,/fillMode: Image\.PreserveAspectCrop/);
  assert.match(cover,/curveDepth: Theme\.curveContainer/);
});

test('Mini App screenshots preserve aspect and skip broken publisher assets', () => {
  const qml=read('ui/components/apps/AppsSurface.qml');
  const section=qml.slice(qml.indexOf('id: screenshotSection'));
  assert.match(section,/screenshotFailures < screenshotTotal/);
  assert.match(section,/model: root\.selectedMiniApp\.screenshots \|\| \[\]/);
  assert.match(section,/visible: !loadFailed/);
  assert.match(section,/fillMode: Image\.PreserveAspectFit/);
  assert.match(section,/clipToSuperellipse: true/);
  assert.match(section,/onLoadFailedChanged:/);
  assert.doesNotMatch(section,/visible: screenshotPreview\.loadFailed/);
  const cpp=read('ui/app/miniapp/catalog.cpp');
  assert.match(cpp,/screenshotArray = app\.value\(QStringLiteral\("screenshotUrls"\)\)/);
  assert.match(cpp,/screenshotArray = row\.value\(QStringLiteral\("screenshotUrls"\)\)/);
  assert.match(qml,/screenshotSection\.screenshotFailures = 0/);
});

test('mobile conversation actions are at the trailing edge and create action uses desktop compose symbol', () => {
  const detail=read('ui/components/feed/DetailSurface.qml');
  assert.match(detail,/objectName: "mobileConversationChannelHeader"/);
  assert.match(detail,/Item \{ Layout\.fillWidth: true; Layout\.minimumWidth: 0 \}/);
  assert.match(detail,/iconGlyph: Theme\.iconCompose/);
  const theme=read('ui/theme/Theme.qml');
  assert.match(theme,/mobileAction === "settings" \? iconSettings : iconCompose/);
  assert.match(theme,/property string colorPreset: "theme"/);
  const dock=read('ui/components/navigation/BottomNavigation.qml');
  assert.match(dock,/Layout\.preferredWidth: 52/);
  assert.match(dock,/highQualityEdges: true/);
  assert.match(dock,/elevation: Theme\.elevationPopover/);
  assert.match(dock,/Theme\.mobileActionIcon/);
});
