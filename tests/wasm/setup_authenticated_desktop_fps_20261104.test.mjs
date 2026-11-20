import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = f => readFileSync(new URL('../../' + f, import.meta.url), 'utf8');

test('verified sign-in is mandatory on all platforms including WebAssembly', () => {
 const cpp=read('src/presentation/qt/presentation_bridge.cpp');
 const qml=read('ui/components/setup/InitialSetup.qml');
 const bridge=cpp.slice(cpp.indexOf('void PresentationBridge::completeInitialSetup()'),cpp.indexOf('void PresentationBridge::resetInitialSetupProgress()'));
 assert.match(cpp,/return !m_setupBypassedForVerification\s*&& \(!m_isAuthenticated \|\| !m_initialSetupCompleted \|\| m_showSetupAfterSignOut\)/);
 assert.match(bridge,/if \(!m_isAuthenticated \|\| !m_legalConsentAccepted/);
 assert.doesNotMatch(bridge,/#if defined\(__EMSCRIPTEN__\)/);
 assert.doesNotMatch(qml,/guestSelected|continue_as_guest/);
 assert.match(qml,/if \(!reviewMode && !authenticated && target > 5\)/);
 assert.match(qml,/if \(currentStep === 7\) return reviewMode \|\| \(legalConsentAccepted\s*&& authenticated && \(accountIntent !== "new" \|\| signerReady\)\)/);
 assert.match(read('ui/components/shell/AppShell.qml'),/visible: root.setupReviewOpen \|\| \(presentationBridge.authenticationStateObserved\s*&& presentationBridge.initialSetupRequired\)/);
});
test('native desktop starts wide enough for side widgets when display permits', () => {
 const qml=read('ui/components/shell/Main.qml');
 assert.match(qml,/Math\.min\(1560, Screen\.desktopAvailableWidth/);
 assert.match(qml,/Math\.min\(940, Screen\.desktopAvailableHeight/);
 assert.match(qml,/minimumWidth: nativeDesktop \? 360 : 0/);
});
test('WASM rendering settings identify WebGL and do not mislabel sparse redraws as target FPS', () => {
 const qml=read('ui/components/settings/RenderingPopup.qml');
 const controller=read('src/presentation/qt/rendering_controller.cpp');
 assert.match(qml,/qsTrId\("genycaster\.rendering\.frame_target"\)/);
 assert.match(qml,/Math\.round\(renderingController\.refreshRate\)/);
 assert.doesNotMatch(qml,/Math\.round\(1000 \/ renderingController\.frameIntervalMs\)/);
 assert.match(qml,/ \+ " FPS"/);
 assert.match(controller,/#if defined\(Q_OS_WASM\)[\s\S]*?QStringLiteral\("WebGL \(OpenGL ES\)"\)/);
 assert.match(controller,/m_frameClock\.restart\(\)/);
 for(const lang of ['en_US','fa_IR']) {
  const ts=read('ui/i18n/qml_'+lang+'.ts');
  assert.match(ts,/<message id="genycaster\.rendering\.frame_target">/);
 }
});
