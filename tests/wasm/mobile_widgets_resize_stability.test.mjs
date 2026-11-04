import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../../'+p,import.meta.url),'utf8');

test('mobile dock exposes a localized Widgets destination with safe six-slot widths',()=>{
 const dock=read('ui/components/BottomNavigation.qml');
 assert.match(dock,/signal widgetsRequested\(\)/);
 assert.match(dock,/property bool widgetsOpen: false/);
 assert.match(dock,/objectName: "mobileDockWidgets"/);
 assert.match(dock,/active: root\.widgetsOpen/);
 assert.match(dock,/caption: qsTrId\("genycaster\.context\.widgets"\)/);
 assert.match(dock,/onClicked: root\.widgetsRequested\(\)/);
 assert.match(dock,/root\.width < 360 \? 34 : 39/);
 assert.match(dock,/showSelectedLabel: width >= 450/);
 assert.equal((dock.match(/DockDestination \{/g)||[]).length,6);
});

test('the mobile Widgets bottom sheet reuses the desktop ContextRail and persistent widget order',()=>{
 const shell=read('ui/components/AppShell.qml');
 const rail=read('ui/components/ContextRail.qml');
 assert.match(shell,/widgetsOpen: contextDrawer\.visible/);
 assert.match(shell,/onWidgetsRequested: \{/);
 assert.match(shell,/objectName: "responsiveWidgetsDrawer"/);
 assert.match(shell,/bottomSheet: root\.mobile/);
 assert.match(shell,/height: parent \? parent\.height : 720/);
 assert.match(shell,/fullScreen: root\.mobile/);
 assert.match(shell,/interactive: !root\.mobile && root\.compactDesktop/);
 assert.match(shell,/onClicked: contextDrawer\.close\(\)/);
 assert.match(shell,/contentItem: ColumnLayout \{/);
 assert.match(shell,/ContextRail \{\s*Layout\.fillWidth: true\s*Layout\.fillHeight: true/);
 assert.match(rail,/model: liveInsights\.contextWidgetOrder/);
 assert.match(rail,/liveInsights\.moveContextWidget\(widgetDelegate\.widgetId, targetIndex\)/);
 assert.match(rail,/DragHandler \{/);
 assert.match(shell,/active: root\.desktop && root\.homeSurfaceActive/);
 assert.match(shell,/Layout\.preferredWidth: root\.showHomeContextRail \? root\.desktopContextRailWidth : 0/);
 assert.match(shell,/Layout\.preferredWidth: root\.showSocialContextRail \? root\.desktopContextRailWidth : 0/);
});

test('GENY widget mark displays the complete SVG rather than square image cropping',()=>{
 const rail=read('ui/components/ContextRail.qml');
 const mark=rail.slice(rail.indexOf('id: genyAssetMark'),rail.indexOf('id: genyAssetMark')+750);
 assert.match(mark,/anchors\.margins: Theme\.space1/);
 assert.match(mark,/fillMode: Image\.PreserveAspectFit/);
 assert.match(mark,/sourceSize\.width: 96/);
 assert.doesNotMatch(mark,/PreserveAspectCrop/);
 assert.match(rail,/Math\.max\(152, assetFrontContent\.implicitHeight/);
 assert.doesNotMatch(rail,/Behavior on Layout\.preferredHeight/);
});

test('resizing profile cover preserves image decode cache and stable account card geometry',()=>{
 const profile=read('ui/components/ProfileSurface.qml');
 const banner=profile.slice(profile.indexOf('id: bannerImage'),profile.indexOf('id: bannerImage')+2050);
 assert.match(banner,/requestedSourceWidth: 1200/);
 assert.match(banner,/requestedSourceHeight: 480/);
 assert.match(banner,/retainPreviousWhileLoading: true/);
 assert.match(banner,/framePacedPresentation: false/);
 assert.doesNotMatch(banner,/requestedSourceWidth: Math\.max/);
 const trust=read('ui/components/ProfileTrustCard.qml');
 assert.match(trust,/property real settledCompactFaceHeight: 168/);
 assert.match(trust,/interval: 180/);
 assert.match(trust,/onWidthChanged: compactHeightSettler\.restart\(\)/);
});

test('all locales include the short widget copy and navigation label',()=>{
 const locales=['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
  'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
 for(const locale of locales){
  const xml=read('ui/i18n/qml_'+locale+'.ts');
  for(const id of ['widgets','geny_membership_hint','geny_asset_explainer'])
   assert.equal((xml.match(new RegExp('id="genycaster\\.context\\.'+id+'"','g'))||[]).length,1,locale+':'+id);
  assert.match(xml,/<source>Verified Base wallets · Read-only, no signing\.<\/source>/);
  assert.match(xml,/<source>GENY powers Genycaster features\.<\/source>/);
 }
});
