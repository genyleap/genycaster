import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');
const profile = read('ui/components/profile/ProfileSurface.qml');
const header = read('ui/components/shell/AppHeader.qml');
const navigation = read('ui/components/navigation/BottomNavigation.qml');
const connections = read('ui/app/identity/connections.cpp');
const connectionHeader = read('ui/app/identity/connections.hpp');
const activeDetailExpression = profile.split('function resolveActiveDetail() {')[1]
    .split('    function synchronizeActiveDetail()')[0].trim().replace(/\}\s*$/, '');
const resolveDetail = new Function('root','presentationBridge',activeDetailExpression);

test('profile identity is a one-way binding: bridge request -> detail; no retained QML feedback loop', () => {
    const key = profile.split('readonly property string profileFidText:')[1]
        .split('readonly property string activeReadNetwork:')[0];
    assert.doesNotMatch(key, /root\.activeDetail/);
    assert.match(key,/presentationBridge\.activeProfileFidText/);
    assert.doesNotMatch(activeDetailExpression,/root\.profileFidText|root\.profileNetwork|root\.retainedDetail/);
    assert.doesNotMatch(profile,/root\.retainedDetail = root\.detail/);
    assert.match(profile,/property var activeDetail: \(\{\}\)/);
    assert.doesNotMatch(profile,/readonly property var activeDetail:/);
    assert.match(profile,/function synchronizeActiveDetail\(\)/);
    assert.match(profile,/if \(signature === root\.activeDetailSignature\) return/);
    assert.match(profile,/root\.activeDetail = next/);
    assert.match(profile,/onDetailChanged: \{\s*root\.synchronizeActiveDetail\(\)/);
    assert.match(profile,/onAuthenticationStateChanged\(\) \{ Qt\.callLater\(root\.synchronizeActiveDetail\) \}/);
    const state = {
        ownProfileNavigation:false, accountFidText:'42',
        detail:{fid:12,fidString:'12',displayName:'Previous person'},
        activeReadNetwork:'farcaster',accountNetwork:'farcaster'
    };
    assert.deepEqual(resolveDetail(state,{activeProfileFidText:'42'}), {});
    state.detail = {fid:42,fidString:'42',displayName:'Current person'};
    assert.equal(resolveDetail(state,{activeProfileFidText:'42'}).displayName,'Current person');
});

test('chromatic light has a soft source-driven bottom lip in header and top rounded rim in navbar', () => {
    assert.match(header, /reflectionSource: root\.reflectionSource/);
    assert.match(header, /edgeStrength: 0\.24\s+edgeWidth: 1\.15\s+edgeInset: 0\.6\s+bottomEdgeOnly: true/);
    assert.doesNotMatch(header,/height: 1\s+color: Theme\.borderMuted/);
    assert.match(navigation,/objectName: "mobileNavigationChromaticReflection"/);
    const top = navigation.split('id: navigationTopReflectionMask')[1]
        .split('// Restrained depth')[0];
    assert.match(top,/objectName: "mobileNavigationTopRimMask"/);
    assert.match(top,/height: parent\.height \/ 2/);
    assert.match(top,/clip: true/);
    assert.match(top,/objectName: "mobileNavigationTopEdgeReflection"/);
    assert.match(top,/width: navigationCapsule\.width\s+height: navigationCapsule\.height/);
    assert.match(top,/reflectionSource: root\.reflectionSource/);
    assert.match(top,/edgeStrength: 0\.26/);
    assert.match(top,/edgeWidth: 1\.25/);
    assert.match(top,/bottomEdgeOnly: false/);
    assert.doesNotMatch(top,/NeonReflectionSource\s*\{/);
});

test('all OpenProof network response handlers guard against timeout and closed sockets', () => {
    const helper = connections.split('[[nodiscard]] QByteArray readOpenReply(')[1]
        .split('void cancelNetworkReply(')[0];
    assert.match(helper,/!reply->isOpen\(\)/);
    assert.match(helper,/!reply->isReadable\(\)/);
    assert.match(helper,/reply->bytesAvailable\(\) <= 0/);
    assert.match(helper,/networkError == QNetworkReply::TimeoutError/);
    assert.match(helper,/networkError == QNetworkReply::OperationCanceledError/);
    assert.equal((connections.match(/reply->readAll\(\)/g)||[]).length,1,
        'only the guarded reader must perform direct readAll');
    assert.match(connections,/if \(pending->isRunning\(\)\) pending->abort\(\)/);
});

test('Pro entitlement timeouts back off automatic checks without revoking a verified owner', () => {
    const fetch = connections.split('void OpenProofConnectionsController::fetchProEntitlements(')[1]
        .split('void OpenProofConnectionsController::requestProWalletChallenge(')[0];
    assert.match(fetch,/now < m_proEntitlementsRetryAfter/);
    assert.match(fetch,/networkError != QNetworkReply::NoError/);
    assert.match(fetch,/status == 429/);
    assert.match(fetch,/QDateTime::currentMSecsSinceEpoch\(\) \+ 60'000/);
    assert.match(fetch,/m_proEntitlements\.value\(QStringLiteral\("status"\)\)/);
    assert.match(fetch,/!= QStringLiteral\("verified"\)/);
    assert.match(fetch,/readOpenReply\(reply\)/);
    assert.match(connectionHeader,/qint64 m_proEntitlementsRetryAfter\{\};/);
    assert.match(connections,/m_proEntitlementsRetryAfter = 0;\s+emit proEntitlementsChanged\(\);/);
});
