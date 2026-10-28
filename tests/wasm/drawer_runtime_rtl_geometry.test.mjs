import test from 'node:test';
import assert from 'node:assert/strict';
import {existsSync, readFileSync, mkdtempSync, rmSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {spawnSync} from 'node:child_process';

const source=readFileSync('ui/components/GenyDrawer.qml','utf8');
const match=source.match(/^\s*edge:\s*(bottomSheet\s*\?[^\n]+)$/m);
assert.ok(match,'real GenyDrawer edge binding must exist');
const expression=match[1].replaceAll('bottomSheet','drawer.bottomSheet').replaceAll('Theme.rtl','root.rtl');

// Run the exact GenyDrawer edge expression in the native Qt 6 Quick runtime.
// It verifies actual rendered x/y, not just a regex or an RTL assumption.
const qmlBin=process.env.GENYCASTER_QML_BIN ||
  '/opt/genycaster/Qt/6.12.0/gcc_64/bin/qml';
test('Qt Drawer actually opens LEFT in RTL, RIGHT in LTR, BOTTOM on mobile',
  {timeout:15000, skip:!existsSync(qmlBin)},()=>{
  const dir=mkdtempSync(join(tmpdir(),'genycaster-drawer-direction-'));
  try {
    const qml=`import QtQuick
import QtQuick.Controls
ApplicationWindow {
  id: root
  width: 960
  height: 640
  visible: true
  property bool rtl: false
  LayoutMirroring.enabled: false
  Drawer {
    id: drawer
    property bool bottomSheet: false
    parent: Overlay.overlay
    width: bottomSheet ? parent.width : 340
    height: bottomSheet ? 200 : parent.height
    edge: ${expression}
    enter: Transition { NumberAnimation { property: "position"; to: 1; duration: 0 } }
    exit: Transition { NumberAnimation { property: "position"; to: 0; duration: 0 } }
  }
  property int stage: 0
  Timer {
    running: true
    repeat: true
    interval: 150
    onTriggered: {
      if(root.stage===0) drawer.open()
      if(root.stage===1) {
        console.log("DIRECTION_LTR", drawer.edge, drawer.x, drawer.y, drawer.width, drawer.position)
        drawer.close()
        root.rtl=true
      }
      if(root.stage===2) drawer.open()
      if(root.stage===3) {
        console.log("DIRECTION_RTL", drawer.edge, drawer.x, drawer.y, drawer.width, drawer.position)
        drawer.close()
        drawer.bottomSheet=true
      }
      if(root.stage===4) drawer.open()
      if(root.stage===5) {
        console.log("DIRECTION_MOBILE", drawer.edge, drawer.x, drawer.y, drawer.width, drawer.position)
        Qt.quit()
      }
      root.stage++
    }
  }
}`;
    const file=join(dir,'drawer.qml');
    writeFileSync(file,qml);
    const result=spawnSync(qmlBin,[file],{
      encoding:'utf8',timeout:12000,
      env:{...process.env,QT_QPA_PLATFORM:'offscreen',QT_QUICK_BACKEND:'software'}
    });
    assert.equal(result.status,0,`Qt did not execute: ${result.stderr}\n${result.stdout}`);
    const output=result.stderr+'\n'+result.stdout;
    assert.match(output,/DIRECTION_LTR 4 620 0 340 1/);
    assert.match(output,/DIRECTION_RTL 2 0 0 340 1/);
    assert.match(output,/DIRECTION_MOBILE 8 0 440 960 1/);
  } finally {rmSync(dir,{recursive:true,force:true});}
});
