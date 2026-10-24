import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');

test('provider-neutral canonical writer routes without vendor names in core', () => {
    const api = read('src/routing/write_gateway.cppm');
    const impl = read('src/routing/write_gateway.cpp');
    assert.doesNotMatch(api + impl, /hypersnap|snapchain|genyleap|neynar/i);
    assert.match(impl, /capability::catalog::protocolMessageSubmit\(\)/);
    assert.match(impl, /m_bindings\.bindOnce\(operation\.idempotencyKey, std::move\(binding\)\)/);
    assert.match(impl, /Pending canonical write has no durable provider binding/);
    assert.match(impl, /m_router\.resolve\(policy::CapabilityRequest/);
    assert.match(impl, /m_router\.isEligibleProvider\(/);
    assert.match(impl, /\(\*source\)->reconcile\(operation\)/);
});

test('multiple writers are explicit compatible adapter instances, pinned per operation', () => {
    const main = read('ui/mobile/main.cpp');
    const composition = read('ui/mobile/runtime_composition.cpp');
    const api = read('ui/mobile/runtime_composition.cppm');
    assert.match(main, /GENYCASTER_WRITE_PROVIDER_PROFILES_JSON/);
    assert.match(main, /GENYCASTER_PREFERRED_WRITE_PROVIDER_ID/);
    assert.match(main, /--write-provider-profiles-json=/);
    assert.match(main, /--preferred-write-provider-id=/);
    assert.match(main, /network\/writeProviderProfilesJson/);
    assert.match(main, /network\/preferredWriteProviderId/);
    assert.match(composition, /profile\.adapterContract != "snapchain-v1"/);
    assert.match(composition, /No installed write adapter implements contract/);
    assert.match(composition, /runtime->writeSources\.push_back/);
    assert.match(composition, /openWriteRouteBindings\(databasePath\)/);
    assert.match(composition, /make_unique<routing::write::RoutedWriteSubmitter>/);
    assert.match(composition, /setPreferredProviderForAccess\(\s*capability::model::CapabilityAccess::Write/);
    assert.match(api, /std::vector<WriteProviderProfile>& additionalWriteProviders/);
});

test('binding persistence is a SQLite infrastructure concern, never domain data', () => {
    const storage = read('src/storage/write.cpp');
    const local = read('src/local_state/write.cppm');
    const domain = read('src/domain/write.cppm');
    assert.match(storage, /CREATE TABLE IF NOT EXISTS write_route_bindings/);
    assert.match(storage, /FOREIGN KEY \(idempotency_key\) REFERENCES write_outbox/);
    assert.match(storage, /endpoint_identity TEXT NOT NULL/);
    assert.match(read('src/routing/write_gateway.cpp'), /source.endpointIdentity == binding.endpointIdentity/);
    assert.match(local, /class IWriteRouteBindings/);
    assert.doesNotMatch(domain, /write_route_bindings|providerId|provider_id/);
});
