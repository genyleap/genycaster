import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const source = file => readFileSync(new URL('../../' + file, import.meta.url), 'utf8');

test('mobile cast share is a lazy bottom drawer, desktop remains a positioned popover', () => {
  const qml = source('ui/components/EngagementBar.qml');
  assert.match(qml, /id: castShareMenu\s+active: false/);
  assert.match(qml, /sourceComponent: Overlay\.overlay\.width < Theme\.breakpointMobile\s*\? mobileCastShareSheet : desktopCastSharePopup/);
  assert.match(qml, /if \(Overlay\.overlay\.width >= Theme\.breakpointMobile\) \{\s*const point = castShareButton\.mapToItem/);
  assert.match(qml, /id: mobileCastShareSheet\s+GenyDrawer \{/);
  assert.match(qml, /objectName: "castShareMobileDrawer"/);
  assert.match(qml, /bottomSheet: true\s+showHandle: true\s+dimBackground: true/);
  assert.match(qml, /id: desktopCastSharePopup\s+Popup \{/);
  assert.match(qml, /sourceComponent: castShareActionList/g);
  for (const id of ['share_action','copy_link','copy_farcaster_link','open_quorum','open_farcaster']) {
    assert.match(qml, new RegExp('genycaster\\.share\\.' + id));
  }
  assert.match(qml, /function closeShareMenu\(\)/);
  assert.doesNotMatch(qml, /sharePopup\.close\(\)/);
});

test('all GenyDrawer sheets have coordinated eased movement with reduced-motion support', () => {
  const drawer = source('ui/components/GenyDrawer.qml');
  assert.match(drawer, /readonly property int sheetMotionDuration: Theme\.reduceMotion \? 0 : 275/);
  assert.match(drawer, /property: "position"\s+from: 0\.0\s+to: 1\.0\s+duration: control\.sheetMotionDuration\s+easing\.type: Easing\.OutQuart/);
  assert.match(drawer, /property: "position"\s+to: 0\.0\s+duration: control\.sheetMotionDuration/);
  assert.match(drawer, /opacity: control\.position/);
  assert.doesNotMatch(drawer, /property: "y"/);
});

test('miniapp poster is uniformly inset rather than touching card edges', () => {
  const qml = source('ui/components/LinkPreview.qml');
  assert.match(qml, /anchors\.margins: root\.layoutMiniApp \? Theme\.space4\s*: root\.compact \? Theme\.space2 : Theme\.space3/);
  assert.match(qml, /id: miniAppPosterFrame[\s\S]*?clip: true/);
  assert.match(qml, /id: miniAppPoster[\s\S]*?clipToSuperellipse: true/);
  assert.match(qml, /fillMode: Image\.PreserveAspectFit/);
});

test('mobile channel toolbar does not stretch apart avatar, label and membership', () => {
  const qml = source('ui/components/DetailSurface.qml');
  const start = qml.indexOf('objectName: "mobileConversationChannelHeader"');
  const end = qml.indexOf('// One elastic region separates navigation/context', start);
  assert.ok(start > 0 && end > start);
  const toolbar = qml.slice(start, end);
  assert.match(toolbar, /Layout\.fillWidth: false/);
  assert.match(toolbar, /Layout\.maximumWidth: 142/);
  assert.match(toolbar, /Layout\.preferredWidth: Math\.min\(implicitWidth, 142\)/);
  assert.match(toolbar, /text: "\/" \+ root\.castChannel/);
  assert.match(toolbar, /root\.channelJoined/);
  const actions = qml.slice(qml.indexOf('objectName: "castActionTopDivider"'));
  assert.match(actions, /objectName: "castActionBottomDivider"/);
  assert.match(actions, /objectName: "detailDeleteCastAction"/);
  assert.match(qml, /Layout\.topMargin: root\.compact \? 0 : Theme\.space2/);
});

test('Web Mini App chrome vertically centers its close button and respects theme', () => {
  const qml = source('ui/components/ModalHeader.qml');
  const host = source('ui/wasm/miniapp-host-pre.js');
  assert.match(qml, /Layout\.alignment: Qt\.AlignVCenter/);
  assert.match(host, /dismiss\.type = "button"/);
  assert.match(host, /display:"flex",alignItems:"center",justifyContent:"center"/);
  assert.match(host, /fontSize:"25px",lineHeight:"1",width:"40px",height:"40px"/);
  assert.match(host, /app\.dismissButton = dismiss/);
  assert.match(host, /borderColor:colors\.border,color:colors\.text/);
  assert.match(host, /sheet\.animate\(\[\{opacity:0\.3\},\{opacity:1\}\]/);
});
