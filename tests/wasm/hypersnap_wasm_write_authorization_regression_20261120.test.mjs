import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('HyperSnap WASM signer consent routes to browser native signer instead of unsupported C++',()=>{
    const runtime=read('ui/app/runtime/bindings.cpp');
    const start=runtime.indexOf('PresentationBridge::signerProvisionRequested');
    const end=runtime.indexOf('PresentationBridge::signerRevokeRequested',start);
    const handler=runtime.slice(start,end);
    assert.ok(start>0 && end>start);
    const body=handler.indexOf('] {');
    const nativeBranch=handler.indexOf('#if defined(Q_OS_WASM)',body);
    const wasm=handler.slice(nativeBranch,handler.indexOf('#endif',nativeBranch));
    assert.match(wasm,/bridge\.nativeCustodyAccount\(\)/);
    assert.match(wasm,/browserSigner->start\(QStringLiteral\("provision"\)/);
    assert.match(wasm,/QStringLiteral\("hypersnap-v2"\)/);
    assert.match(wasm,/QStringLiteral\("farcaster-snapchain"\)/);
    assert.doesNotMatch(wasm,/&& !bridge\.nativeCustodyAccount\(\)/);
    assert.doesNotMatch(wasm,/runtime->signerService->provision/);
    // Preserve native desktop recovery/provision implementation.
    assert.match(handler,/NativeCustodySignerAccess::forConfirmedNativeFid/);
    assert.match(handler,/runtime->signerService->provision/);
});

test('WASM signer reauthorization uses shared QML and a protocol-scoped one-shot adapter',()=>{
    const shell=read('ui/components/shell/AppShell.qml');
    const block=shell.slice(shell.indexOf('function requestInteractionAuthorization()'),
        shell.indexOf('GenyProDetailsDrawer {'));
    assert.match(block,/Qt\.platform\.os === "wasm"\s*\|\|/);
    assert.match(block,/objectName: "nativeSignerUseRecoveryWords"\s+visible: !interactionAuthorizationDialog.enteringNativeWords/);
    assert.match(block,/objectName: "nativeSignerRecoveryGrid"\s+visible: !interactionAuthorizationDialog.mobileRecoveryInput/);
    assert.match(block,/presentationBridge\.authorizeNativeHyperSnapSigner\(/);
    assert.match(block,/presentationBridge\.authorizeNativeFarcasterSigner\(/);
    const js=read('ui/wasm/protocol-signer-pre.js');
    assert.match(js,/function authorizeRecovery\(fid,native,base,words\)/);
    assert.match(js,/owner!==fid\|\|ownerProtocol!==protocol\|\|inFlight/);
    const provision=js.slice(js.indexOf('} else if (action === "provision")'),js.indexOf('} else if (action === "revoke")'));
    assert.doesNotMatch(provision,/promptForFarcasterRecovery|openRecoveryDialog/);
    assert.match(provision,/await current\.restore\(fid\)/);
    assert.match(provision,/requestedFid=>nativeCustodyForFid\(requestedFid,recoveryWords\)/);
    assert.match(provision,/current\.provisionFromRecovery\(fid,recoveryWords,stillCurrent\)/);
    const bridge=read('ui/app/browser/signer_bridge.hpp');
    assert.match(bridge,/secret\.fill\('\\0'\)/);
    assert.match(js,/vault:createBrowserSignerVault\(window,\{scope:'hypersnap-v2'\}\)/);
});

test('HyperSnap browser writes still require verified signer readiness and cannot borrow Farcaster authority',()=>{
    const runtime=read('ui/app/runtime/bindings.cpp');
    assert.match(runtime,/if \(!bridge\.isAuthenticated\(\) \|\| !bridge\.writeReady\(\)\)/);
    const js=read('ui/wasm/protocol-signer-pre.js');
    assert.match(js,/report\(\{kind:'signer',protocol:'hypersnap-v2',\.\.\.status\}\)/);
    const bridge=read('ui/app/browser/signer_bridge.hpp');
    assert.match(bridge,/if \(nativeEvent != presentation->nativeCustodyAccount\(\)\)/);
    const vault=read('ui/wasm/secure_signer_vault.mjs');
    assert.match(vault,/const keyOf = fid => scope \? scope \+ ':' \+ fid : fid/);
});
