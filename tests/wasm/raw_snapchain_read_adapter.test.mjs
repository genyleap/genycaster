import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = file => readFileSync(new URL(file, root), 'utf8');

test('raw-node adapter declares only its directly implemented read capabilities', () => {
    const adapter = read('src/providers/snapchain/read.cpp');
    const declarations = adapter.slice(adapter.indexOf('std::vector<capability::model::CapabilityDescriptor> capabilities()'));
    assert.match(declarations, /identityFidResolve\(\)/);
    assert.match(declarations, /profileRead\(\)/);
    assert.match(declarations, /feedUserPostsRead\(\)/);
    assert.match(declarations, /feedUserQuotesRead\(\)/);
    assert.match(declarations, /feedUserRepliesRead\(\)/);
    assert.match(declarations, /boundedDerived\(capability::catalog::feedFollowingRead\(\)\)/);
    assert.doesNotMatch(declarations, /feedTrendingRead\(\)|searchRead\(\)|notificationsRead\(\)|castRead\(\)/);
    assert.match(adapter, /Raw Snapchain cannot satisfy/);
});

test('raw-node reads never guess that indexed-v2 API implements raw protocol', () => {
    const composition = read('ui/app/runtime/composition.cpp');
    const source = read('src/providers/snapchain/read.cpp');
    assert.match(composition, /profile\.adapterContract == "snapchain-v1"/);
    assert.match(composition, /snapchain::read::capabilities\(\)/);
    assert.match(composition, /snapchain::read::makeGateway/);
    assert.match(composition, /runtime->rawReadGateways\.push_back/);
    assert.match(composition, /runtime->rawReadProviderIds\.push_back/);
    assert.match(source, /\/v1\/castsByFid\?fid=/);
    assert.match(source, /\/v1\/userDataByFid\?fid=/);
    assert.match(source, /\/v1\/userNameProofByName\?name=/);
    assert.match(read('ui/CMakeLists.txt'), /FARCASTER_HAS_SNAPCHAIN_READ=1/);
});

test('raw-node continuation stays provider, capability, endpoint and FID scoped and private', () => {
    const source = read('src/providers/snapchain/read.cpp');
    const domain = read('src/domain/cast.cppm');
    assert.match(source, /record->providerId\.value != m_config\.providerId\.value/);
    assert.match(source, /record->capabilityId\.value != capabilityId\.value/);
    assert.match(source, /Raw-node continuation has a different endpoint or user FID/);
    assert.match(source, /"raw-v2:" \+ m_config\.baseUrl/);
    assert.match(source, /std::to_string\(fid\.value\) \+ "\\n" \+ \*cursor/);
    assert.match(source, /"&reverse=true"/);
    assert.doesNotMatch(domain, /providerCursor|pageToken/);
    assert.match(source, /WeightedThreadPoolExecutor/);
});

test('WASM portable Boost dependency avoids native Boost package lookup', () => {
    const cmake = read('src/providers/snapchain/CMakeLists.txt');
    assert.match(cmake, /if\(EMSCRIPTEN\)\s+[^]*?genycaster_wasm_setup_boost_headers\(\)/);
});
