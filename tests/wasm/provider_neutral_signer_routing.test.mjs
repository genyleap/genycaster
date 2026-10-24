import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = file => readFileSync(new URL(file, root), 'utf8');

test('signer operations route through capability-neutral access contract', () => {
    const source = read('src/routing/signer_gateway.cpp');
    const api = read('src/routing/signer_gateway.cppm');
    assert.doesNotMatch(source + api, /snapchain|hypersnap|neynar|genyleap/i);
    assert.match(source, /protocolSignerManage\(\)/);
    assert.match(source, /CapabilityAccess::Write/);
    assert.match(source, /m_router->resolve\(policy::CapabilityRequest/);
    assert.match(source, /\(\*source\)->registerGasless\(/);
    assert.match(source, /\(\*source\)->selfRevoke\(/);
    assert.match(source, /\(\*source\)->inspect\(/);
    assert.match(source, /no registered adapter/);
});

test('write adapter registrations explicitly declare signer capability', () => {
    const composition = read('ui/mobile/runtime_composition.cpp');
    assert.match(composition, /capability::catalog::protocolSignerManage\(\)/);
    assert.match(composition, /runtime->signerRegistrySources\.push_back/);
    assert.match(composition, /runtime->signerRegistry->addSource/);
    assert.match(composition, /runtime->signerRegistry->bindRouter\(\*runtime->router\)/);
    assert.match(composition, /signerConfig\.submitBaseUrl = profile\.endpoint/);
    assert.match(composition, /signerConfig\.baseUrl = profile\.readEndpoint/);
    assert.match(composition, /\.providerId = farcaster::provider::model::ProviderId\{profile\.id\}/);
    assert.match(read('src/capabilities/catalog.cpp'), /protocol\.signer\.manage/);
});

test('signer management remains an injected port in the application', () => {
    const app = read('src/application/signer.cpp');
    assert.doesNotMatch(app, /import farcaster\.provider\./);
    assert.match(read('ui/mobile/runtime_composition.cppm'), /RoutedSignerRegistry/);
    assert.match(read('src/capabilities/signer.cppm'), /class ISignerRegistry/);
});
