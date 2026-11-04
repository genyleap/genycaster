import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=name=>readFileSync(new URL('../../'+name,import.meta.url),'utf8');

test('in-app Cast/Profile navigation retains bounded typed breadcrumb frames',()=>{
 const h=read('src/presentation/qt/presentation_bridge.hpp');
 const cpp=read('src/presentation/qt/presentation_bridge.cpp');
 assert.match(h,/struct DetailLocation \{/);
 assert.match(h,/QString castId\{\}/);
 assert.match(h,/qulonglong profileFid\{\}/);
 assert.match(h,/QString profileUsername\{\}/);
 assert.match(h,/QString profileFeed\{\}/);
 assert.match(h,/std::vector<DetailLocation> m_detailHistory\{\}/);
 assert.match(cpp,/void PresentationBridge::rememberDetailLocation\(\)/);
 assert.match(cpp,/constexpr std::size_t maxBreadcrumbs = 24/);
 assert.match(cpp,/m_detailHistory\.erase\(m_detailHistory\.begin\(\)\)/);
 assert.match(cpp,/m_detailHistory\.push_back\(std::move\(location\)\)/);
 assert.match(cpp,/m_currentCastId = canonical/);
 assert.match(cpp,/rememberDetailLocation\(\)/);
});

test('Back unwinds exact quoted casts and profiles and never records a replay',()=>{
 const h=read('src/presentation/qt/presentation_bridge.hpp');
 const cpp=read('src/presentation/qt/presentation_bridge.cpp');
 const back=cpp.split('void PresentationBridge::returnFromDetail()')[1].split('void PresentationBridge::loadMore()')[0];
 assert.match(h,/Q_INVOKABLE void clearDetailHistory\(\)/);
 assert.match(back,/while \(!m_detailHistory\.empty\(\)\)/);
 assert.match(back,/m_detailHistory\.pop_back\(\)/);
 assert.match(back,/m_restoringDetailHistory = true/);
 assert.match(back,/openCast\(previous\.castId\)/);
 assert.match(back,/openProfileFid\(previous\.profileFid\)/);
 assert.match(back,/openProfileUsername\(previous\.profileUsername\)/);
 assert.match(back,/m_profileFeed = previous\.profileFeed/);
 assert.match(back,/m_restoringDetailHistory = false/);
 assert.match(back,/showFeed\(\)/);
 assert.doesNotMatch(back,/m_detailReturnView ==/);
 const shell=read('ui/components/AppShell.qml');
 assert.match(shell,/function returnFromProfileSurface\(\) \{\s*presentationBridge\.returnFromDetail\(\)/);
 assert.match(shell,/function returnToPendingShellRoute\(\) \{[\s\S]*?presentationBridge\.activeView !== "feed"/);
 assert.match(shell,/onShellRouteChanged: \{\s*if \(root\.shellRoute\.length > 0\)\s*presentationBridge\.clearDetailHistory\(\)/);
 assert.match(shell,/function openMyProfileFromNavigation\(\) \{\s*presentationBridge\.clearDetailHistory\(\)/);
 assert.match(cpp,/m_activeFeed = nextFeed/);
 assert.match(cpp,/clearDetailHistory\(\);\s*m_activeView = QStringLiteral\("feed"\)/);
});

test('Explorer explicit entry scrolls the inline StoryStrip from every previous destination',()=>{
 const shell=read('ui/components/AppShell.qml');
 const feed=read('ui/components/FeedSurface.qml');
 const nav=shell.split('function openExplorer()')[1].split('function openTrends()')[0];
 assert.match(nav,/\+\+root\.explorerTopRevision/);
 assert.match(nav,/presentationBridge\.refreshFollowingStories\(\)/);
 assert.match(nav,/selectExplorerPresentationMode\(root\.explorerPresentationMode, false\)/);
 assert.doesNotMatch(nav,/if \(wasExplorer\) Qt\.callLater/);
 assert.match(shell,/scrollRequestRevision: root\.primaryDestination === "explorer"/);
 assert.match(feed,/property int appliedScrollRequestRevision: 0/);
 assert.match(feed,/function honorPrimaryScrollRequest\(\)/);
 assert.match(feed,/onScrollRequestRevisionChanged: Qt\.callLater\(root\.honorPrimaryScrollRequest\)/);
 assert.match(feed,/root\.scrollToBeginning\(\)/);
 assert.match(feed,/readList\.positionViewAtBeginning\(\)/);
 assert.match(feed,/headerPositioning: root\.compact \|\| root\.activeFeed === "channel"\s*\|\| stableRows\.count === 0\s*\? ListView\.InlineHeader/);
});

test('short mobile sheets center titles beneath their grabber without duplicate close glyphs',()=>{
 const header=read('ui/components/ModalHeader.qml');
 const share=read('ui/components/EngagementBar.qml');
 const create=read('ui/components/MobileCreateMenu.qml');
 const app=read('ui/components/AppsSurface.qml');
 const pro=read('ui/components/GenyProDetailsDrawer.qml');
 assert.match(header,/property bool sheetPresentation: false/);
 assert.match(header,/root\.sheetPresentation\s*\? Text\.AlignHCenter/);
 assert.match(header,/visible: root\.showClose && !root\.sheetPresentation/);
 assert.match(share,/objectName: "castShareMobileDrawer"[\s\S]*?showHandle: true[\s\S]*?interactive: true/);
 assert.match(share,/sheetPresentation: true/);
 assert.match(share,/contentPadding: Theme\.space2/);
 assert.match(create,/floatingSheet: true\s*interactive: true/);
 assert.match(create,/sheetPresentation: true/);
 assert.match(app,/sheetPresentation: miniAppDetailDrawer\.mobilePresentation/);
 assert.match(pro,/horizontalAlignment: root\.compact \? Text\.AlignHCenter/);
 assert.match(pro,/visible: !root\.compact[\s\S]*?iconGlyph: Theme\.iconClose/);
});
