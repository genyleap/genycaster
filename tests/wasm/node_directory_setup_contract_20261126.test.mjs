import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = p => readFileSync(new URL('../../'+p, import.meta.url),'utf8');
test('setup preflight precedes runtime composition and preserves pending origin',()=>{
 const main=read('ui/app/main.cpp');
 assert.match(main,/hasPendingNativeRegistration/);
 assert.match(main,/nativeRegistration\/expiry-v1\//);
 assert.match(main,/nativeRegistration\/lastRuntimeOrigin/);
 assert.match(main,/if \(hasPendingNativeRegistration\) \{ launch\(nativeRegistrationEndpoint\); return; \}/);
 assert.match(main,/ProviderProfilesController::directoryRefreshFinished/);
 assert.match(main,/NativeRegistrationStatusController::selectBeforeRuntime/);
 assert.match(main,/nativeRegistrationPublisher.setEndpointBeforeRuntime\(candidate\)/);
 assert.match(main,/startupOptions.hyperSnapNativeNodeBaseUrl = selectedNativeOrigin.toStdString\(\)/);
});
test('public node fallback is read-only, not a custom or capability-specific pin',()=>{
 const router=read('src/routing/router.cpp');
 assert.match(router,/sameNetworkPool = request.requiredAccess == capability::model::CapabilityAccess::Read/);
 assert.match(router,/!specificPreference && explicitPreference && explicitPreference->require/);
 assert.match(router,/publicReadAlternatives.cbegin\(\)/);
 const main=read('ui/app/main.cpp');
 assert.match(main,/configuredProfiles->readProviderApiKeys.empty\(\)/);
 assert.match(main,/network\/nodeSelectionMode/);
});

test('setup and settings consume the same directory and label the source',()=>{
 const setup=read('ui/components/setup/InitialSetup.qml');
 const settings=read('ui/components/settings/NodeManagementPanel.qml');
 const controller=read('ui/app/settings/providers.cpp');
 for(const qml of [setup,settings]){
   assert.match(qml,/providerProfiles\.directoryNodes/);
   assert.match(qml,/providerProfiles\.directorySource/);
   assert.match(qml,/providerProfiles\.refreshDirectory\(\)/);
 }
 assert.match(setup,/providerProfiles\.registrationNode/);
 assert.match(setup,/modelData\.registrationPreflight/);
 assert.match(controller,/EVP_DigestVerify\(/);
 assert.match(controller,/crypto\.subtle\.verify\("Ed25519"/);
 assert.match(controller,/verifyDirectory\(m_settings\.value\(settingKey\(kDirectoryCache\)\)\.toByteArray\(\), true\)/);
 assert.match(controller,/m_directoryNodes\.clear\(\)/);
});

test('public node inventory comes from signed API, not a compiled duplicate',()=>{
 const main=read('ui/app/main.cpp');
 const controller=read('ui/app/settings/providers.cpp');
 const qml=read('ui/components/settings/NodeManagementPanel.qml');
 for(const code of [main,controller,qml]){
   assert.doesNotMatch(code,/hypersnap\.(?:castrater|imgnai)\.|hs1\.qbase\.tech|sizzle\.onchain\.cooking/);
 }
 assert.match(controller,/registrationAllowed/);
 assert.match(qml,/columns: root\.narrowPanel \? 1 : 2/);
 assert.match(qml,/wrapMode: Text\.WrapAnywhere/);
 assert.match(qml,/providerProfiles\.probeAllReadProviders\(true\)/);
});

test('compact rows sort by effective health and measured latency with unknowns last',()=>{
 const qml=read('ui/components/settings/NodeManagementPanel.qml');
 const functions=qml.slice(qml.indexOf('    function observation(node)'),qml.indexOf('    readonly property int eligibleNodes'));
 const root={};
 new Function('root',functions+';Object.assign(root,{observation,nodeRank,compareNodes});')(root);
 const row=(label,status,latencyMs,serverStatus,serverLatencyMs,enabled=true)=>({id:label,label,status,latencyMs,serverStatus,serverLatencyMs,enabled});
 const rows=[
  row('offline','failed',10,'healthy',1),
  row('server','idle',-1,'healthy',120),
  row('local slow','healthy',350,'healthy',2),
  row('local fast','healthy',60,'healthy',500),
  row('unknown','idle',-1,'unknown',-1),
  row('disabled','healthy',1,'healthy',1,false),
  row('lagging','idle',-1,'degraded',10)
 ];
 assert.deepEqual(rows.sort(root.compareNodes).map(n=>n.label),
    ['local fast','server','local slow','lagging','unknown','offline','disabled']);
 assert.equal(root.observation(rows[0]).local,true);
 assert.equal(root.observation(rows[1]).local,false);
 assert.match(qml,/model: root.liveHealthRows/);
 assert.match(qml,/visible: nodeRow.expanded/);
 assert.match(qml,/ItemDelegate \{/);
});

test('WASM startup registry probes use cancellable browser fetch and bounded Qt polling',()=>{
 const cpp=read('ui/app/identity/native_registration.cpp');
 const bridge=read('ui/wasm/protocol-signer-pre.js');
 assert.match(cpp,/#if defined\(Q_OS_WASM\)[\s\S]*?__genycasterRegistryPreflight\?\.start/);
 assert.match(cpp,/elapsed->elapsed\(\) < deadlineMs/);
 assert.match(cpp,/__genycasterRegistryPreflight\?\.cancel/);
 assert.match(bridge,/preflightBrowserNativeRegistries/);
 assert.match(bridge,/entry\?\.controller.abort\(\)/);
});
