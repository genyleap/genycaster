import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = path => readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('KEY_REMOVE waits for verified network revocation instead of assuming submit means removal',()=>{
 const cpp=read('src/providers/snapchain/signer.cpp');
 const revoke=cpp.slice(cpp.indexOf('[[nodiscard]] foundation::result::Status selfRevoke('),cpp.indexOf('\nprivate:',cpp.indexOf('[[nodiscard]] foundation::result::Status selfRevoke(')));
 assert.match(revoke,/confirmationDeadline/);
 assert.match(revoke,/while \(true\)/);
 assert.match(revoke,/if \(!signerActive && nonceConfirmed\) return/);
 assert.match(revoke,/ErrorCode::Timeout/);
 assert.doesNotMatch(revoke,/Snapchain accepted KEY_REMOVE but the signer remains active afterwards/);
 const contract=read('tests/providers/snapchain/contract_tests.cpp');
 assert.match(contract,/void testSelfRevokePollsUntilNetworkConfirmsRemoval\(\)/);
 assert.match(contract,/testSelfRevokePollsUntilNetworkConfirmsRemoval\(\);/);
});
test('QML avoids reading transient null data controllers during profile teardown',()=>{
 const shell=read('ui/components/AppShell.qml');
 const profile=read('ui/components/ProfileSurface.qml');
 const pulse=read('ui/components/GenyPulsePanel.qml');
 const engagement=read('ui/components/EngagementBar.qml');
 const link=read('ui/components/ProfileLinkRow.qml');
 assert.match(shell,/openProofConnections \? openProofConnections.topicCastsBusy : false/);
 assert.match(profile,/connections: openProofConnections \? openProofConnections.connections : \[\]/);
 assert.match(profile,/const server = openProofConnections \? openProofConnections.cloudProfileExtras : null/);
 assert.match(pulse,/genyPulseActivity \? genyPulseActivity.githubContributions : \(\{\}\)/);
 assert.match(engagement,/castViewController \? castViewController.entryFor/);
 assert.match(link,/const native = linkPreviewResolver \? linkPreviewResolver.previewFor/);
});
