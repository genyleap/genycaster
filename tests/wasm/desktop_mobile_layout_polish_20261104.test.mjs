import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=name=>readFileSync(new URL('../../ui/components/'+name+'.qml',import.meta.url),'utf8');

test('cast toolbar keeps New cast only on desktop, trailing actions at all widths',()=>{
 const detail=read('DetailSurface');
 const toolbar=detail.slice(detail.indexOf('// Contextual conversation toolbar.'),
                          detail.indexOf('// On phones, channel identity'));
 assert.match(toolbar,/Layout\.fillWidth: false/);
 assert.match(toolbar,/Item \{ Layout\.fillWidth: true; Layout\.minimumWidth: 0 \}/);
 assert.match(toolbar,/visible: root\.activeView === "cast" && !root\.compact/);
 assert.match(toolbar,/text: qsTrId\("genycaster\.header\.new_cast"\)/);
 assert.match(toolbar,/iconGlyph: Theme\.iconRefresh/);
 assert.match(toolbar,/onClicked: root\.refreshRequested\(\)/);
});

test('Pro drawer uses neutral surfaces and retains fixed header, footer and interactive feature details',()=>{
 const pro=read('GenyProDetailsDrawer');
 assert.match(pro,/objectName: "genyProNeutralEligibilitySummary"/);
 assert.match(pro,/fillColor: Theme\.surfaceRaised/);
 assert.match(pro,/borderWidth: Theme\.strokeHairline/);
 assert.match(pro,/fillColor: feature\.expanded \? Theme\.surfaceMuted/);
 assert.match(pro,/fillColor: Theme\.surfaceMuted/);
 assert.match(pro,/color: Theme\.inkMuted/);
 assert.doesNotMatch(pro,/fillColor: Theme\.proSurface/);
 assert.doesNotMatch(pro,/borderColor: Theme\.proBorder/);
 assert.match(pro,/id: fixedHeader/);
 assert.match(pro,/id: fixedActions/);
 assert.match(pro,/id: detailsViewport/);
 assert.match(pro,/onClicked: root\.expandedFeature = expanded \? -1 : index/);
 assert.match(pro,/title: qsTrId\("genycaster\.pro\.feature_miniapps_title"\)/);
 assert.match(pro,/detail: qsTrId\("genycaster\.pro\.feature_miniapps_desc"\)/);
});

test('bookmark grid uses dense desktop columns, top aligned cards and single-column mobile',()=>{
 const bookmarks=read('BookmarksSurface');
 assert.match(bookmarks,/readonly property int columnCount: root\.compact \|\| gridAvailableWidth < 660/);
 assert.match(bookmarks,/gridAvailableWidth < 1680 \? 2 : 3/);
 assert.match(bookmarks,/columns: columnCount/);
 assert.match(bookmarks,/Layout\.alignment: Qt\.AlignTop \| Qt\.AlignLeft/);
 assert.match(bookmarks,/Layout\.maximumWidth: savedCastsGrid\.cardWidth/);
 assert.match(bookmarks,/compact: root\.compact \|\| savedCastsGrid\.columns >= 3/);
 assert.match(bookmarks,/CastItem \{/);
});

test('desktop profile card keeps compact Pro/Profile actions without redundant account cog',()=>{
 const card=read('ProfileTrustCard');
 const nav=read('SideNavigation');
 assert.match(card,/faceHeight: root\.compact/);
 assert.match(card,/settledCompactFaceHeight/);
 assert.match(card,/Math\.max\(frontContent\.implicitHeight, backContent\.implicitHeight, 200\)/);
 assert.match(card,/maximumLineCount: root\.compact \? 1 : 3/);
 const beforeBack=card.slice(0,card.indexOf('id: backFace'));
 assert.match(beforeBack,/onClicked: root\.proRequested\(\)/);
 assert.doesNotMatch(beforeBack,/profileTrustCardAccountButton|iconGlyph: Theme\.iconSettings/);
 assert.match(beforeBack,/onClicked: root\.requestProfile\(\)/);
 assert.match(card,/root\.streakDetailsExpanded\s*\? backContent\.implicitHeight : frontContent\.implicitHeight/);
 assert.match(nav,/ProfileTrustCard \{/);
});

test('status source badge follows Live without reserved 112px gap',()=>{
 const status=read('StatusFooter');
 assert.match(status,/id: liveStatusLabel/);
 assert.match(status,/Math\.ceil\(liveStatusMetrics\.advanceWidth\) \+ 7 \+ Theme\.space2/);
 assert.match(status,/id: providerLabel/);
 assert.doesNotMatch(status,/root\.iconOnlyMetadata \? 12 : 112/);
 assert.match(status,/text: nodeProviderLabel/);
});
