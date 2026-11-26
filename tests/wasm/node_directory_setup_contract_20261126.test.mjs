import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = p => readFileSync(new URL('../../'+p, import.meta.url),'utf8');
test('setup preflight precedes runtime composition and preserves pending origin',()=>{
 const main=read('ui/app/main.cpp');
 assert.match(main,/hasPendingNativeRegistration/);
 assert.match(main,/nativeRegistration\/expiry-v1\//);
 assert.match(main,/nativeRegistration\/lastRuntimeOrigin/);
 assert.match(main,/automaticRegistration && !hasPendingNativeRegistration/);
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
