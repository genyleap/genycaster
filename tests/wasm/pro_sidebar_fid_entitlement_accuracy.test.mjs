import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = name => readFileSync(new URL('../../'+name,import.meta.url),'utf8');

test('sidebar account card presents four factual metrics including tappable full FID',()=>{
 const qml=read('ui/components/ProfileTrustCard.qml');
 const face=qml.slice(qml.indexOf('id: frontFace'),qml.indexOf('id: backFace'));
 assert.match(face,/objectName: "sidebarFidMetric"/);
 assert.match(face,/valueText: Number\(root\.fid\) > 0\s*\? LocaleFormat\.formatInteger\(root\.fid\)/);
 assert.match(face,/labelText: "FID"/);
 assert.match(face,/interactive: true\s*onActivated: root\.requestProfile\(\)/);
 assert.match(face,/visible: !root\.compact && root\.bio\.length > 0/);
 assert.match(face,/text: root\.compact && root\.proEligible\s*\? qsTrId\("genycaster\.pro\.short"\)/);
 assert.match(face,/Layout\.minimumWidth: root\.compact \? 84 : 0/);
 assert.match(face,/wrapMode: Text\.WrapAnywhere/);
 assert.match(face,/maximumLineCount: 2\s*elide: Text\.ElideNone/);
 assert.match(qml,/text: parent\.labelText[\s\S]*?wrapMode: Text\.Wrap[\s\S]*?elide: Text\.ElideNone/);
 assert.match(qml,/objectName: "sidebarStreakRing"/);
 assert.match(qml,/onTapped: root\.toggleFace\(\)/);
 assert.match(qml,/qsTrId\("genycaster\.streak\.next"\)/);
});

test('drawer describes backend-verified Pro capabilities without using cast thread dialog strings',()=>{
 const drawer=read('ui/components/GenyProDetailsDrawer.qml');
 const policy=read('services/account-sync/pro_entitlements.mjs');
 const composer=read('ui/components/ComposerDrawer.qml');
 const settings=read('ui/components/LocaleSettingsPopup.qml');
 const chooser=read('ui/components/BookmarkCollectionChooser.qml');
 assert.match(policy,/concurrent_miniapps:1,market_pulse:4/);
 assert.match(policy,/concurrent_miniapps:4,market_pulse:16/);
 assert.match(policy,/cast_characters:4096,image_embeds:4/);
 assert.match(composer,/maxComposerCharacters: verifiedGenyPro/);
 assert.match(settings,/marketAssetLimit: marketProEligible \? 16 : 4/);
 assert.match(chooser,/enabled: root\.proEligible/);
 for(const id of [
  'feature_miniapps_title','feature_miniapps_desc',
  'feature_collections_desc','feature_connections_desc','feature_market_desc',
  'feature_composer_title','feature_composer_desc'
 ])assert.match(drawer,new RegExp('genycaster\\.pro\\.'+id));
 assert.doesNotMatch(drawer,/genycaster\.pro\.thread_explanation/);
 assert.doesNotMatch(drawer,/genycaster\.miniapp\.session_limit/);
 assert.match(drawer,/genycaster\.pro\.status_verified/);
 assert.match(drawer,/genycaster\.pro\.eligibility_summary/);
});

test('Check status rotates its refresh glyph only while the actual HTTP entitlement request is pending',()=>{
 const drawer=read('ui/components/GenyProDetailsDrawer.qml');
 const button=read('ui/components/ActionButton.qml');
 const header=read('ui/app/openproof_connections_controller.hpp');
 const cpp=read('ui/app/openproof_connections_controller.cpp');
 assert.match(header,/Q_PROPERTY\(bool proEntitlementsBusy READ proEntitlementsBusy NOTIFY proEntitlementsChanged\)/);
 assert.match(header,/proEntitlementsBusy\(\) const noexcept \{/);
 assert.match(header,/return m_proEntitlementsRead != nullptr/);
 assert.match(cpp,/m_proEntitlementsRead = reply;\s*emit proEntitlementsChanged\(\)/);
 assert.match(cpp,/m_proEntitlementsRead\.clear\(\)/);
 assert.match(drawer,/spinIconWhileBusy: true/);
 assert.match(drawer,/busy: openProofConnections \? openProofConnections\.proEntitlementsBusy : false/);
 assert.match(drawer,/enabled: !busy/);
 assert.match(drawer,/onClicked: if \(openProofConnections\) openProofConnections\.fetchProEntitlements\(true\)/);
 assert.match(button,/visible: \(!control\.busy \|\| control\.spinIconWhileBusy\)/);
 assert.match(button,/RotationAnimator on rotation/);
 assert.match(button,/running: control\.busy && control\.spinIconWhileBusy/);
 assert.match(button,/onStopped: actionGlyph\.rotation = 0/);
 assert.match(button,/if \(!control\.enabled && !control\.busy\)/);
});

test('Pro descriptions are translated and contain no unfilled %1 in feature details',()=>{
 const locales=['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR','hi_IN',
  'ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
 const keys=['status_verified','status_unavailable','eligibility_summary',
  'feature_miniapps_title','feature_miniapps_desc','feature_collections_desc',
  'feature_connections_desc','feature_market_desc','feature_composer_title',
  'feature_composer_desc'];
 for(const lang of locales){
  const xml=read('ui/i18n/qml_'+lang+'.ts');
  for(const key of keys){
   const needle='id="genycaster.pro.'+key+'"';
   assert.equal(xml.split(needle).length-1,1,lang+':'+key);
  }
  assert.match(xml,/<source>Pro supports up to 4 Mini Apps at once \(Basic: 1\)\./);
  assert.match(xml,/<source>Track up to 16 Market Pulse tokens \(Basic: 4\)\./);
  const featureSection=xml.slice(xml.indexOf('id="genycaster.pro.feature_composer_desc"'),
   xml.indexOf('id="genycaster.pro.thread_title"'));
  assert.doesNotMatch(featureSection,/%1/);
 }
});
