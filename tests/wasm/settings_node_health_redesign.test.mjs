import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root=new URL('../../',import.meta.url);
const read=p=>readFileSync(new URL(p,root),'utf8');

test('desktop settings uses vertical categorized destinations instead of two-row tabs',()=>{
 const qml=read('ui/components/LocaleSettingsPopup.qml');
 assert.match(qml,/id: settingsNavigation/);
 assert.match(qml,/Layout\.preferredWidth: 212/);
 assert.match(qml,/visible: !root\.mobilePresentation \|\| !root\.mobileIndexOpen/);
 assert.match(qml,/root\.currentSection = Number\(modelData\.section\)/);
 assert.match(qml,/visible: Number\(modelData\.section\) !== 5 \|\| presentationBridge\.isAuthenticated/);
 assert.doesNotMatch(qml,/id: settingsTabs/);
 assert.match(qml,/id: mobileSettingsIndex/);
 assert.match(qml,/id: nodeSettingsScroll/);
});

test('node dashboard lists real per-instance API results without claiming continuous ping',()=>{
 const qml=read('ui/components/NodeManagementPanel.qml');
 const cpp=read('ui/mobile/provider_profiles_controller.cpp');
 const hpp=read('ui/mobile/provider_profiles_controller.hpp');
 for(const pattern of [/providerProfiles\.connectionRows/,/providerProfiles\.probeAllReadProviders\(\)/,
    /genycaster\.nodes\.health_explain/,/modelData\.latencyMs/,/modelData\.httpStatus/,
    /checked: providerProfiles\.compatibleReadFallback/,/readProfileCard\.showCredentials/])
   assert.match(qml,pattern);
 assert.match(hpp,/Q_PROPERTY\(QVariantList connectionRows READ connectionRows NOTIFY probeChanged\)/);
 assert.match(hpp,/Q_INVOKABLE bool probeAllReadProviders\(\)/);
 assert.match(cpp,/m_pendingBatch\.push_back/);
 assert.match(cpp,/m_batchProbing = true/);
 assert.match(cpp,/runProbeStep\(id, contract, origin, draftApiKey, 0\)/);
 assert.match(cpp,/m_probeResults\.insert/);
 assert.doesNotMatch(qml,/root\.showAdvanced/);
 assert.doesNotMatch(cpp,/ping -c/);
});

test('social graph counts are attributed as supplementary data, not claimed from raw Snapchain',()=>{
 const profile=read('ui/components/ProfileSurface.qml');
 const sidebar=read('ui/components/SocialSidebar.qml');
 const node=read('ui/components/NodeManagementPanel.qml');
 for(const content of [profile,sidebar,node]) {
    assert.match(content,/genycaster\.nodes\.counts_external/);
    assert.match(content,/\.arg\(providerProfiles\.activeReadProviderLabel\)/);
 }
 for(const loc of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR','hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
    const cat=read('ui/i18n/qml_'+loc+'.ts');
    assert.match(cat,/genycaster\.nodes\.counts_external/);
    assert.ok(cat.includes('%1'),loc+' must support active provider label');
 }
 assert.match(profile,/socialCountsKnown === true/);
 assert.match(node,/providerProfiles\.appliedCompatibleReadFallback/);
});

test('new settings and health copy is available in all supported languages',()=>{
 const keys=['raw_brief','indexed_brief','extra_data','extra_data_desc','counts_external','counts_raw',
  'health_title','health_explain','health_test_all','health_checking','health_ok','health_auth',
  'health_mismatch','health_incompatible','health_invalid','health_failed','health_untested'];
 const locales=['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR','hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
 for(const loc of locales){
  const content=read('ui/i18n/qml_'+loc+'.ts');
  for(const key of keys)assert.match(content,new RegExp('id="genycaster\\.nodes\\.'+key+'"'));
 }
});
