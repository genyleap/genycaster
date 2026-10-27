import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = name => readFileSync(new URL('../../'+name,import.meta.url),'utf8');

test('header uses physical x/y coordinates, not mirrored conditional horizontal anchors',()=>{
  const qml=read('ui/components/AppHeader.qml');
  assert.match(qml,/objectName: "appHeaderBrandCluster"[\s\S]*?x: Theme.rtl \? parent.width - root.gutter - width : root.gutter/);
  assert.match(qml,/id: brandSymbol[\s\S]*?x: Theme.rtl \? brandCluster.width - width : 0/);
  assert.match(qml,/objectName: "appHeaderWordmarkAndVersion"[\s\S]*?x: Theme.rtl \? 0 : brandSymbol.width \+ brandCluster.brandGap/);
  assert.match(qml,/objectName: "appHeaderActions"[\s\S]*?width: implicitWidth[\s\S]*?x: Theme.rtl \? root.gutter : parent.width - root.gutter - width/);
  assert.match(qml,/LayoutMirroring.childrenInherit: false/);
  assert.doesNotMatch(qml,/anchors\.(left|right): Theme.rtl \?/);
});

test('RTL brand cluster and actions cannot cross at mobile widths',()=>{
  for(const width of [320,360,390,420,550]){
    for(const rtl of [true,false]){
      const gutter=16,symbol=30,wordmark=132,gap=8,actions=80;
      const brandWidth=symbol+gap+wordmark;
      const brandX=rtl ? width-gutter-brandWidth : gutter;
      const actionsX=rtl ? gutter : width-gutter-actions;
      const symbolX=brandX+(rtl ? brandWidth-symbol : 0);
      const wordX=brandX+(rtl ? 0 : symbol+gap);
      assert.equal(Math.abs((rtl?symbolX:wordX) - (rtl?wordX+wordmark: symbolX+symbol)),gap);
      assert.ok(rtl ? actionsX+actions < brandX : brandX+brandWidth < actionsX,
                `separation width=${width} rtl=${rtl}`);
    }
  }
});

test('version sits beneath the wordmark using separate visual runs',()=>{
  const qml=read('ui/components/AppHeader.qml');
  const fa=read('ui/i18n/qml_fa_IR.ts');
  assert.match(qml,/id: localizedVersion[\s\S]*?x: parent.width - implicitWidth/);
  assert.match(qml,/id: localizedVersion[\s\S]*?y: brandWordmark.height \+ Theme.scaleUi\(2\)/);
  assert.match(qml,/layoutDirection: Qt.RightToLeft/);
  assert.match(qml,/genycaster.release.beta_version_number/);
  assert.match(fa,/<source>Beta<\/source>\s*<translation>بتا<\/translation>/);
  assert.match(fa,/<source>1\.0<\/source>\s*<translation>۱٫۰<\/translation>/);
  assert.match(qml,/objectName: "appHeaderWordmarkAndVersion"/);
});

test('all header controls retain their original actions',()=>{
  const qml=read('ui/components/AppHeader.qml');
  assert.match(qml,/visible: root.showBrand/);
  for(const handler of ['notificationsRequested','settingsRequested','ownProfileRequested'])
    assert.ok(qml.includes('root.'+handler+'()'),handler);
});
