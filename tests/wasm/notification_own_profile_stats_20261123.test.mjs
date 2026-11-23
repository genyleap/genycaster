import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=p=>readFileSync(p,'utf8');
const shell=read('ui/components/shell/AppShell.qml');
const profile=read('ui/components/profile/ProfileSurface.qml');
const cpp=read('src/presentation/qt/presentation_bridge.cpp');
const header=read('src/presentation/qt/presentation_bridge.hpp');
const stats=read('ui/components/home/StatsPanel.qml');

const start=shell.indexOf('    function openProfileFromContent(fid) {');
const end=shell.indexOf('\n    property bool profileEditorRequestedFromSettings:',start);
assert.ok(start>0 && end>start);
const body=shell.slice(start,end).trim()
 .replace(/^function openProfileFromContent\(fid\)\s*\{/,'').replace(/\}\s*$/,'');
const invoke=new Function('fid','root','providerProfiles','presentationBridge',body);

function harness({readContract='snapchain-v1',route=''}={}){
  const calls=[];
  const root={
    shellRoute:route,pendingReturnShellRoute:'',
    rememberShellRouteForReturn(){calls.push('remember-route');this.pendingReturnShellRoute=this.shellRoute;}
  };
  const bridge={
    clearDetailHistory(){calls.push('clear-history');},
    openContentProfileFid(fid,contract,fromNotifications){calls.push(['context',fid,contract,fromNotifications]);}
  };
  return {calls,root,open:fid=>invoke(fid,root,{activeReadContract:readContract},bridge)};
}

test('QML uses the C++ protocol/custody routing API for all content profile links',()=>{
  const a=harness();
  a.open(42);
  assert.deepEqual(a.calls,[['context',42,'snapchain-v1',false]]);
  const b=harness({readContract:'hypersnap-v2'});
  b.open(42);
  assert.deepEqual(b.calls,[['context',42,'hypersnap-v2',false]]);
  const invalid=harness();
  invalid.open(Number.NaN);
  assert.equal(invalid.calls.length,0);
  assert.match(header,/Q_INVOKABLE void openContentProfileFid\(qulonglong fid, const QString& readContract,/);
  assert.match(cpp,/void PresentationBridge::openContentProfileFid\(/);
  assert.match(cpp,/const bool sameProtocol = nativeCustodyAccount\(\)/);
  assert.match(cpp,/readContract == QStringLiteral\("neynar-v2"\)/);
  assert.match(cpp,/sameProtocol \|\| fromOwnNotifications/);
  assert.match(cpp,/fid == m_accountFid[\s\S]*?sameProtocol \|\| fromOwnNotifications\)/);
  assert.match(cpp,/openMyProfile\(\);[\s\S]*?return;[\s\S]*?openProfileFid\(fid\)/);
  assert.match(profile,/root\.ownProfileNavigation\s*\|\| root\.activeReadNetwork === root\.accountNetwork/);
  assert.match(profile,/visible: Number\(root\.activeDetail\.fid \|\| 0\) > 0 && !root\.isOwnProfile/);
});

test('a Notification -> Cast -> own Profile navigation preserves the origin route',()=>{
  const a=harness({route:'notifications'});
  a.open(42);
  assert.equal(a.root.pendingReturnShellRoute,'notifications');
  assert.equal(a.root.shellRoute,'');
  assert.deepEqual(a.calls,['remember-route','clear-history',['context',42,'snapchain-v1',true]]);
  const detailStart=shell.indexOf('id: castSurfaceLoader');
  const detail=shell.slice(detailStart,detailStart+9000);
  assert.match(detail,/onProfileRequested: fid => \{[\s\S]*?root\.openProfileFromContent\(fid\)/);
  const own=cpp.split('void PresentationBridge::openMyProfile()')[1]
    .split('void PresentationBridge::openProfileUsername(')[0];
  assert.match(own,/rememberDetailLocation\(\)/);
  const remember=cpp.split('void PresentationBridge::rememberDetailLocation()')[1]
    .split('void PresentationBridge::clearDetailHistory()')[0];
  assert.match(remember,/location\.view = QStringLiteral\("cast"\)/);
  assert.match(remember,/location\.castId = m_currentCastId/);
  const back=cpp.split('void PresentationBridge::returnFromDetail()')[1]
    .split('void PresentationBridge::loadMore()')[0];
  assert.match(back,/openCast\(previous\.castId\)/);
  assert.match(shell,/function returnFromProfileSurface\(\) \{\s*presentationBridge\.returnFromDetail\(\)\s*root\.returnToPendingShellRoute\(\)/);
  assert.match(shell,/function returnToPendingShellRoute\(\)[\s\S]*?presentationBridge\.activeView !== "feed"/);
});

test('@username links also delegate account ownership and protocol matching to C++',()=>{
  const link=shell.slice(shell.indexOf('function openCastTextLink(url)'),
      shell.indexOf('function openSuggestedProfile('));
  assert.match(link,/root\.openProfileFromContent\(Number\(target\.substring\(5\)\)\)/);
  assert.match(link,/presentationBridge\.openContentProfileUsername\(/);
  assert.match(header,/Q_INVOKABLE void openContentProfileUsername\(/);
  const method=cpp.split('void PresentationBridge::openContentProfileUsername(')[1]
      .split('void PresentationBridge::selectProfileFeed(')[0];
  assert.match(method,/readContract == QStringLiteral\("snapchain-v1"\)/);
  assert.match(method,/target\.compare\(m_accountUsername, Qt::CaseInsensitive\) == 0/);
  assert.match(method,/openMyProfile\(\)/);
  assert.match(method,/openProfileUsername\(target\)/);
});

test('Stats share modal never dereferences destroyed scroll objects',()=>{
  assert.match(stats,/contentHeight: shareModalBody \? shareModalBody\.implicitHeight : 0/);
  assert.match(stats,/width: parent \? parent\.width : \(shareModalScroll \? shareModalScroll\.width : 0\)/);
  assert.doesNotMatch(stats,/width: shareModalScroll\.width/);
});
