import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root=new URL('../../',import.meta.url);
const qml=readFileSync(new URL('ui/components/UpdateNoticeDrawer.qml',root),'utf8');
const js=readFileSync(new URL('ui/wasm/genycaster-loader.js',root),'utf8');
const html=readFileSync(new URL('ui/wasm/genycaster-shell.html',root),'utf8');

test('Qt update drawer identifies consecutive WASM builds sharing the same semver',()=>{
  assert.match(qml,/readonly property bool sameSemanticRelease/);
  assert.match(qml,/webCurrentBuild\.slice\(0, 8\)/);
  assert.match(qml,/webLatestBuild/);
  assert.match(qml,/releaseKey = Qt\.platform\.os === "wasm"/);
  assert.match(qml,/lastPresentedVersion = releaseKey/);
  assert.match(qml,/genycaster\.update\.build_label/);
});

test('Qt update drawer anchors fixed header and actions outside the notes scroller',()=>{
  const header=qml.indexOf('id: updateFixedChrome');
  const notes=qml.indexOf('id: updateScrollableSection');
  const footer=qml.indexOf('id: updateNoticeStickyActions');
  assert.ok(header>0 && header<notes && notes<footer);
  assert.match(qml,/objectName: "updateNoticeStickyHeader"/);
  assert.match(qml,/anchors\.top: updateFixedChrome\.bottom/);
  assert.match(qml,/anchors\.bottom: updateNoticeStickyActions\.top/);
  assert.match(qml,/anchors\.bottom: parent\.bottom/);
  assert.match(qml,/objectName: "updateReleaseNotesScroll"/);
  assert.match(qml,/interactive: contentHeight > height/);
});

test('HTML fallback follows the same contract for any domain or build number',()=>{
  assert.match(js,/const sameAppVersion=!!applicationVersion && newAppVersion===applicationVersion/);
  assert.match(js,/primaryOld=sameAppVersion && currentBuild/);
  assert.match(js,/primaryNew=sameAppVersion && nextBuild/);
  assert.match(js,/new URL\("\.\/genycaster-assets\.json",location\.href\)/);
  assert.match(html,/\.update-scroll-body\{[^}]*overflow-y:auto/);
  assert.match(html,/\.update-actions\{flex:0 0 auto/);
});
