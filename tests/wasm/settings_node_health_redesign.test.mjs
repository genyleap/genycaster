import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root=new URL('../../',import.meta.url);
const read=p=>readFileSync(new URL(p,root),'utf8');

test('desktop settings uses vertical categorized destinations instead of two-row tabs',()=>{
 const qml=read('ui/components/settings/SettingsPopup.qml');
 assert.match(qml,/id: settingsNavigation/);
 assert.match(qml,/Layout\.preferredWidth: Math\.min\(224/);
 assert.match(qml,/visible: !root\.mobilePresentation \|\| !root\.mobileIndexOpen/);
 assert.match(qml,/root\.currentSection = Number\(modelData\.section\)/);
 assert.match(qml,/visible: Number\(modelData\.section\) !== 5 \|\| presentationBridge\.isAuthenticated/);
 assert.doesNotMatch(qml,/id: settingsTabs/);
 assert.match(qml,/id: mobileSettingsIndex/);
 assert.match(qml,/id: nodeSettingsScroll/);
});

test('node dashboard lists real per-instance API results without claiming continuous ping',()=>{
 const qml=read('ui/components/settings/NodeManagementPanel.qml');
 const cpp=read('ui/app/settings/providers.cpp');
 const hpp=read('ui/app/settings/providers.hpp');
 for(const pattern of [/providerProfiles\.connectionRows/,/providerProfiles\.probeAllReadProviders\(true\)/,
    /genycaster\.nodes\.health_short/,/modelData\.latencyMs/,/modelData\.serverLatencyMs/,
    /checked: providerProfiles\.compatibleReadFallback/,/readProfileCard\.showCredentials/])
   assert.match(qml,pattern);
 assert.match(hpp,/Q_PROPERTY\(QVariantList connectionRows READ connectionRows NOTIFY probeChanged\)/);
 assert.match(hpp,/Q_INVOKABLE bool probeAllReadProviders\(bool includeDirectory = false\)/);
 assert.match(cpp,/m_pendingBatch\.push_back/);
 assert.match(cpp,/m_batchProbing = true/);
 assert.match(cpp,/runProbeStep\(id, contract, origin, draftApiKey, 0\)/);
 assert.match(cpp,/m_probeResults\.insert/);
 assert.doesNotMatch(qml,/root\.showAdvanced/);
 assert.doesNotMatch(cpp,/ping -c/);
});

test('social graph counts are attributed as supplementary data, not claimed from raw Snapchain',()=>{
 const profile=read('ui/components/profile/ProfileSurface.qml');
 const sidebar=read('ui/components/navigation/SocialSidebar.qml');
 const node=read('ui/components/settings/NodeManagementPanel.qml');
 for(const content of [profile,sidebar,node]) {
    assert.match(content,/genycaster\.nodes\.counts_short/);
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

test('settings navigation wraps translated titles instead of truncating them',()=>{
 const nav=read('ui/components/settings/SettingsPopup.qml');
 const button=read('ui/components/controls/ActionButton.qml');
 assert.match(nav,/Layout\.preferredWidth: Math\.min\(224/);
 assert.match(nav,/component SettingsNavigationTab: Button/);
 assert.match(nav,/wrapMode: Text.Wrap/);
 assert.match(button,/property bool wrapText: false/);
 assert.match(button,/wrapMode: control\.wrapText \? Text\.Wrap : Text\.NoWrap/);
 assert.match(button,/elide: control\.wrapText \? Text\.ElideNone : Text\.ElideRight/);
 assert.doesNotMatch(nav,/id: settingsTabs/);
});
test('selected reader and measured latency remain distinct from actual API health',()=>{
 const qml=read('ui/components/settings/NodeManagementPanel.qml');
 assert.match(qml,/objectName: "publicProviderDirectory"/);
 assert.match(qml,/providerProfiles\.activeReadProviderId/);
 assert.match(qml,/Theme\.iconCheck/);
 assert.match(qml,/color: Theme\.positive/);
 assert.match(qml,/root\.measuredStatus\(/);
 assert.match(qml,/health_progress"\)\.arg\(root\.checkedNodes\)\.arg\(root\.eligibleNodes\)/);
 assert.match(qml,/root\.expandedNodeId = nodeRow\.expanded/);
 assert.match(qml,/genycaster\.nodes\.default_choice/);
 assert.match(qml,/https:\/\/genycaster\.xyz\/api\/v1\/nodes\?format=2/);
 assert.match(qml,/const directory = providerProfiles\.directoryNodes/);
 assert.match(qml,/providerProfiles\.probeDirectoryNode/);
 assert.match(qml,/String\(nodeRow.modelData.status\) === "healthy"/);
});
test('self-host provisioning is only an honest, credentials-free future preview',()=>{
 const qml=read('ui/components/settings/NodeManagementPanel.qml');
 assert.match(qml,/objectName: "serverProvisioningPreview"/);
 for(const key of ['provision_soon','provision_personal','provision_private','provision_business',
   'provision_pipeline','provision_requirements','provision_security','provision_docs'])
   assert.ok(qml.includes('genycaster.nodes.'+key),key);
 assert.match(qml,/onClicked: Qt\.openUrlExternally\("https:\/\/github\.com\/farcasterxyz\/snapchain"\)/);
 const preview=qml.slice(qml.indexOf('objectName: "serverProvisioningPreview"'),qml.indexOf('visible: providerProfiles.readRestartRequired'));
 assert.doesNotMatch(preview,/DirectionAwareTextField|requestBrowserPaste|setSessionReadApiKey|startProcess|ssh_run/);
});
test('new status and self-host guide copy is translated for all built-in locales',()=>{
 const ids=['counts_short','selected_now','default_choice','health_short','health_progress','health_passed',
  'directory_title','directory_info','directory_expand','directory_collapse','directory_source','directory_unverified',
  'provision_title','provision_soon','provision_desc','provision_personal','provision_private','provision_business',
  'provision_pipeline','provision_guide','provision_hardware','provision_requirements','provision_security','provision_docs'];
 for(const loc of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR','hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']){
  const cat=read('ui/i18n/qml_'+loc+'.ts');
  for(const id of ids)assert.ok(cat.includes('id="genycaster.nodes.'+id+'"'),loc+' '+id);
 }
});

test('configured primary has its own green outline independent of sorting and health',()=>{
 const qml=read('ui/components/settings/NodeManagementPanel.qml');
 assert.match(qml,/borderColor: nodeRow\.modelData\.active \? Theme\.positive : Theme\.accent/);
 assert.match(qml,/borderWidth: nodeRow\.modelData\.active \|\| Theme\.keyboardFocusVisible/);
 assert.match(qml,/Accessible\.selected: !!nodeRow\.modelData\.active/);
 assert.match(qml,/active: !!match && match\.id === providerProfiles\.activeReadProviderId/);
});
