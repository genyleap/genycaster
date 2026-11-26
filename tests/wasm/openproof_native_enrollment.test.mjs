import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync, readdirSync } from 'node:fs';

const read = path => readFileSync(path, 'utf8');
const runtime = read('ui/app/runtime/bindings.cpp');
const wallet = read('ui/app/identity/native_wallet.cpp');
const walletApi = read('ui/app/identity/native_wallet.hpp');
const signer = read('src/platform/custody_recovery.cpp');
const capability = read('src/providers/openproof_auth/auth.cpp');
const setup = read('ui/components/setup/InitialSetup.qml');
const sessions = read('src/application/session.cpp');

test('new Farcaster FID uses the original trusted OPP proof and never repeats protocol registration', () => {
    assert.match(runtime, /NeynarRegistrationController::farcasterFidConfirmed/);
    assert.match(runtime, /commitVerifiedIdentity\(\s*static_cast<std::uint64_t>\(fid\), custody,\s*std::move\(permit\), true\)/);
    assert.match(runtime, /beginNativeOpenProofEnrollment/);
    assert.match(runtime, /startRecoveryChallenge\(\*runtime->transport/);
    assert.match(runtime, /completeRecoveryChallenge\(/);
    assert.match(runtime, /signOpenProofFarcasterChallenge\(/);
    const enrollment=runtime.slice(runtime.indexOf('beginNativeOpenProofEnrollment ='),runtime.indexOf('beginNativeWalletEnrollment ='));
    assert.doesNotMatch(enrollment,/acceptVerifiedRecovery\(|sessionObserver\(/);
    assert.match(enrollment,/connections\.activeAccountFid\(\) != fid/);
    assert.match(enrollment,/connections\.acceptRecoveryGrant\(/);
    assert.match(runtime,/acceptVerifiedRecovery\(/); // explicit OpenProof recovery remains supported
    assert.match(runtime, /genycaster::deployment::openProofIssuer/);
    assert.match(wallet, /custody::signOneTimeIdentityProof\(/);
    assert.doesNotMatch(walletApi, /Q_INVOKABLE.*signOpenProof/);
});

test('HyperSnap uses SIWE wallet self-provisioning without claiming native FID linking', () => {
    assert.match(runtime, /HyperSnapRegistrationPublisher::nativeAccountConfirmed/);
    assert.match(runtime, /beginNativeWalletEnrollment/);
    assert.match(runtime, /beginWalletEnrollment\(\*runtime->transport/);
    assert.match(runtime, /signOpenProofWalletChallenge\(/);
    assert.match(capability, /"\/auth\/web3\/start"/);
    assert.match(capability, /"\/auth\/web3\/complete"/);
    assert.match(capability, /"\/account\/profile"/);
    assert.match(capability, /"\/account\/connections"/);
    assert.match(capability, /"ethereum-wallet"/);
    assert.match(runtime, /setNativeEnrollmentPhase\(\s*fid, QStringLiteral\("wallet-linked"\)\)/);
    assert.match(runtime, /active->identityIssuer == "hypersnap:native-registry"/);
    assert.doesNotMatch(capability, /"provider":"hypersnap"/);
    assert.match(sessions, /"hypersnap:native-registry"/);
    assert.match(signer, /signOpenProofWalletChallenge\(/);
    assert.match(signer, /Sign in to OpenProof with your Ethereum account/);
    assert.match(signer, /Chain ID: 1/);
    assert.match(signer, /Ethereum Signed Message/);
});

test('C++ keeps sensitive custody keys out of QML and blocks Qt GUI thread KDF', () => {
    assert.match(wallet, /QtConcurrent::run\(/);
    assert.match(wallet, /auto erase = \[\]\(QByteArray\* bytes\)/);
    assert.match(wallet, /bytes->fill\(char\{0\}\)/);
    assert.match(wallet, /privateWords = std::move\(privateWords\)/);
    assert.match(wallet, /std::string_view\{privateWords->constData\(\)/);
    assert.match(walletApi, /QFuture<std::optional<QString>> signOpenProofFarcasterChallenge/);
    assert.match(walletApi, /QFuture<std::optional<QString>> signOpenProofWalletChallenge/);
    assert.match(runtime, /QFutureWatcher<void>/);
    assert.doesNotMatch(setup, /signOpenProofWalletChallenge|signOneTimeIdentityProof|recoveryWords.*fetch/);
});

test('outage and cancellation preserve verified network FID and support safe retry', () => {
    assert.match(runtime, /enrollmentGeneration->fetch_add\(1U, std::memory_order_acq_rel\)/);
    assert.match(runtime, /PresentationBridge::signOutRequested/);
    assert.match(runtime, /active->fid.value != fid/);
    assert.match(runtime, /!bridge.isAuthenticated\(\) \|\| bridge.accountFid\(\) != fid/);
    assert.match(runtime, /setNativeEnrollmentPhase\(fid, QStringLiteral\("retry"\)\)/);
    assert.match(runtime, /nativeEnrollmentRetryRequested/);
    assert.match(setup, /objectName: "setupRetryCentralAccount"/);
    assert.match(setup, /openProofConnections.retryNativeEnrollment\(\)/);
    assert.match(setup, /genycaster.setup.opp_wallet_linked/);
    assert.match(setup, /genycaster.setup.opp_linked/);
});

test('all 12 locales expose separate Farcaster identity and HyperSnap wallet statuses', () => {
    for(const filename of readdirSync('ui/i18n').filter(name => /^qml_.*\.ts$/.test(name))) {
        const xml = read('ui/i18n/' + filename);
        for(const id of ['opp_pending','opp_linked','opp_retry','opp_retry_action',
                          'opp_wallet_pending','opp_wallet_linked','opp_wallet_retry']) {
            assert.match(xml, new RegExp('id="genycaster\\.setup\\.'+id+'"'), filename+':'+id);
        }
    }
});
