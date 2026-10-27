import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = name => readFileSync(new URL('../../'+name,import.meta.url),'utf8');

test('RTL header keeps symbol immediately beside wordmark, without fill-width brand layouts',()=>{
    const qml=read('ui/components/AppHeader.qml');
    assert.match(qml,/id: brandCluster[\s\S]*?width: symbolSize \+ brandGap \+ wordmarkWidth/);
    assert.match(qml,/id: brandSymbol[\s\S]*?anchors\.right: Theme\.rtl \? parent\.right : undefined/);
    assert.match(qml,/id: brandText[\s\S]*?anchors\.right: Theme\.rtl \? brandSymbol\.left : undefined/);
    assert.match(qml,/anchors\.rightMargin: Theme\.rtl \? brandCluster\.brandGap : 0/);
    assert.match(qml,/id: headerActions[\s\S]*?anchors\.left: Theme\.rtl \? parent\.left : undefined/);
    assert.doesNotMatch(qml,/RowLayout \{\s*anchors\.fill: parent/);
    assert.match(qml,/LayoutMirroring\.childrenInherit: true/);
});

test('beta caption has bounded separate RTL text runs and does not let digit order reflow',()=>{
    const qml=read('ui/components/AppHeader.qml');
    const locale=read('ui/i18n/qml_fa_IR.ts');
    assert.match(qml,/id: localizedVersion[\s\S]*?layoutDirection: Qt\.RightToLeft/);
    assert.match(qml,/genycaster\.release\.beta_version_number/);
    assert.match(locale,/<source>Beta<\/source>\s*<translation>بتا<\/translation>/);
    assert.match(locale,/<source>1\.0<\/source>\s*<translation>۱٫۰<\/translation>/);
    assert.match(qml,/anchors\.top: brandWordmark\.bottom/);
    assert.match(qml,/objectName: "appHeaderWordmarkAndVersion"/);
});

test('header action group stays on physical left for RTL and is unaffected by brand visibility',()=>{
    const qml=read('ui/components/AppHeader.qml');
    assert.match(qml,/id: headerActions[\s\S]*?anchors\.left: Theme\.rtl \? parent\.left : undefined/);
    assert.match(qml,/visible: root\.showBrand/);
    for(const handler of ['notificationsRequested','settingsRequested','ownProfileRequested'])
        assert.ok(qml.includes('root.'+handler+'()'),handler);
});
