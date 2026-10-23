import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');

test('capability router is vendor-neutral and supports per-access and operation pinning', () => {
    const router = read('src/routing/router.cpp');
    const api = read('src/routing/router.cppm');
    assert.doesNotMatch(router + api, /hypersnap|snapchain|genyleap|neynar/i);
    assert.match(api, /setPreferredProviderForAccess/);
    assert.match(api, /setPreferredProviderForCapability/);
    assert.match(router, /explicitPreference->require/);
    assert.match(router, /m_policyEngine\.isEligible\(candidate, request, m_policy/);
    assert.match(router, /std::shared_lock lock\{m_preferencesMutex\}/);
    assert.match(router, /std::unique_lock lock\{m_preferencesMutex\}/);
});

test('multiple provider instances register only for explicit supported adapter contracts', () => {
    const main = read('ui/mobile/main.cpp');
    const composition = read('ui/mobile/runtime_composition.cpp');
    const interfaces = read('ui/mobile/runtime_composition.cppm');
    assert.match(main, /GENYCASTER_READ_PROVIDER_PROFILES_JSON/);
    assert.match(main, /--read-provider-profiles-json=/);
    assert.match(main, /--preferred-read-provider-id=/);
    assert.match(main, /network\/preferredReadProviderId/);
    assert.match(main, /isValidHttpServiceUrl\(endpoint\)/);
    assert.match(main, /!seenIds\.insert\(id\.toStdString\(\)\)\.second/);
    assert.match(composition, /profile\.adapterContract != "hypersnap-v2"/);
    assert.match(composition, /No installed read adapter implements contract/);
    assert.match(composition, /runtime->additionalReadGateways\.push_back/);
    assert.match(composition, /runtime->additionalReadProviderIds\.push_back/);
    assert.match(composition, /for \(std::size_t index = 0; index < runtime->additionalReadGateways\.size\(\)/);
    assert.match(interfaces, /std::vector<ReadProviderProfile>& additionalReadProviders/);
    assert.doesNotMatch(read('src/routing/router.cpp'), /profile\.adapterContract/);
});

test('user preference maps to registered adapter instance id, not global vendor switch', () => {
    const bridge = read('ui/mobile/runtime_ui_bindings.cpp');
    const presentation = read('src/presentation/qt/presentation_bridge.cpp');
    assert.match(bridge, /runtime->independentReadProviderId/);
    assert.match(bridge, /runtime->hostedReadProviderId/);
    assert.match(bridge, /runtime->requestedReadProviderId/);
    assert.match(bridge, /ProviderPreference/);
    assert.match(presentation, /m_settings->remove\(QStringLiteral\("network\/preferredReadProviderId"\)\)/);
});
