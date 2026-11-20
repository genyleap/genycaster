import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');
const runtime = read('ui/app/runtime/bindings.cpp');
const bridge = read('src/presentation/qt/presentation_bridge.cpp');
const setup = read('ui/components/setup/InitialSetup.qml');
const catalog = read('ui/i18n/catalog.cpp');
const pro = read('ui/components/profile/GenyProDetailsDrawer.qml');
const streak = read('ui/components/profile/StreakCard.qml');

test('recovered Farcaster custody proves the same OpenProof identity without external approval', () => {
    const enrollment = runtime.slice(runtime.indexOf('beginNativeOpenProofEnrollment ='),
        runtime.indexOf('beginNativeWalletEnrollment ='));
    assert.match(runtime, /std::function<void\(std::uint64_t, QString, std::shared_ptr<QByteArray>\)> beginNativeOpenProofEnrollment/);
    assert.match(enrollment, /recoveredWords && !recoveredWords->isEmpty\(\)/);
    assert.match(enrollment, /signOneTimeIdentityProof\(/);
    assert.match(enrollment, /recoveredWords->fill\(char\{0\}\)/);
    assert.match(enrollment, /proof->address[\s\S]*?compare\(custody, Qt::CaseInsensitive\) != 0/);
    assert.match(enrollment, /completeRecoveryChallenge\(/);
    assert.match(runtime, /commitVerifiedIdentity\(fid, address, std::move\(permit\), true, secret\)/);
    assert.match(runtime, /beginNativeOpenProofEnrollment\(fid, custody, std::move\(recoveredWords\)\)/);
    assert.match(runtime, /connections.acceptRecoveryGrant\(/);
    assert.doesNotMatch(enrollment, /Qt\.openUrlExternally|beginFarcasterSignIn\(/);
});

test('a wrong protocol is not diagnosed as an internet outage', () => {
    const recovery = runtime.slice(runtime.indexOf('&PresentationBridge::nativeHyperSnapRecoveryRequested'),
        runtime.indexOf('#else', runtime.indexOf('&PresentationBridge::nativeHyperSnapRecoveryRequested')));
    assert.match(recovery, /LookupState::NotFound[\s\S]*?hypersnap_recovery_not_found/);
    assert.match(recovery, /LookupState::Unavailable[\s\S]*?hypersnap_recovery_unavailable/);
    assert.match(recovery, /address\.isEmpty\(\)[\s\S]*?recovery_phrase_invalid/);
    for (const id of ['hypersnap_recovery_not_found', 'hypersnap_recovery_unavailable',
        'recovery_phrase_invalid']) {
        assert.ok(catalog.includes('genycaster.setup.' + id));
    }
});

test('failed native recovery returns to an editable 24-word grid', () => {
    assert.match(bridge, /void PresentationBridge::applyAuthenticationError[\s\S]*?m_authenticationPhase = QStringLiteral\("failed"\)/);
    assert.match(setup, /if \(authenticationPhase === "failed" \|\| authenticationPhase === "signed-out"\)[\s\S]*?recoverySubmitPending = false/);
    assert.match(setup, /onAuthenticationErrorTextChanged:[\s\S]*?recoverySubmitPending = false/);
    assert.match(setup, /id: recoveryWordRepeater[\s\S]*?model: 24/);
    assert.match(setup, /enabled: !root.authenticationInProgress && !root.recoverySubmitPending/);
    assert.match(setup, /onClicked: \{\s*root.recoverySubmitPending = false\s*root.cancelAuthentication\(\)/);
});

test('Pro and Streak do not infer Farcaster ownership from a HyperSnap login', () => {
    for (const qml of [pro, streak]) {
        assert.match(qml, /readonly property bool nativeFarcasterRecoveryNeeded:/);
        assert.match(qml, /presentationBridge.accountIdentityIssuer === "farcaster:optimism-id-registry"/);
        assert.match(qml, /presentationBridge.accountIdentityIssuer === deploymentEndpoints.openProofIssuer/);
        assert.match(qml, /presentationBridge.authenticationRoute !== "farcaster-native-recovery"/);
        assert.match(qml, /!presentationBridge.nativeCustodyAccount/);
    }
    assert.match(pro, /qsTrId\("genycaster.shell.native_signer_description"\)/);
});
